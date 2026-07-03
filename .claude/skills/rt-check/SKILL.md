---
name: rt-check
description: Verify realtime-tier compliance for a target (or all realtime targets) — compiles under harness::realtime_flags, scans object files for exception/RTTI/heap symbols, and runs clang-tidy's realtime check set. Use before trusting any harness_enable_realtime() target.
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
   `-fno-exceptions -fno-rtti -Werror` plus `EIGEN_NO_MALLOC` compliance for that TU.

3. **Gate 2 — symbol scan**: `tools/rt_symbol_scan.sh build/release <target>`. This greps
   `nm` output over the target's `.o` files for `_Znwm`/`_Znam` (operator new/new[]),
   `__cxa_throw`, `__cxa_allocate_exception`, and `_ZTI*` (typeinfo). Non-zero exit means a
   violation.

4. **Gate 3 — clang-tidy realtime checks**: if `clang-tidy` is installed, run it over the
   target's source files with `cppcoreguidelines-no-malloc`, `cppcoreguidelines-owning-memory`,
   `modernize-avoid-c-arrays`, `misc-no-recursion`, `bugprone-exception-escape` enabled
   (these are already part of `.clang-tidy`'s check set). If clang-tidy isn't installed,
   report that gate as skipped, not failed.

5. **Report** pass/fail per gate per target. On any failure, append a journal entry:
   `{"ts": "<utc-iso8601>", "type": "rt_violation", "cmd": "<target>", "excerpt": "<which gate failed and why>", "tags": ["rt-check"]}`.

## Rules

- Core headers must always pass all gates — a failure here blocks any change touching
  `core/` per the CLAUDE.md definition of done.
- Don't weaken a gate to make a target pass; fix the code or move it out of the realtime
  tier and say so explicitly.
