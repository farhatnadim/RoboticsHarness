#!/usr/bin/env python3
"""PostToolUse hook (Bash): journal build/test/sanitizer failures.

Feeds the self-evolution loop: failed cmake/ctest/ninja/compiler commands are
appended to evolution/journal.ndjson for later clustering by /evolve.
Never blocks the agent: always exits 0.
"""
import json
import re
import sys
import time

from _journal import append_entry

# A build verb must appear at the start of the command or right after a shell
# connector — not merely anywhere in the string. Heredoc bodies, commit
# messages, and echoed text mentioning "cmake" used to trigger false captures
# (see journal entries of 2026-07-03).
BUILD_CMD = re.compile(
    r"(?:^|&&|\|\||[;|\n])\s*"
    r"(?:cmake|ctest|ninja|make|clang\+\+|clang-tidy|clang|g\+\+|gcc)\b"
)

CONTEXT_BEFORE = 500
CONTEXT_AFTER = 1500


def strip_heredocs(cmd: str) -> str:
    # Drop heredoc bodies so their content can't look like a build command.
    return re.sub(r"<<-?\s*'?(\w+)'?.*?\n\1\s*$", "<<HEREDOC>", cmd, flags=re.S | re.M)


SANITIZER_MARKS = ("AddressSanitizer", "LeakSanitizer", "runtime error:")
TEST_MARKS = ("Errors while running CTest", "[  FAILED  ]")
BUILD_MARKS = ("error:", "CMake Error", "ninja: build stopped", "subcommand failed")
FAILED_COUNT = re.compile(r"tests passed, (\d+) tests? failed")


def classify(text: str) -> tuple[str, int] | None:
    """Return (kind, position of the first matched marker), or None."""
    for mark in SANITIZER_MARKS:
        pos = text.find(mark)
        if pos != -1:
            return "sanitizer_failure", pos
    for mark in TEST_MARKS:
        pos = text.find(mark)
        if pos != -1:
            return "test_failure", pos
    count = FAILED_COUNT.search(text)
    if count and int(count.group(1)) > 0:
        return "test_failure", count.start()
    for mark in BUILD_MARKS:
        pos = text.find(mark)
        if pos != -1:
            return "build_failure", pos
    ninja_failed = re.search(r"\bFAILED\b", text)
    if ninja_failed is not None:
        return "build_failure", ninja_failed.start()
    return None


def main() -> None:
    try:
        data = json.load(sys.stdin)
    except Exception:
        return
    cmd = str((data.get("tool_input") or {}).get("command") or "")
    # Never capture commands that read or write the journal itself: their
    # output quotes old entries, which re-match the failure markers.
    if "journal.ndjson" in cmd:
        return
    if not BUILD_CMD.search(strip_heredocs(cmd)):
        return
    # Prefer decoded stdout/stderr; the serialized fallback covers other
    # tool_response schemas.
    response = data.get("tool_response")
    if isinstance(response, dict) and ("stdout" in response or "stderr" in response):
        text = str(response.get("stdout") or "") + "\n" + str(response.get("stderr") or "")
    else:
        text = json.dumps(response, default=str, ensure_ascii=False)
    hit = classify(text)
    if hit is None:
        return
    kind, pos = hit
    # Excerpt a window around the first marker: a blind tail often kept only
    # trailing passed-test noise and truncated the actual error away.
    start = max(0, pos - CONTEXT_BEFORE)
    append_entry(
        {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "type": kind,
            "session_id": str(data.get("session_id") or ""),
            "cwd": str(data.get("cwd") or ""),
            "cmd": cmd[:300],
            "excerpt": text[start : pos + CONTEXT_AFTER],
            "tags": [],
        }
    )


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
