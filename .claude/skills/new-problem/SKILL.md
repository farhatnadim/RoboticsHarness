---
name: new-problem
description: Scaffold a new robotics problem project under problems/ from the template, wired to harness::core, then build it and run its smoke test. Use when the user wants to start attacking a new robotics problem.
---

# /new-problem <name> [one-line description]

Scaffold `problems/<name>/` from `problems/_template/`, build it, and prove it works.

## Steps

1. **Validate the name**: must match `[a-z0-9_-]+`, must not be `_template`, and
   `problems/<name>/` must not already exist. If the description is missing, ask for a
   one-liner or derive it from the conversation.

2. **Render the template**: for each `*.in` file under `problems/_template/`, create the
   corresponding file under `problems/<name>/` (same relative path, `.in` suffix stripped),
   replacing every `@PROBLEM_NAME@` with the name and `@PROBLEM_DESC@` with the description.
   Also create the (empty) `include/` directory — problem-local headers go there and the
   rendered CMakeLists.txt already wires it into both targets.
   Result: `CMakeLists.txt`, `GOALS.md`, `README.md`, `src/main.cpp`,
   `tests/test_smoke.cpp`, `include/`.

3. **Configure and build** — reconfigure **both** presets, not just debug: the top-level
   glob only sees the new directory on reconfigure, and `/rt-check` builds in `release`
   (a debug-only reconfigure has produced `ninja: unknown target` there before):
   ```sh
   cmake --preset debug && cmake --preset release
   cmake --build --preset debug --target <name>_app <name>_tests
   ```

4. **Run the smoke test**: `ctest --preset debug -R <name>` and run the app binary once
   (`build/debug/problems/<name>/<name>_app`). Both must pass.

5. **Journal it**: append one line to `evolution/journal.ndjson`:
   `{"ts": "<utc-iso8601>", "type": "new_problem", "cmd": "<name>", "excerpt": "<description>", "tags": ["new-problem"]}`
   — take the timestamp from `date -u +%FT%TZ` (hand-written local times with a `Z`
   suffix have polluted the journal before).

6. **Report**: point the user at `problems/<name>/GOALS.md` for goal tracking, and mention
   that a realtime target opts in with `harness_enable_realtime(<name>_app)` in the
   problem's CMakeLists.txt, verified via `/rt-check`.

## Rules

- Never hand-edit `problems/_template/` as part of scaffolding a problem; template
  improvements go through `/evolve`.
- The new problem links `harness::core` — if it needs code from another problem, that code
  should be promoted to core first (CLAUDE.md promotion rule).
