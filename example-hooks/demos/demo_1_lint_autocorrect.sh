#!/usr/bin/env bash
# Demo 1 — static analysis catches what the compiler accepts.
#
# The agent writes a function that compiles clean under -Werror, but uses a
# literal 0 as a null pointer. The PostToolUse hook (lint_cpp.py) runs
# clang-tidy on the edited file, exits 2, and its stderr is fed straight back
# to the agent, which fixes the line; the re-run is clean.
#
# Safe to re-run: the injected line is reverted on exit, and journal writes
# are stubbed out (see _journal.py in this directory).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
export CLAUDE_PROJECT_DIR="$PWD"

header=problems/cart-pole/include/cart_pole_model.hpp
git diff --quiet -- "$header" || { echo "refusing to run: $header has uncommitted changes" >&2; exit 1; }

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"; git checkout -- "'"$header"'"' EXIT
cp .claude/hooks/lint_cpp.py "$tmp/"
export PYTHONPATH="$PWD/example-hooks/demos:$PWD/.claude/hooks"

payload() { printf '{"tool_input":{"file_path":"%s/%s"}}' "$PWD" "$header"; }

echo "=== 1. Agent edit: compiles fine under -Werror, but 0 used as a null pointer ==="
printf 'inline int* demo_bug() { return 0; } // injected by demo_1\n' >> "$header"
tail -1 "$header"

echo
echo "=== 2. PostToolUse fires lint_cpp.py; exit 2 feeds stderr back to the agent ==="
rc=0
payload | python3 "$tmp/lint_cpp.py" || rc=$?
echo "--- hook exit code: $rc (2 = findings returned to the agent) ---"

echo
echo "=== 3. The agent applies the fix the hook pointed at (0 -> nullptr) ==="
sed -i 's|return 0; } // injected by demo_1|return nullptr; } // injected by demo_1|' "$header"
tail -1 "$header"

echo
echo "=== 4. PostToolUse re-fires on the fix: clean ==="
rc=0
payload | python3 "$tmp/lint_cpp.py" || rc=$?
echo "--- hook exit code: $rc (0 = clean, agent moves on) ---"
