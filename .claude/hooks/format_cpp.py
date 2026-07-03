#!/usr/bin/env python3
"""PostToolUse hook (Edit|Write): clang-format any C++ file the agent touched.

Never blocks the agent: always exits 0, silent on every failure mode
(including clang-format simply not being installed).
"""
import json
import os
import subprocess
import sys

CPP_EXTENSIONS = {".hpp", ".cpp", ".h", ".cc", ".cxx"}


def main() -> None:
    try:
        data = json.load(sys.stdin)
    except Exception:
        return
    path = str((data.get("tool_input") or {}).get("file_path") or "")
    if not path or os.path.splitext(path)[1] not in CPP_EXTENSIONS:
        return
    project = os.environ.get("CLAUDE_PROJECT_DIR", "")
    if project:
        abs_path = os.path.abspath(path)
        if not abs_path.startswith(os.path.abspath(project) + os.sep):
            return
    if not os.path.isfile(path):
        return
    try:
        subprocess.run(
            ["clang-format", "-i", path],
            timeout=30,
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except Exception:
        pass


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
