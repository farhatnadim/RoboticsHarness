#!/usr/bin/env python3
"""SessionEnd hook: append a session_end record to the evolution journal.

Never blocks: always exits 0.
"""
import json
import sys
import time

from _journal import append_entry


def main() -> None:
    try:
        data = json.load(sys.stdin)
    except Exception:
        data = {}
    append_entry(
        {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "type": "session_end",
            "session_id": str(data.get("session_id") or ""),
        }
    )


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
