#!/usr/bin/env python3
"""SessionStart hook: nudge toward /evolve when the journal has piled up.

Stdout from SessionStart hooks is injected into the session context, so the
assistant sees the nudge and can relay it. Never blocks: always exits 0.
"""
import json
import os
import sys

NUDGE_THRESHOLD = 10


def journal_path() -> str:
    root = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    return os.path.join(root, "evolution", "journal.ndjson")


def main() -> None:
    unprocessed = 0
    try:
        with open(journal_path(), encoding="utf-8") as f:
            for raw in f:
                raw = raw.strip()
                if not raw:
                    continue
                try:
                    entry = json.loads(raw)
                except Exception:
                    continue
                kind = entry.get("type")
                if kind == "evolve_mark":
                    unprocessed = 0
                elif kind != "session_end":
                    unprocessed += 1
    except OSError:
        return
    if unprocessed >= NUDGE_THRESHOLD:
        print(
            f"The evolution journal has {unprocessed} unprocessed entries since the "
            f"last /evolve run. Suggest running /evolve to fold the lessons back "
            f"into the harness."
        )


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
