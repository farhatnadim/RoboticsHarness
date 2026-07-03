#!/usr/bin/env python3
"""PreToolUse hook (Edit|Write|MultiEdit): enforce the append-only journal.

CLAUDE.md declares evolution/journal.ndjson append-only; the hooks and
tools/journal_note.sh are its only writers. Exit 2 blocks the tool call
(stderr is fed back to the agent).
"""
import json
import os
import sys


def main() -> int:
    try:
        data = json.load(sys.stdin)
    except Exception:
        return 0
    path = str((data.get("tool_input") or {}).get("file_path") or "")
    if not path:
        return 0
    root = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    journal = os.path.realpath(os.path.join(root, "evolution", "journal.ndjson"))
    if os.path.realpath(path) == journal:
        print(
            "evolution/journal.ndjson is append-only (CLAUDE.md evolution rules). "
            'Append via: bash tools/journal_note.sh "<the lesson>" tag1 tag2',
            file=sys.stderr,
        )
        return 2
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        sys.exit(0)
