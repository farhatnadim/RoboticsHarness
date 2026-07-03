---
name: evolve
description: Read the append-only evolution journal, cluster recurring friction, and propose reviewed improvements to CLAUDE.md rules, skills, hooks, or the GOALS.md backlog. Use when nudged at session start or roughly weekly.
---

# /evolve [--dry-run]

The self-evolution engine. Turns captured friction (build/test/sanitizer failures, rt
violations, notes, new-problem events) into reviewed harness improvements. Never applies
anything without per-item user approval.

## Steps

1. **Read the journal**: `evolution/journal.ndjson`. Find the last line with
   `"type": "evolve_mark"`; only consider entries strictly after it (the whole file if no
   mark exists yet).

2. **Cluster**: group the entries by `type` plus a normalized signature (same missing
   include, same clang-tidy check, same CMake error class, same recurring `note` theme).
   Count occurrences per cluster. Report the cluster summary to the user before proposing
   anything.

   Skip `session_end` entries entirely — they are lifecycle markers, not friction.
   The journal is append-only, so known noise stays in it forever; read it robustly:
   entries from 2026-07-03 include pre-fix classifier false positives (all-green ctest
   runs labeled `test_failure`), hook self-test entries (`session_id: "test123"`,
   deliberate `/rt-check` smoke failures), and hand-written notes whose `ts` carries
   local wall-clock time with a `Z` suffix. Weigh such entries accordingly instead of
   clustering them as real recurring friction.

   If `--dry-run` was passed: stop here after printing the cluster summary. Do not
   propose changes, do not touch any file, do not append an `evolve_mark`.

3. **Propose one action per cluster**, each exactly one of:
   - **Rule edit** — add/refine a line in `CLAUDE.md` (e.g. a recurring build failure
     traces to an undocumented convention).
   - **Skill edit** — improve `/new-problem`, `/evolve`, or `/rt-check` (e.g. the template
     kept missing an include that every new problem needed).
   - **Hook edit** — adjust a matcher or failure-marker regex in `.claude/hooks/`.
   - **Backlog item** — add a `- [ ]` line to `GOALS.md` (scope, not process).
   - **No action** — explicitly say so for one-off noise; do not force a change per cluster.

4. **Show each proposal as a concrete diff** (old vs. new content) and ask for approval
   per item — never batch-approve. Skip rejected items entirely (no partial application).

5. **Apply approved diffs.** Append a dated section to `EVOLUTION.md`:
   ```markdown
   ## <UTC date>
   - <what changed> — motivated by <N> journal entries of type <type> (cluster summary)
   ```

6. **Append the mark**: add one line to `evolution/journal.ndjson`:
   `{"ts": "<utc-iso8601>", "type": "evolve_mark", "applied": <n>, "proposed": <m>}`
   (UTC from `date -u +%FT%TZ`, not local wall-clock time).

## Rules

- The journal itself is append-only — `/evolve` reads it, never edits or deletes lines
  (a PreToolUse hook also blocks Edit/Write on it).
- `/evolve` is the only path allowed to rewrite `CLAUDE.md`, skills, or hooks based on
  accumulated experience, and only after explicit user approval per item.
- If nothing has accumulated since the last mark, say so and stop — don't manufacture
  proposals to justify running.
