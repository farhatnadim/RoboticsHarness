#!/usr/bin/env python3
"""Stop hook: run the sanitized (ASan/UBSan) debug build + tests before stopping.

Mechanically enforces CLAUDE.md's "never claim done without having run the
build and tests": if any C++ / CMake file changed and the debug suite isn't
already proven green for exactly this working-tree state, build and run it.
A red suite blocks the stop (exit 2) with the failure excerpt so the agent
keeps working, and the failure is journaled with the same schema
capture_failure.py uses so /evolve clusters them together. A green run is
stamped with a content digest under build/debug/, so repeated stops with an
unchanged tree skip the rebuild entirely.

Loop safety: when the agent is already continuing because this hook blocked
once (stop_hook_active), a still-red suite is journaled and reported but no
longer blocks, so an unfixable failure can't spin forever.

Silent no-op when cmake or git is missing, when the debug preset can't be
configured, or on build/test timeout — an incomplete environment never traps
the agent (same convention as format_cpp.py / lint_cpp.py).
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

from _journal import append_entry
from capture_failure import classify

RELEVANT_EXTENSIONS = {".hpp", ".cpp", ".h", ".cc", ".cxx", ".cmake", ".in"}
RELEVANT_NAMES = {"CMakeLists.txt", "CMakePresets.json"}
SKIP_PREFIXES = ("build/", "plans/", ".claude/", "evolution/")
CONTEXT_BEFORE = 500
CONTEXT_AFTER = 1500
STEP_TIMEOUT = 420
STAMP_REL = os.path.join("build", "debug", ".gate_stop_green")


def project_root() -> str:
    return os.path.abspath(os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd())


def is_relevant(rel: str) -> bool:
    if rel.startswith(SKIP_PREFIXES):
        return False
    return (
        os.path.basename(rel) in RELEVANT_NAMES
        or os.path.splitext(rel)[1] in RELEVANT_EXTENSIONS
    )


def changed_files(project: str) -> list[str] | None:
    """Relevant modified/untracked paths, or None when git is unusable."""
    try:
        proc = subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=project, capture_output=True, text=True, timeout=30, check=False,
        )
    except Exception:
        return None
    if proc.returncode != 0:
        return None
    paths: set[str] = set()
    for line in proc.stdout.splitlines():
        if len(line) < 4:
            continue
        p = line[3:]
        if " -> " in p:
            p = p.split(" -> ", 1)[1]
        p = p.strip().strip('"')
        if p.endswith("/"):  # untracked directory: git lists it collapsed
            for root, _dirs, names in os.walk(os.path.join(project, p)):
                for name in names:
                    paths.add(os.path.relpath(os.path.join(root, name), project))
        else:
            paths.add(p)
    return sorted(p for p in paths if is_relevant(p))


def tree_digest(project: str, files: list[str]) -> str:
    h = hashlib.sha256()
    try:
        head = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=project, capture_output=True, text=True, timeout=30, check=False,
        ).stdout.strip()
    except Exception:
        head = ""
    h.update(head.encode())
    for rel in files:
        h.update(b"\0" + rel.encode())
        try:
            with open(os.path.join(project, rel), "rb") as f:
                h.update(f.read())
        except OSError:
            h.update(b"<missing>")
    return h.hexdigest()


def run_step(project: str, cmd: list[str]):
    """Run one build/test step; None means infra trouble (never blocks)."""
    try:
        return subprocess.run(
            cmd, cwd=project, capture_output=True, text=True,
            timeout=STEP_TIMEOUT, check=False,
        )
    except Exception:
        return None


def report(data: dict, cmd: list[str], proc) -> int:
    text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    hit = classify(text)
    if hit is not None:
        kind, pos = hit
    else:
        kind, pos = "build_failure", max(0, len(text) - CONTEXT_AFTER)
    excerpt = text[max(0, pos - CONTEXT_BEFORE) : pos + CONTEXT_AFTER]
    append_entry(
        {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "type": kind,
            "session_id": str(data.get("session_id") or ""),
            "cwd": "",
            "cmd": " ".join(cmd),
            "excerpt": excerpt,
            "tags": ["gate-stop"],
        }
    )
    blocking = not data.get("stop_hook_active")
    sys.stderr.write(
        f"Stop gate: `{' '.join(cmd)}` failed on this session's C++/CMake changes"
        f" (ASan/UBSan debug preset):\n{excerpt}\n"
        + (
            "Fix the failures and re-run the debug build and tests; the suite must"
            " be green before stopping (CLAUDE.md definition of done).\n"
            if blocking
            else "Already retried once via this gate — not blocking again, but the"
            " suite is still red and must not be reported as done.\n"
        )
    )
    return 2 if blocking else 0


def main() -> int:
    try:
        data = json.load(sys.stdin)
    except Exception:
        return 0
    project = project_root()
    if shutil.which("cmake") is None:
        return 0
    files = changed_files(project)
    if not files:
        return 0
    digest = tree_digest(project, files)
    stamp = os.path.join(project, STAMP_REL)
    try:
        with open(stamp, encoding="utf-8") as f:
            if f.read().strip() == digest:
                return 0
    except OSError:
        pass
    if not os.path.isfile(os.path.join(project, "build", "debug", "CMakeCache.txt")):
        configure = run_step(project, ["cmake", "--preset", "debug"])
        if configure is None or configure.returncode != 0:
            return 0  # unconfigured/incomplete environment: never trap the agent
    for cmd in (
        ["cmake", "--build", "--preset", "debug"],
        ["ctest", "--preset", "debug", "--output-on-failure"],
    ):
        proc = run_step(project, cmd)
        if proc is None:
            return 0
        if proc.returncode != 0:
            return report(data, cmd, proc)
    try:
        os.makedirs(os.path.dirname(stamp), exist_ok=True)
        with open(stamp, "w", encoding="utf-8") as f:
            f.write(digest)
    except OSError:
        pass
    return 0


if __name__ == "__main__":
    try:
        rc = main()
    except Exception:
        rc = 0
    sys.exit(rc)
