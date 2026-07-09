# Hooks in RoboticsHarness — presentation notes

Companion material: [EXAMPLES.md](EXAMPLES.md) (captured failure → auto-correct
walkthroughs) and [demos/](demos/) (re-runnable live demos, safe to run repeatedly).

## 1. What the hooks are and how they're programmed into the harness

Claude Code exposes **lifecycle events** — moments in an agent session where it will
run arbitrary shell commands you register. This repo registers seven Python hooks in
[`.claude/settings.json`](../.claude/settings.json). The mechanism is deliberately
primitive, which is what makes it robust:

- **Registration** — `settings.json` maps an event (`PreToolUse`, `PostToolUse`,
  `Stop`, `SessionStart`, `SessionEnd`) plus an optional tool matcher
  (e.g. `Edit|Write|MultiEdit`) to a command line. Commands reference the repo via
  `$CLAUDE_PROJECT_DIR`, so no absolute paths are baked in.
- **Input** — each hook receives one JSON object on stdin describing the event: the
  tool's input (file path, command line), the tool's response, session id, cwd.
- **Output protocol** — the exit code is the contract:
  - `0` — proceed silently (for `SessionStart`, stdout is injected into the agent's
    context — that's how the `/evolve` nudge reaches the model).
  - `2` — *intervene*: block the tool call (`PreToolUse`), block the agent from
    ending its turn (`Stop`), or hand stderr back to the agent as feedback
    (`PostToolUse`). **stderr becomes model input** — this is the channel that makes
    auto-correction work.
  - anything else — hook error, ignored (never traps the session).

### The seven hooks, in lifecycle order

| Hook | Event (matcher) | Role | On failure |
|---|---|---|---|
| `session_start.py` | SessionStart | Nudges toward `/evolve` when ≥10 unprocessed journal entries piled up | never fails (stdout → context) |
| `guard_journal.py` | PreToolUse (Edit\|Write\|MultiEdit) | Vetoes edits to the append-only `evolution/journal.ndjson` **before** any byte is written | exit 2 → tool call blocked, agent redirected to `tools/journal_note.sh` |
| `format_cpp.py` | PostToolUse (Edit\|Write\|MultiEdit) | clang-formats every C++ file the agent touches | never fails (silent auto-correct) |
| `lint_cpp.py` | PostToolUse (Edit\|Write\|MultiEdit) | clang-tidies the edited file (headers via a TU that includes them) | exit 2 → findings fed back, agent fixes; findings journaled |
| `capture_failure.py` | PostToolUse (Bash) | Classifies failed build/test/sanitizer output and journals it for `/evolve` | never blocks (observability, not policy) |
| `gate_stop.py` | Stop | Rebuilds and runs the full ASan/UBSan debug suite if C++/CMake changed | exit 2 → agent may not end its turn until the suite is green |
| `session_end.py` | SessionEnd | Journals a `session_end` marker | never fails |

### One edit's journey through the hooks

```
SessionStart ──▶ session_start.py        (context injection: "/evolve nudge")
      │
agent Edit ───▶ PreToolUse  guard_journal.py ──exit 2──▶ EDIT BLOCKED, reason → agent
      │ exit 0                                                        │
      ▼                                                               │
 file written ─▶ PostToolUse format_cpp.py   (silent reformat)        │
                             lint_cpp.py ────exit 2──▶ findings → agent edits again ─┐
      │                                                               ◀──────────────┘
agent Bash ───▶ PostToolUse  capture_failure.py ──▶ journal (failures, for /evolve)
      │
agent stops ──▶ Stop        gate_stop.py: build + ASan/UBSan ctest
      │              red ──exit 2──▶ STOP BLOCKED, excerpt → agent keeps fixing
      │ green
SessionEnd ───▶ session_end.py ──▶ journal marker
      │
      ▼
evolution/journal.ndjson ──/evolve──▶ improved rules, skills, hooks (EVOLUTION.md)
```

### Design conventions (worth stating in the talk)

- **Policy as code, not as prompt.** `CLAUDE.md` *asks* the model to run tests and
  keep the journal append-only; a model can drift, forget, or rationalize. Hooks are
  deterministic code that runs every time — the model cannot skip them.
- **Never trap the agent.** Every hook is a silent no-op when its tool is missing
  (no clang-tidy, no compile DB, no cmake) and swallows its own crashes
  (`except Exception: exit 0`). An incomplete toolchain degrades enforcement, never
  progress. `gate_stop.py` additionally downgrades to advisory when it already
  blocked once (`stop_hook_active`), so an unfixable failure can't loop forever.
- **Two kinds of hooks.** *Gates* (guard, lint, stop-gate) exit 2 and feed the agent.
  *Sensors* (capture, session markers) only journal — they feed the slower
  self-improvement loop (`/evolve` clusters recurring friction into rule/skill/hook
  changes, recorded in `EVOLUTION.md`).

## 2. These hooks vs. git hooks (pre-commit)

Both intercept a workflow at a checkpoint and can veto it. The difference is *when*
they fire and *who consumes the failure*.

| | Claude Code hooks | git pre-commit hooks |
|---|---|---|
| Trigger | every tool call, turn end, session boundary | only when someone commits |
| Failure consumer | **the agent, mid-task, with full context** | a human, often long after writing the code |
| Auto-correction | yes — exit-2 stderr becomes model input; the agent edits and retries in the same turn | no — the commit aborts; a human fixes manually |
| Feedback latency | seconds after the offending edit | minutes-to-days later, at commit time |
| Coverage | only changes made *through the agent* | every commit path: humans, IDEs, other tools |
| Bypassability | agent has no `--no-verify`; enforced by the harness | trivially skipped: `git commit --no-verify` |
| Granularity | single file edit / single command / turn | the whole staged diff at once |
| Available context | tool input JSON, tool response, session id, lifecycle stage | staged files only |
| Lifecycle reach | session start/end, "agent wants to stop", pre-tool veto | commit/push/merge only |
| Cost profile | seconds added to *every* edit/turn (must stay fast) | one-time cost at commit |

**Advantages of the agent-side hooks**

1. *The failure lands on an author who is still there.* The agent has the file, the
   intent, and the error in one context window and fixes it immediately — this is the
   auto-correct loop git hooks can't have (their "author" may have mentally moved on
   hours ago).
