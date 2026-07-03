---
name: rt-check
description: Verify realtime-tier compliance for a target (or all realtime targets) — compiles under harness::realtime_flags, scans object files for exception/RTTI symbols, flags allocation-capable code for a runtime malloc-guard test, and runs clang-tidy's realtime check set. Use before trusting any harness_enable_realtime() target.
---

# /rt-check [target]

Verify that a realtime-tier target (or all of them) actually meets the realtime rules in
CLAUDE.md, not just that it compiles.

## Steps

1. **Discover targets**: if no argument given, `grep -rn "harness_enable_realtime(" .`
   across `core/` and `problems/` (plus the always-present `core_rt_compile_check`) to
   build the list of realtime targets.

2. **Gate 1 — compiles under realtime flags**: build the target in the `release` preset:
   `cmake --build --preset release --target <target>`. A clean build already proves
   `-fno-exceptions -fno-rtti -Werror` compliance for that TU (a bare `throw` or RTTI use
   is a hard compile error under these flags — confirmed empirically). Note: a plain `new`
   expression still compiles fine even with `-fno-exceptions`, so this gate does not by
   itself prove the absence of heap allocation; that's Gate 2 and Gate 4's job.

3. **Gate 2 — symbol scan**: `tools/rt_symbol_scan.sh build/release <target>`. HARD failures
   (non-zero exit): `__cxa_throw`, `__cxa_allocate_exception`, RTTI typeinfo — these are
   reliable signals of actual exception/RTTI codegen. SOFT advisories (printed, does not
   fail the gate): allocating `operator new`/`new[]` references. Eigen's generic
   blocked-GEMM internals (`.ldlt()`, `.inverse()`, similar decompositions) statically
   reference `operator new`/`malloc` even for small fixed-size matrices that never actually
   take that path at runtime — a static symbol scan cannot tell "compiled in but dead"
   from "actually called", so this is advisory, not a hard failure. Placement new is
   excluded entirely (it never allocates).

4. **Gate 4 — runtime malloc-guard test (when Gate 2 emits an advisory)**: if the target's
   code path uses an Eigen decomposition or otherwise trips the Gate 2 advisory, don't take
   the static result on faith — add or point to a test that wraps the call in
   `Eigen::internal::set_is_malloc_allowed(false)` / `(true)` with `EIGEN_RUNTIME_NO_MALLOC`
   defined on the test target (see `RealtimeGuard.LqrDoesNotAllocate` and
   `RealtimeGuard.KalmanDoesNotAllocate` in `core/tests/` for the pattern) and confirm it
   passes. This is the authoritative check for "does this code allocate for these concrete
   types", not the static scan.

5. **Gate 3 — clang-tidy realtime checks**: run clang-tidy over the target's source files
   with `cppcoreguidelines-no-malloc`, `cppcoreguidelines-owning-memory`,
   `modernize-avoid-c-arrays`, `misc-no-recursion`, `bugprone-exception-escape` enabled
   (already part of `.clang-tidy`'s check set). If clang-tidy isn't installed, report that
   gate as skipped, not failed.

6. **Report** pass/fail per gate per target. On any hard failure, append a journal entry:
   `{"ts": "<utc-iso8601>", "type": "rt_violation", "cmd": "<target>", "excerpt": "<which gate failed and why>", "tags": ["rt-check"]}`.

## Rules

- Core headers must always pass Gates 1–3 and have Gate 4 coverage for any code that trips
  the Gate 2 advisory — a failure here blocks any change touching `core/` per the CLAUDE.md
  definition of done.
- Don't weaken a gate to make a target pass; fix the code, add the missing runtime
  malloc-guard test, or move it out of the realtime tier and say so explicitly.
