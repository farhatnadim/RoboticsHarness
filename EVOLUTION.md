# Evolution changelog

Human-readable record of harness self-improvements applied via `/evolve`.
Each entry lists what changed and which journal clusters motivated it.
The raw evidence lives in `evolution/journal.ndjson` (append-only).

## 2026-07-03

Initial v0 build-out and verification pass. Cluster summary from the journal
(entries since repo creation, no prior `evolve_mark`):

- **build_failure / test_failure clusters (bulk of entries)** — traced entirely to
  deliberate, self-reverted verification steps in this same session: an injected
  out-of-bounds array read (to prove ASan/UBSan fire), an injected `throw` in
  `Pid::update` (to prove the realtime compile check catches it), and the
  `balance-demo` scaffold's pre-fix "No tests were found" run. All already resolved
  by reverting the injections / fixing the root cause below. **No action** — one-off
  session noise, not recurring friction.
- **Template test-naming bug (1 occurrence, fixed directly)** —
  `problems/_template/CMakeLists.txt.in`'s `gtest_discover_tests` call had no
  `TEST_PREFIX`, so the skill's documented `ctest --preset debug -R <name>` never
  matched any test for a scaffolded problem. Fixed by adding
  `TEST_PREFIX "<name>."` to the template (and to `core`'s already-rendered
  `balance-demo` verification copy, since removed).
- **rt_symbol_scan.sh false positive (1 occurrence, fixed directly)** — the scanner's
  plain substring grep for `_Znwm`/`_Znam` also matched the harmless placement-new
  mangled forms (`_ZnwmPv`/`_ZnamPv`), used internally by `std::optional`/Eigen and
  never actually allocating. Fixed by switching to `nm -C` demangled output with
  end-anchored patterns so only the allocating `operator new(unsigned long)` form is
  flagged.
- **Debug vs. release build artifact (note, no fix needed)** — confirmed
  `core_rt_compile_check` shows a dead (never-called) reference to
  `operator new(unsigned long)` under the unoptimized debug preset, but scans clean
  under release. Validates `/rt-check`'s existing choice to gate on the release
  preset rather than debug.

**Backlog item added**: `capture_failure.py` classifies failures by regex pattern
matching over raw stdout/stderr text rather than actual command exit codes, which can
misfire on chained (`&&`) commands or historical text embedded in later output. Low
priority since it only over-captures (never blocks work) — tracked in `GOALS.md`.

## 2026-07-03 (later same day)

User installed clang/clang-format/clang-tidy 18.1.3; switched `CMakePresets.json` back to
`clang++` per the backlog item, wiped and reconfigured both build dirs, and reverified
(green on both presets). This surfaced a second, more interesting `rt_symbol_scan.sh`
finding while re-running `/rt-check`:

- **Static operator-new scanning is fundamentally unreliable for Eigen decomposition code
  (fixed directly, design change)** — under clang, even the *release* preset showed a
  reference to `operator new(unsigned long)` in `core_rt_compile_check` (unlike gcc, where
  release was clean). Traced via `nm -C` + `objdump -r` to Eigen's generic
  `triangular_solve_matrix`/`gemm_pack_rhs` internals (used by `.ldlt().solve()`, which both
  `dlqr` and `KalmanFilter::update` call) — these reference `operator new`/`malloc`
  statically regardless of matrix size, with no way for a symbol scan to tell "compiled in
  but never taken for our fixed 2x2/2x1/1x1 types" from "actually called". Redesigned
  `tools/rt_symbol_scan.sh`: exception/RTTI symbols remain hard failures (reliable — a bare
  `throw` is a compile error under `-fno-exceptions`, confirmed empirically); allocating
  `operator new`/`new[]` became an advisory that points at a runtime malloc-guard test
  instead of failing the gate. Added `RealtimeGuard.LqrDoesNotAllocate` and
  `RealtimeGuard.KalmanDoesNotAllocate` (`core/tests/test_control.cpp`,
  `core/tests/test_estimation.cpp`) using `Eigen::internal::set_is_malloc_allowed(false)` —
  both pass, proving no runtime allocation for our concrete fixed sizes. Documented the
  distinction in `CLAUDE.md` and rewrote `/rt-check`'s Gate 2/3 description (added a Gate 4
  for the runtime malloc-guard pattern).
- Marked the "install clang" backlog item done in `GOALS.md`.
