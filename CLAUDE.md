# CLAUDE.md — RoboticsHarness

## What this is

A self-evolving robotics C++ harness with three parts that must stay coherent:

- `core/` — header-only C++20 library (`harness::core`) of robotics building blocks
  (spatial math, control, estimation, trajectories, kinematics). Grows over time.
- `problems/` — one sub-project per robotics problem, always linking `harness::core`.
- `evolution/` + `.claude/` — the self-improvement loop: hooks journal friction
  automatically, `/evolve` folds it back into skills, rules, and backlog.

Plain C++/Eigen. **ROS-free but ROS-ready**: no framework types (ROS, Qt, …) may appear
in core public APIs — only Eigen and the standard library — so adapters stay thin.

## Definition of done

For any change touching `core/`:

1. New or updated gtest coverage in `core/tests/`.
2. `cmake --build --preset debug && ctest --preset debug` green (ASan/UBSan are on in debug).
3. `core_rt_compile_check` still compiles (new headers must be added to
   `core/tests/rt_compile_check.cpp`).
4. clang-format applied (the PostToolUse hook does this; don't fight it).

For problem code: the problem's own tests green in the debug preset.

Never claim done without having run the build and tests.

## Two tiers

- **Default** (`harness::default_flags`): every target links it. C++20, full warning set
  as `-Werror`, sanitizers in Debug. Non-negotiable for all code including prototypes.
- **Realtime** (opt-in via `harness_enable_realtime(<target>)`): adds `-fno-exceptions
  -fno-rtti`, `EIGEN_NO_MALLOC`. Rules for realtime targets: no heap allocation on hot
  paths, fixed-size Eigen types only, no recursion, bounded loops, every variable
  initialized, no implicit narrowing, `[[nodiscard]]` on value-returning functions,
  errors via `std::optional` — never exceptions.
- **Core headers must always stay realtime-clean** regardless of who consumes them;
  `core_rt_compile_check` enforces this at every build. Verify targets with `/rt-check`.

## Layout rules

- Create new problems **only** via `/new-problem` — never hand-roll a problem directory.
- Problems link `harness::core`; they never copy core code.
- **Promotion rule**: code used by ≥2 problems, or clearly general, gets promoted into
  `core/` with tests. Record the promotion as a `note` entry in the journal.
- Scope that gets cut goes to `GOALS.md` as a `- [ ]` item — never silently dropped.

## Evolution rules

- `evolution/journal.ndjson` is **append-only**. Never delete or rewrite lines.
- Hooks journal build/test/sanitizer failures automatically. When you notice recurring
  friction the hooks can't see (e.g. repeatedly looking up the same API, a template gap),
  append a `note` entry yourself:
  `{"ts": "<utc-iso>", "type": "note", "excerpt": "<the lesson>", "tags": [..]}`.
- Run `/evolve` when the session-start nudge fires or roughly weekly.
- `/evolve` proposes diffs and applies them **only after user approval**; applied changes
  are recorded in `EVOLUTION.md`. No other path may rewrite rules or skills.

## Toolchain

- Ninja, presets only: `cmake --preset debug`, `cmake --build --preset debug`,
  `ctest --preset debug`. Same with `release`.
- **Compiler**: `g++` is the current preset compiler (`CMakeLists.txt`'s
  `CMAKE_CXX_COMPILER`) because `clang++`/`clang-format`/`clang-tidy` are not installed on
  this machine — only libclang shared libs are present. Clang is still the intended
  long-term compiler (matches the rest of the workspace and `.clangd`/`.clang-format`
  configs); switch the preset back to `clang++` once `sudo apt install clang clang-format
  clang-tidy` has been run (backlog item in `GOALS.md`). All flags in
  `cmake/HarnessFlags.cmake` are GCC/Clang-compatible so the switch is a one-line preset
  change.
- The `format_cpp.py` hook and `/rt-check`'s clang-tidy gate no-op silently when the
  respective binary is missing — they do not block work in the meantime.
- compile_commands.json lives in `build/debug` (`.clangd` points there).
- Dependencies: Eigen via system find_package with pinned FetchContent fallback; GoogleTest
  always pinned FetchContent. No conan/vcpkg.
- `GOALS.md` follows The_Architect checkbox convention (`- [ ]` / `- [x]` under `## Goals`).
