# Hook failure → agent auto-correction: captured walkthroughs

All output below is **verbatim from real runs in this repository** (2026-07-09,
clang 18.1.3). Examples 1–3 are reproducible live via the scripts in `demos/`;
example 4 is quoted from the gate's original verification run. In every case the
pattern is the same:

> bad change → hook exits 2 → **stderr is fed to the agent as input** → agent edits
> → hook re-runs clean.

---

## Example 1 — clang-tidy catches what `-Werror` accepts (PostToolUse)

The agent writes a function that compiles cleanly under the harness's full
warnings-as-errors set, but uses `0` as a null pointer:

```cpp
inline int* demo_bug() { return 0; }
```

The `PostToolUse` hook `lint_cpp.py` fires on the edit and exits 2. This exact text
lands in the agent's context:

```
clang-tidy: 1 finding(s) in problems/cart-pole/include/cart_pole_model.hpp:
/home/…/problems/cart-pole/include/cart_pole_model.hpp:47:33: warning: use nullptr [modernize-use-nullptr]
Fix these, or if a check is genuinely inapplicable here, suppress the single line
with NOLINT(<check-name>) plus a brief justification comment.
```

and the hook journals the friction for the `/evolve` loop:

```json
{"ts": "2026-07-09T11:22:58Z", "type": "static_analysis",
 "file": "problems/cart-pole/include/cart_pole_model.hpp",
 "checks": ["modernize-use-nullptr"], "count": 1, "tags": ["clang-tidy"]}
```

**Auto-correction:** the agent applies the fix the message names —
`return 0;` → `return nullptr;` — and the hook re-runs on the follow-up edit:

```
hook exit code: 0   (clean, agent moves on)
```

*Presentation point:* the finding arrives seconds after the edit, while the agent
still holds the file and the intent in context. No human ever saw the bad line.

---

## Example 2 — AddressSanitizer failure blocks the agent from stopping (Stop)

The agent adds a test with an out-of-bounds read that no compiler warning can prove
(the index is a runtime value):

```cpp
TEST(DemoAsan, ReadsPastTheEndOfABuffer) {
    double gains[4] = {1.0, 2.0, 3.0, 4.0};
    volatile int idx = 4;           // runtime value defeats static analysis
    EXPECT_LT(gains[idx], 10.0);    // ASan: stack-buffer-overflow
}
```

It compiles. Plain tests would even *pass* on a lucky stack layout. But when the
agent tries to end its turn, the `Stop` hook `gate_stop.py` rebuilds the debug
preset — where ASan/UBSan are always on — and runs the full suite:

```
Stop gate: `ctest --preset debug --output-on-failure` failed on this session's
C++/CMake changes (ASan/UBSan debug preset):
.DemoAsan.ReadsPastTheEndOfABuffer ........***Failed    0.48 sec
[ RUN      ] DemoAsan.ReadsPastTheEndOfABuffer
=================================================================
==2552861==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7b6ed5100340 …
READ of size 8 at 0x7b6ed5100340 thread T0
    #1 0x571fc7ae5beb in DemoAsan_ReadsPastTheEndOfABuffer_Test::TestBody()
       problems/cart-pole/tests/test_smoke.cpp:62:5
    …
Fix the failures and re-run the debug build and tests; the suite must be green
before stopping (CLAUDE.md definition of done).

hook exit code: 2   (the agent is NOT allowed to end its turn)
```

The failure is also journaled — note the hook *classified* it as a sanitizer issue,
not a generic test failure:

```json
{"ts": "2026-07-09T11:24:11Z", "type": "sanitizer_failure", "session_id": "demo",
 "cmd": "ctest --preset debug --output-on-failure",
 "excerpt": ".DemoAsan.ReadsPastTheEndOfABuffer ........***Failed …"}
```

**Auto-correction:** the excerpt names the exact test and line; the agent fixes the
index (`idx = 4` → `idx = 3`) and tries to stop again:

```
hook exit code: 0   (full sanitized suite green, stop allowed)
```

*Presentation point:* this mechanically enforces the repo rule "never claim done
without having run the build and tests" — the agent literally cannot finish its turn
with a red suite. A loop guard (`stop_hook_active`) downgrades to advisory if the
agent already got one repair attempt, so an unfixable failure can't spin forever.

---

## Example 3 — PreToolUse veto: the edit never happens (guard_journal)

`evolution/journal.ndjson` is append-only by repo law — it's the evidence stream the
self-improvement loop runs on. Suppose the agent decides to "clean up" an old entry
with the Edit tool. The `PreToolUse` hook fires **before the tool executes**:

```
evolution/journal.ndjson is append-only (CLAUDE.md evolution rules).
Append via: bash tools/journal_note.sh "<the lesson>" tag1 tag2

hook exit code: 2   (tool call blocked, file untouched)
```

**Auto-correction:** the veto message doesn't just say *no* — it names the sanctioned
path, and the agent re-routes to it:

```bash
bash tools/journal_note.sh "<the lesson>" tag1 tag2
```

*Presentation point:* PostToolUse hooks correct damage; PreToolUse hooks make the
damage impossible. This is the strongest form — a written rule the model *cannot*
violate even if it reasons itself into wanting to.

---

## Example 4 — build break blocks the stop (from the gate's verification run)

Captured when `gate_stop.py` was first installed: a deliberate
`static_assert(false, "gate_stop deliberate failure")` was appended to
`problems/cart-pole/src/main.cpp`, and the agent's stop attempt produced:

```
Stop gate: `cmake --build --preset debug` failed on this session's C++/CMake changes
(ASan/UBSan debug preset):
/home/…/problems/cart-pole/src/main.cpp:39:15: error: static assertion failed:
gate_stop deliberate failure
   39 | static_assert(false, "gate_stop deliberate failure");
      |               ^~~~~
ninja: build stopped: subcommand failed.

hook exit code: 2
```

On the simulated retry (`stop_hook_active: true`) the gate reported but did not
block again:

```
Already retried once via this gate — not blocking again, but the suite is still red
and must not be reported as done.

hook exit code: 0
```

Both runs were journaled as `build_failure` entries tagged `gate-stop` — visible in
`evolution/journal.ndjson` at `2026-07-09T10:35:44Z` / `10:35:46Z`.

---

## Timing summary (measured)

| Path | Cost |
|---|---|
| Stop gate, nothing relevant changed | 0.04 s (no-op) |
| Stop gate, unchanged tree since last green run | 0.04 s (content-digest stamp) |
| Stop gate, C++ changed → incremental build + 42-test ASan/UBSan suite | ~15 s |
| lint_cpp on one edited header (via its including TU) | ~10–15 s |
