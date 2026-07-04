# CAUTION
PURLE CLAUDE CODE FABLE 5 generated, I DID NOT DO ANY EDITS OR REVIEW , use it your OWN RISK. Fable 5 pulled skills from other Github , I attributted them to their orignal owners. Please do the same. 


# RoboticsHarness

A self-evolving robotics C++ harness: a growing Eigen-based core library, per-problem
project scaffolding, and a Claude Code evolution loop that turns captured friction into
harness improvements.

## Layout

| Path | Purpose |
|---|---|
| `core/` | Header-only C++20 library (`harness::core`): spatial math, control, estimation, trajectories, kinematics |
| `problems/` | One directory per robotics problem, scaffolded by `/new-problem`, linking `harness::core` |
| `cmake/` | Flag tiers (`harness::default_flags`, `harness::realtime_flags`), dependencies |
| `evolution/` | Append-only lesson journal (`journal.ndjson`) feeding `/evolve` |
| `.claude/` | Skills (`/new-problem`, `/evolve`, `/rt-check`), hooks (auto-format, failure capture, journal guard), and 13 vendored third-party robotics/embedded/C++ skills |
| `tools/` | `rt_symbol_scan.sh` — nm scan for exception/RTTI symbols (hard fail) and allocation-capable symbols (advisory) in realtime targets |

## Building

```sh
cmake --preset debug          # configure (ASan/UBSan on)
cmake --build --preset debug
ctest --preset debug
```

`release` preset: same commands with `release`. Compile database at `build/debug/compile_commands.json` (`.clangd` points there).

## Two tiers

- **Default**: C++20, full warning set as errors, sanitizers in Debug. Every target links `harness::default_flags`.
- **Realtime** (opt-in): `harness_enable_realtime(<target>)` adds `-fno-exceptions -fno-rtti` and `EIGEN_NO_MALLOC`. Core headers are kept realtime-clean at all times, enforced by the `core_rt_compile_check` target and `/rt-check`.

## Evolution loop

Hooks journal build/test/sanitizer failures automatically into `evolution/journal.ndjson`.
Run `/evolve` (or when nudged at session start) to cluster the journal and apply reviewed
improvements to skills, rules, and the backlog. Applied changes are logged in `EVOLUTION.md`.

## Bundled third-party skills

Thirteen vendored Claude Code skills ship with the repo so any clone gets robotics
domain knowledge out of the box: ten ROS/robotics-engineering skills from
[arpitg1304/robotics-agent-skills](https://github.com/arpitg1304/robotics-agent-skills)
(Apache-2.0), `embedded-systems` and `cpp-pro` from
[Jeffallan/claude-skills](https://github.com/Jeffallan/claude-skills) (MIT), and
`arm-cortex-expert` from
[sickn33/antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills)
(MIT, lightly cleaned). Full attribution, license texts, per-source commit SHAs, and
the update procedure live in `.claude/skills/THIRD_PARTY.md` and `.claude/skills/licenses/`.

## Toolchain

CMake 3.28.3, Ninja 1.11.1, Clang/clang-format/clang-tidy 18.1.3, Python 3.12.3, system
Eigen3 at `/usr/include/eigen3`. GoogleTest always via pinned FetchContent (v1.14.0); no
conan/vcpkg. All harness flags are also GCC-compatible if clang is ever unavailable.
