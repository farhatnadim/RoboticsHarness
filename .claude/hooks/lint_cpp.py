#!/usr/bin/env python3
"""PostToolUse hook (Edit|Write|MultiEdit): clang-tidy any C++ file the agent touched.

Unlike format_cpp.py this hook talks back: when clang-tidy reports findings in
the edited file it exits 2 so the agent sees them on stderr and fixes (or
NOLINT-justifies) them. Every failure mode of the hook itself — clang-tidy not
installed, compile_commands.json missing, timeout, crash — is a silent exit 0,
so an incomplete toolchain never blocks work.

Headers aren't compile-database entries, so they are linted through a
translation unit that includes them: core headers through
core/tests/rt_compile_check.cpp (which by contract includes every core
header), problem headers through a TU from the same problem directory.
Findings are filtered back to the edited file, and journaled compactly
(type "static_analysis", check names only) so /evolve can cluster recurring
lint friction.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time

from _journal import append_entry

CPP_EXTENSIONS = {".hpp", ".cpp", ".h", ".cc", ".cxx"}
HEADER_EXTENSIONS = {".hpp", ".h"}
MAX_REPORT_LINES = 40
TIDY_TIMEOUT = 120

WARNING_LINE = re.compile(
    r"^(?P<path>[^\s:][^:]*):\d+:\d+: (?:warning|error): .* \[(?P<checks>[^\]]+)\]$"
)


def same_file(printed_path: str, target_realpath: str, project: str) -> bool:
    p = printed_path
    if not os.path.isabs(p):
        p = os.path.join(project, p)
    return os.path.realpath(p) == target_realpath


def db_files(db_path: str) -> set[str]:
    with open(db_path, encoding="utf-8") as f:
        return {os.path.realpath(entry["file"]) for entry in json.load(f)}


def lint_target(abs_path: str, rel: str, project: str, db_path: str) -> str | None:
    """The TU to hand clang-tidy: the file itself, or one that includes it."""
    try:
        files = db_files(db_path)
    except Exception:
        return None
    if os.path.splitext(abs_path)[1] not in HEADER_EXTENSIONS:
        return abs_path if os.path.realpath(abs_path) in files else None
    if rel.startswith("core" + os.sep):
        tu = os.path.join(project, "core", "tests", "rt_compile_check.cpp")
        return tu if os.path.realpath(tu) in files else None
    parts = rel.split(os.sep)
    if len(parts) < 2 or parts[0] != "problems":
        return None
    problem_prefix = os.path.realpath(os.path.join(project, parts[0], parts[1])) + os.sep
    candidates = sorted(f for f in files if f.startswith(problem_prefix))
    if not candidates:
        return None
    for f in candidates:
        if os.sep + "src" + os.sep in f:
            return f
    return candidates[0]


def extract_findings(stdout: str, abs_path: str, project: str):
    target = os.path.realpath(abs_path)
    lines: list[str] = []
    checks: set[str] = set()
    count = 0
    for line in stdout.splitlines():
        m = WARNING_LINE.match(line)
        if m is None or not same_file(m.group("path"), target, project):
            continue
        count += 1
        checks.update(c.strip() for c in m.group("checks").split(","))
        if len(lines) < MAX_REPORT_LINES:
            lines.append(line)
    return lines, sorted(checks), count


def main() -> int:
    try:
        data = json.load(sys.stdin)
    except Exception:
        return 0
    path = str((data.get("tool_input") or {}).get("file_path") or "")
    if not path or os.path.splitext(path)[1] not in CPP_EXTENSIONS:
        return 0
    project = os.path.abspath(os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd())
    abs_path = os.path.abspath(path)
    if not abs_path.startswith(project + os.sep) or not os.path.isfile(abs_path):
        return 0
    rel = os.path.relpath(abs_path, project)
    if not rel.startswith(("core" + os.sep, "problems" + os.sep)):
        return 0
    tidy = shutil.which("clang-tidy")
    build_dir = os.path.join(project, "build", "debug")
    db_path = os.path.join(build_dir, "compile_commands.json")
    if tidy is None or not os.path.isfile(db_path):
        return 0
    target = lint_target(abs_path, rel, project, db_path)
    if target is None:
        return 0
    cmd = [tidy, "-p", build_dir, "--quiet"]
    if os.path.splitext(abs_path)[1] in HEADER_EXTENSIONS:
        cmd.append("--header-filter=" + re.escape(abs_path))
    cmd.append(target)
    try:
        proc = subprocess.run(
            cmd, capture_output=True, text=True, timeout=TIDY_TIMEOUT, check=False
        )
    except Exception:
        return 0
    lines, checks, count = extract_findings(proc.stdout, abs_path, project)
    if count == 0:
        return 0
    append_entry(
        {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "type": "static_analysis",
            "file": rel,
            "checks": checks,
            "count": count,
            "tags": ["clang-tidy"],
        }
    )
    sys.stderr.write(
        f"clang-tidy: {count} finding(s) in {rel}:\n"
        + "\n".join(lines)
        + "\nFix these, or if a check is genuinely inapplicable here, suppress the"
        " single line with NOLINT(<check-name>) plus a brief justification comment.\n"
    )
    return 2


if __name__ == "__main__":
    try:
        rc = main()
    except Exception:
        rc = 0
    sys.exit(rc)
