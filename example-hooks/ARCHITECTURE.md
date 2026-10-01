# One policy, three enforcement rings — a layered feedback architecture

This document describes how agent hooks, git hooks, and continuous integration
(GitHub Actions, Jenkins, or any equivalent) are combined so that a developer
receives feedback on a defect at the earliest and least expensive point in the
workflow, and receives it once, rather than first from a local `pre-commit` hook
and then again, in a different form, from a failed pipeline some time later.

## Design problem

An LLM agent changes the economics of code review. It produces plausible code
faster than it can be read, so the bottleneck moves from writing code to
verifying it. Two consequences follow and inform every decision below:

1. **Verification must be deterministic code**, never a prompt or the agent's
   own report. A statement from the model that the tests pass is a claim; an
   exit status of 0 from `ctest` is a fact.
2. **Feedback value decreases with distance from the edit.** Each enforcement
   ring further from the editing moment is slower, has less context, and
   reports to an author who has moved on. Ring 0 corrects an author who still
   has the file and the intent in front of them; CI notifies someone whose
   attention is elsewhere.

## The three rings

```
        policy — defined once, as plain executable scripts
        scripts/checks/{format,lint,test-debug,rt-check}.sh
        ────────────┬──────────────────┬──────────────────┐
                    ▼                  ▼                  ▼
   ring 0      agent hooks        ring 1  git hooks   ring 2  CI
   trigger     every edit / turn      commit / push       PR / merge
   scope       touched file           staged diff         full tree + matrix
   latency     seconds                < 1 minute          minutes
   on failure  stderr → agent,        veto; author        merge blocked;
               automatic correction   fixes immediately   team notified
   covers      agent-made edits       any local commit    anything that reaches
                                                          the repository
   fail mode   open (never traps)     open (--no-verify)  closed (authoritative)
```

| Property | Ring 0 — agent hooks | Ring 1 — git hooks | Ring 2 — CI |
|---|---|---|---|
| Trigger | each tool call, end of turn, session boundary | commit, push | pull request, merge |
| Scope | the edited file | the staged diff | full tree, build matrix |
| Latency | seconds | under one minute | minutes |
| Failure handling | stderr returned to the agent, which corrects and retries | operation vetoed; author fixes locally | merge blocked; team notified |
| Coverage | changes made through the agent | any local commit | anything that reaches the repository by any path |
| Failure mode | open — a missing or broken tool never blocks the agent | open — bypassable with `--no-verify` | closed — the single authoritative check |

## Principle 1 — checks are defined once and invoked three times

The common failure is three independent definitions of "clean": the agent hook
runs clang-tidy with one configuration, `pre-commit` with another, and CI with a
third, so one defect produces three different reports. The remedy is
structural rather than procedural: each check is a plain script that accepts an
optional file list, and **rings differ only in scope and in failure policy,
never in the check itself**. Ring 0 passes the edited file, ring 1 the staged
diff, ring 2 nothing (meaning the whole tree). Upgrading clang-tidy or adding a
check is then a single change that applies to all three rings at once.

## Principle 2 — no new information at commit time

Each ring should detect, earlier and at lower cost, everything the next ring
would detect. If `pre-commit` or CI fails on something ring 0 could have
reported, that is an architecture defect: the correct response is to move the
check inward, not to accept the later report. Outer rings exist for
**coverage** — contributors using other editors, other tools, other machines —
not to introduce new policy.

The legitimate exception is a check that cannot fit an inner ring's latency
budget or environment: a cross-platform build matrix, the release preset,
long-running fuzzing, hardware-in-the-loop tests. These belong only in CI. The
rule is therefore: *all rings share one policy; outer rings add only what cannot
physically run earlier.*

## Principle 3 — the LLM operates inside the guardrails, never as part of them

- **Gate on facts, not on claims.** The Stop gate executes the sanitized test
  suite itself; it does not ask the agent whether the suite was run.
- **The agent must not be able to weaken its own gates.** Hooks call the shared
  check scripts, and CI runs the same scripts from the committed tree, so any
  weakening appears as a reviewable diff rather than as silent drift in a
  prompt. In this repository, `/evolve` additionally requires explicit user
  approval before modifying rules or hooks. (See the note on current state
  below: in ring 0 alone this property is currently enforced by policy, not by
  code.)
- **Escape hatches must leave a trace.** `NOLINT(<check>)` requires a
  justification comment; `--no-verify` defers a check to CI but does not avoid
  it; ring-0 friction is written to the journal. Bypass is permitted; invisible
  bypass is not.
- **Fail open inside, fail closed outside.** Ring 0 performs no action when its
  toolchain is missing and suppresses its own exceptions: an agent blocked by
  broken tooling produces worse results than an unlinted edit. CI is the
  opposite — it is the single fail-closed authority, and the existence of that
  authority is what makes fail-open behaviour safe everywhere else.
- **Latency budgets are an architectural constraint, not a tuning parameter.**
  A slow inner gate causes both humans and agents to disable it or to work
  around it. This is the reason for the content-digest stamp in the Stop gate
  (approximately 0.04 s when nothing has changed) and for scoping lint to the
  edited file. A check that cannot meet a ring's budget is moved to the next
  ring; the ring is not slowed down to accommodate it.

## Blind spots of each ring

| Ring | Blind spot |
|---|---|
| Agent hooks | changes not authored through the agent; per-file scope misses cross-file breakage until the Stop gate |
| Git hooks | unversioned (`.git/hooks`), installed per clone, bypassable with `--no-verify`; sees only the staged diff |
| CI | too late for inexpensive fixes; feedback arrives minutes to hours after the change was made |

Each ring's blind spot is another ring's primary strength. This is the
justification for layering three rings rather than selecting one.

## Current state and retrofit plan

The repository currently implements ring 0 only (`.claude/hooks/`, registered
in `.claude/settings.json`). The check logic is embedded in the hook scripts
rather than extracted into shared scripts, and the ring-0 enforcement of "the
agent cannot weaken its own gates" is currently a `CLAUDE.md` rule rather than
a hook. The steps to reach the full architecture are:

1. **Extract the shared checks** from the hook bodies into
   `scripts/checks/format.sh`, `lint.sh`, `test-debug.sh`, and `rt-check.sh`.
   Rewrite `lint_cpp.py` and `gate_stop.py` as thin adapters that call these
   scripts, preserving current behaviour so that there is a single definition
   of each check.
2. **Ring 1.** Version the git hooks, either with the
   [`pre-commit`](https://pre-commit.com) framework or with a committed
   `tools/git-hooks/` directory enabled via `git config core.hooksPath`.
   `pre-commit` calls the shared scripts on the staged diff; `pre-push` runs
   `test-debug.sh`.
3. **Ring 2.** Add a GitHub Actions workflow (or Jenkinsfile) that runs the
   debug preset with ASan/UBSan and the release preset, `clang-format
   --dry-run` and clang-tidy over the full tree, the `/rt-check` symbol scan,
   and `pre-commit run --all-files` to verify parity with ring 1.
4. **Close the telemetry loop.** Ring 0 already records friction in
   `evolution/journal.ndjson` for `/evolve`. Recurring CI failure classes
   should be written to the same journal so that all three rings improve a
   single shared policy rather than diverging.
5. **Enforce self-protection in ring 0.** Add a `PreToolUse` guard that vetoes
   edits to `.claude/hooks/`, `.claude/settings.json`, and the harness-native
   skills unless an explicit, logged override is set by the `/evolve` flow
   after user approval. Until this exists, Principle 3's second bullet holds
   only for rings 1 and 2.
