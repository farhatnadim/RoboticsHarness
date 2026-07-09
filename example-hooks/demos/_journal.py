"""Demo stub for .claude/hooks/_journal.py.

The demo scripts copy a hook into a temp directory and run it with this
directory first on PYTHONPATH, so the hook's `from _journal import ...`
resolves here instead of the real journal writer. Live demo runs therefore
NEVER append to the append-only evolution/journal.ndjson — they print what
the real hook would have journaled.
"""
import json
import os


def journal_path() -> str:
    root = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    return os.path.join(root, "evolution", "journal.ndjson")


def append_entry(entry: dict) -> None:
    line = json.dumps(entry, ensure_ascii=False)
    if len(line) > 300:
        line = line[:300] + "…(truncated)"
    print(f"[demo] real hook would append to evolution/journal.ndjson:\n[demo]   {line}")
