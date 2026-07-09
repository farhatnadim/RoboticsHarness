# AGENTS.md — RoboticsHarness

Cross-assistant conventions for AI agents working in this repository. Claude Code
sessions get the full working rules from `CLAUDE.md`; this file records what applies
to any agent.

## `archive/` — old topics live here

`archive/` holds closed-out topics: one-off presentation material, retired docs, and
finished explorations kept for reference only.

- **Do not read, index, or summarize anything under `archive/` unless the user
  explicitly mentions it.** It is not part of the working harness and must not
  inform builds, code changes, or reviews.
- Nothing in the live tree may depend on files under `archive/`.
- When a topic is closed out, move its material here rather than deleting it.

Current contents:

- `archive/example-hooks/` — hooks presentation material (2026-07-09 talk):
  explanation of the hook wiring, comparison with git pre-commit hooks, captured
  failure→auto-correct examples, and self-reverting live-demo scripts (still
  runnable from the repo root).
