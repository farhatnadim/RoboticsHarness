"""Shared journal helpers for the RoboticsHarness hooks.

Single source of truth for locating evolution/journal.ndjson and appending
entries to it (one O_APPEND write per entry, so concurrent sessions don't
interleave partial lines).
"""
import json
import os


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
