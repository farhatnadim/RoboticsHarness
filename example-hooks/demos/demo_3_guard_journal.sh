#!/usr/bin/env bash
# Demo 3 — a PreToolUse hook vetoes a tool call before it runs.
#
# CLAUDE.md declares evolution/journal.ndjson append-only. If the agent tries
# to Edit/Write it (even with good intentions, e.g. "let me clean up that
# journal entry"), guard_journal.py exits 2: the tool call is blocked before
# any byte is written, and the stderr message redirects the agent to the
# sanctioned append path.
#
# Runs the real hook directly — it only reads, so there is nothing to revert.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
export CLAUDE_PROJECT_DIR="$PWD"

echo "=== 1. Agent attempts: Edit(file_path=evolution/journal.ndjson) ==="
echo
echo "=== 2. PreToolUse fires guard_journal.py BEFORE the edit executes ==="
rc=0
printf '{"tool_input":{"file_path":"%s/evolution/journal.ndjson"}}' "$PWD" \
    | python3 .claude/hooks/guard_journal.py || rc=$?
echo "--- hook exit code: $rc (2 = tool call blocked, file untouched) ---"

echo
echo "=== 3. The agent self-corrects to the sanctioned path the hook named ==="
echo '   bash tools/journal_note.sh "<the lesson>" tag1 tag2'
echo "(not executed here — it would append a real entry to the real journal)"
