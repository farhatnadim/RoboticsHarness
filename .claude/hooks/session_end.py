#!/usr/bin/env python3
"""SessionEnd hook: append a session_end record to the evolution journal.

Never blocks: always exits 0.
"""
import json
import os
import sys
import time


def journal_path() -> str:
    root = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    return os.path.join(root, "evolution", "journal.ndjson")


def main() -> None:
    try:
        data = json.load(sys.stdin)
    except Exception:
        data = {}
    entry = {
        "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "type": "session_end",
        "session_id": str(data.get("session_id") or ""),
    }
    line = (json.dumps(entry, ensure_ascii=False) + "\n").encode("utf-8")
    fd = os.open(journal_path(), os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o644)
    try:
        os.write(fd, line)
    finally:
        os.close(fd)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
