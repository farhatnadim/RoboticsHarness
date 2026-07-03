# Problems

Each subdirectory is one robotics problem: an independent CMake sub-project that links
`harness::core` and is picked up automatically by the top-level build glob.

- **Create problems only via `/new-problem <name> [description]`** — it copies `_template/`,
  renders the `@PROBLEM_NAME@` placeholders, reconfigures, builds, and runs the smoke test.
- Names: lowercase `[a-z0-9_-]+`.
- Problems **link** core, they never copy code from it. Code that proves useful to two or
  more problems gets promoted into `core/` with tests (see CLAUDE.md promotion rule).
- Realtime tier: opt a target in with `harness_enable_realtime(<name>_app)` in the
  problem's CMakeLists.txt, then verify with `/rt-check`.

`_template/` holds the unrendered `*.in` files and is never built.
