# One policy, three rings — an architecture for early feedback

The question this answers: how do you combine agent hooks, git hooks, and CI
(GitHub Actions / Jenkins — the architecture doesn't care which) so a developer
gets feedback at the *cheapest possible moment*, instead of being shouted at by
`pre-commit` and then again, differently, by a red pipeline twenty minutes later?

## The failure mode being designed against

An LLM agent changes the economics of code review: it produces plausible code
faster than anyone can read it, so the bottleneck moves from *writing* to
*verifying*. Two consequences drive everything below:

1. **Verification must be deterministic code**, never prompts or the agent's
   self-report. "The tests pass" from a model is a claim; `ctest` exit 0 is a fact.
2. Every ring outward is slower, sees less context, and shouts at a colder
   author. Ring 0 corrects an author who still holds the file and the intent;
   CI emails someone who has mentally moved on. Feedback value decays with
   distance from the keystroke.

## The three rings

```
        policy — defined exactly once, as plain runnable commands
        scripts/checks/{format,lint,test-debug,rt-check}.sh
        ────────────┬──────────────────┬──────────────────┐
                    ▼                  ▼                  ▼
   ring 0      agent hooks        ring 1  git hooks   ring 2  CI
   trigger     every edit / turn      commit / push       PR / merge
   scope       touched file           staged diff         full tree + matrix
   latency     seconds                < 1 minute          minutes
   on failure  stderr → agent,        veto, author        block merge,
               auto-corrects          fixes now           team notified
   covers      agent-made edits       any local commit    anything that reaches
                                                          the repo by any path
   fail mode   open (never trap)      open (--no-verify)  closed (authority)
```

## Principle 1 — checks are defined once, invoked three times

The classic mess is three drifting definitions of "clean": the agent hook runs
clang-tidy with one config, `pre-commit` with another, Jenkins with a third —
and the developer gets three different shouts for one defect. The cure is
structural, not disciplinary: each check is a plain script taking an optional
file list, and **rings differ only in scope and policy, never in the check
itself**. Ring 0 passes the touched file, ring 1 the staged diff, ring 2
nothing (= whole tree). Upgrading clang-tidy or adding a check is then one
diff that tightens all three rings simultaneously.

## Principle 2 — no new news at commit time

Each ring should catch, earlier and cheaper, everything the next ring would.
If `pre-commit` or CI fails on something ring 0 could have known, that is an
architecture bug — fix it by pushing the check left, not by accepting the
shout. Later rings exist for **coverage** (humans in vim, other tools, other
machines), not for new policy.

The honest exception: checks that are too expensive or environment-dependent
for an inner ring's latency budget — cross-platform build matrix, release
preset, long-running fuzzing, hardware-in-the-loop — legitimately live only in
CI. The rule survives as: *rings share one policy; outer rings add only what
physically cannot run earlier.*

## Principle 3 — the LLM is inside the guardrails, never part of them

- **Gate on facts, not claims.** The Stop gate reruns the sanitized suite
  itself; it never asks the agent whether it did.
- **The agent cannot weaken its own gates.** Hooks call the shared scripts, and
  CI runs those same scripts *from the committed tree* — so any weakening is a
  reviewable diff, not silent prompt drift. In this repo `/evolve` additionally
  requires explicit user approval before touching rules or hooks.
- **Escape hatches must leave residue.** `NOLINT(<check>)` needs a
  justification comment; `--no-verify` only *defers* to CI, never avoids it;
  ring-0 friction is journaled. Bypass is allowed, invisible bypass is not.
- **Fail-open inside, fail-closed outside.** Ring 0 no-ops when the toolchain
  is missing and swallows its own crashes — an agent trapped by broken tooling
  produces worse messes than an unlinted edit. CI is the opposite: it is the
  single fail-closed authority, which is precisely what makes fail-open safe
  everywhere else.
- **Latency budgets are architecture, not tuning.** A slow inner gate trains
  both humans and agents into disabling or thrashing it. Hence the
  content-digest stamp (0.04 s when nothing changed) and lint scoped to the
  touched file. If a check can't fit the budget, it moves out a ring — it
  doesn't slow the ring down.

## What each ring is worst at (why all three exist)

| Ring | Blind spot |
|---|---|
| Agent hooks | anything not authored through the agent; per-file scope misses cross-file breakage until the Stop gate |
| Git hooks | unversioned (`.git/hooks`), per-clone install, `--no-verify`; sees only the staged diff |
| CI | too late for cheap fixes; feedback lands minutes-to-hours after intent has evaporated |

Each ring's blind spot is another ring's core competence — that's the whole
argument for layering rather than picking one.

## Retrofitting this repo (current state: ring 0 only)

1. **Extract the shared checks** out of the hook bodies into
   `scripts/checks/format.sh`, `lint.sh`, `test-debug.sh`, `rt-check.sh`;
   rewrite `lint_cpp.py` / `gate_stop.py` as thin adapters that call them
   (identical behavior, single definition).
2. **Ring 1** — version the git hooks (the [`pre-commit`](https://pre-commit.com)
   framework, or a committed `tools/git-hooks/` dir wired via
   `git config core.hooksPath`), calling the same scripts on the staged diff;
   `pre-push` runs `test-debug.sh`.
3. **Ring 2** — a GitHub Actions workflow (or Jenkinsfile): debug preset with
   ASan/UBSan + release preset, `clang-format --dry-run` and clang-tidy over
   the full tree, the `/rt-check` symbol scan, plus `pre-commit run
   --all-files` to prove ring-1 parity.
4. **Close the telemetry loop** — ring 0 already journals friction into
   `evolution/journal.ndjson` for `/evolve`; recurring CI failure classes
   should feed the same journal, so all three rings improve one shared policy
   instead of three diverging ones.
