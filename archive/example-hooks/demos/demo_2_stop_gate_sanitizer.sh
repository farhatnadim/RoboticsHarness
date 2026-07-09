#!/usr/bin/env bash
# Demo 2 — the Stop gate runs the sanitized test suite and refuses to let the
# agent finish its turn on a memory bug.
#
# The agent adds a test with an out-of-bounds read that -Werror cannot prove
# (runtime index). When the agent tries to stop, gate_stop.py rebuilds the
# ASan/UBSan debug preset and runs the whole suite: AddressSanitizer aborts
# the test, the hook exits 2 (stop BLOCKED) with the failure excerpt on
# stderr, the agent fixes the index, and the next stop attempt goes green.
#
# Safe to re-run: the injected test is reverted on exit, and journal writes
# are stubbed out (see _journal.py in this directory). Takes ~40s (two
# build+test cycles).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../../.."
export CLAUDE_PROJECT_DIR="$PWD"

testfile=problems/cart-pole/tests/test_smoke.cpp
git diff --quiet -- "$testfile" || { echo "refusing to run: $testfile has uncommitted changes" >&2; exit 1; }

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"; git checkout -- "'"$testfile"'"' EXIT
cp .claude/hooks/gate_stop.py "$tmp/"
export PYTHONPATH="$PWD/archive/example-hooks/demos:$PWD/.claude/hooks"

gate() {
    rc=0
    printf '{"session_id":"demo","stop_hook_active":false}' \
        | python3 "$tmp/gate_stop.py" || rc=$?
    echo "--- hook exit code: $rc ---"
}

echo "=== 1. Agent adds a test with an OOB read the compiler can't see ==="
cat >> "$testfile" <<'EOF'

// injected by demo_2 — out-of-bounds read that -Warray-bounds cannot prove
TEST(DemoAsan, ReadsPastTheEndOfABuffer) {
    double gains[4] = {1.0, 2.0, 3.0, 4.0};
    volatile int idx = 4; // runtime value defeats static analysis
    EXPECT_LT(gains[idx], 10.0); // ASan: stack-buffer-overflow
}
EOF
tail -6 "$testfile"

echo
echo "=== 2. Agent tries to stop; the Stop hook rebuilds + runs the ASan/UBSan suite ==="
gate
echo "(exit 2 = the agent is NOT allowed to end its turn; stderr above is its feedback)"

echo
echo "=== 3. The agent fixes the index (4 -> 3) ==="
sed -i 's|volatile int idx = 4;|volatile int idx = 3;|' "$testfile"

echo
echo "=== 4. Agent tries to stop again: full sanitized suite is green, stop allowed ==="
gate