2. *They gate things git never sees*: ending a turn ("definition of done"), a tool
   call before it executes (append-only journal), a failing `ctest` run that was
   never going to be committed.
3. *They can inject context*, not just veto — `session_start.py` pushes the `/evolve`
   nudge into the conversation.
4. *Unbypassable from inside*: the model cannot opt out of its own harness.

**Disadvantages / what git hooks still do better**

1. *Coverage gap*: a human editing in vim, or any non-Claude tool, bypasses every
   agent hook. Git hooks (and CI) see all contributors.
2. *Tool lock-in*: they only exist inside Claude Code sessions; pre-commit is
   universal across editors and machines.
3. *Latency tax on every edit* — a slow hook punishes the whole session, which is why
   `gate_stop.py` stamps green runs (0.04 s when nothing changed) and `lint_cpp.py`
   lints only the touched file.
4. *No history guarantee*: agent hooks keep the *working tree* honest; only
   commit-time/CI checks keep the *repository* honest.

**Bottom line for the talk:** they're complementary layers. Agent hooks give
fast, in-context enforcement with automatic repair; git hooks/CI are the universal
backstop for whatever reaches the repo by any other path. The same checks
(clang-format, clang-tidy, sanitized ctest) can — and arguably should — run in both.

## 3. Failure → auto-correct examples

See [EXAMPLES.md](EXAMPLES.md) for four walkthroughs with verbatim captured output,
and `demos/` for the three re-runnable versions:

```bash
bash archive/example-hooks/demos/demo_1_lint_autocorrect.sh     # clang-tidy catches 0-as-nullptr, agent fixes  (~30 s)
bash archive/example-hooks/demos/demo_2_stop_gate_sanitizer.sh  # ASan OOB read blocks the stop, agent fixes    (~40 s)
bash archive/example-hooks/demos/demo_3_guard_journal.sh        # PreToolUse veto + redirect to sanctioned path (instant)
```

The demos self-revert every injected bug on exit and replace the journal writer with
a printing stub (`demos/_journal.py`), so repeated live runs never pollute the
append-only `evolution/journal.ndjson`.
