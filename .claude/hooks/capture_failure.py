#!/usr/bin/env python3
"""PostToolUse hook (Bash): journal build/test/sanitizer failures.

Feeds the self-evolution loop: failed cmake/ctest/ninja/compiler commands are
appended to evolution/journal.ndjson for later clustering by /evolve.
Never blocks the agent: always exits 0.
"""
import json
import os
import re
import sys
import time

BUILD_CMD = re.compile(r"\b(cmake|ctest|ninja|make|clang\+\+|clang|clang-tidy|g\+\+|gcc)\b")

EXCERPT_LIMIT = 2000


def journal_path() -> str:
    root = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    return os.path.join(root, "evolution", "journal.ndjson")


def append_entry(entry: dict) -> None:
    line = (json.dumps(entry, ensure_ascii=False) + "\n").encode("utf-8")
    fd = os.open(journal_path(), os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o644)
    try:
        os.write(fd, line)
    finally:
        os.close(fd)


def classify(text: str) -> str | None:
    if "AddressSanitizer" in text or "LeakSanitizer" in text or "runtime error:" in text:
        return "sanitizer_failure"
    failed_match = re.search(r"tests passed, (\d+) tests? failed", text)
    if (
        "Errors while running CTest" in text
        or "[  FAILED  ]" in text
        or (failed_match and int(failed_match.group(1)) > 0)
    ):
        return "test_failure"
    if "error:" in text or "CMake Error" in text or re.search(r"\bFAILED\b", text):
        return "build_failure"
    return None


def main() -> None:
    try:
        data = json.load(sys.stdin)
    except Exception:
        return
    cmd = str((data.get("tool_input") or {}).get("command") or "")
    if not BUILD_CMD.search(cmd):
        return
    # tool_response schema varies across versions: scan the whole serialization.
    response = json.dumps(data.get("tool_response", ""), default=str, ensure_ascii=False)
    kind = classify(response)
    if kind is None:
        return
    append_entry(
        {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "type": kind,
            "session_id": str(data.get("session_id") or ""),
            "cwd": str(data.get("cwd") or ""),
            "cmd": cmd[:300],
            "excerpt": response[-EXCERPT_LIMIT:],
            "tags": [],
        }
    )


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
