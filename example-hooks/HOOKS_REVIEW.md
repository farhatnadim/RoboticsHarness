# Review — RoboticsHarness agent hooks (`.claude/hooks/`, `.claude/settings.json`)

Reviewed at commit `4c79565`. Scope: the seven hook scripts, their registration,
the shared journal writer, and the claims made about them in
`example-hooks/README.md` / `ARCHITECTURE.md`.

Overall: the hook set is well designed and the implementation is careful — the
fail-open convention is applied consistently, every hook swallows its own crashes,
the classifier false-positive fixes are documented, and the content-digest stamp
in `gate_stop.py` is a good solution to the latency problem. The findings below are
ordered by severity. Items 1–3 are real gaps in the guarantees the documentation
claims; the rest are robustness and hygiene.

## 1. `gate_stop.py` is bypassed by committing (High)

`changed_files()` is derived from `git status --porcelain`, and the hook returns 0
when the list is empty. If the agent runs `git commit` as part of its turn (which
`CLAUDE.md` neither forbids nor discourages), the working tree is clean at `Stop`,
no files are reported, and the build/test suite is never run — regardless of
whether it was ever green. The digest does include `HEAD`, but it is never
computed on the empty-list path.

This contradicts the stated guarantee ("gate on facts, not claims"): the one
action that makes the change permanent is the one that disables the gate.

Suggested fix: treat "HEAD differs from the stamped HEAD" as a change. Store
`<head>\n<digest>` in the stamp, and when `git status` is clean, compare `HEAD`
against the stamped `HEAD` and run the suite if it moved. Alternatively, diff
`HEAD` against the last stamped commit (`git diff --name-only <stamped>..HEAD`)
and apply `is_relevant()` to that list.

## 2. `format_cpp.py` and `lint_cpp.py` race on the same file (High)

Claude Code runs all hooks that match one event in parallel, not in registration
order. Both hooks are registered under the same `PostToolUse` matcher, so
`clang-format -i <file>` and `clang-tidy ... <file>` start concurrently on the
same path. Consequences:

- clang-tidy may read a partially rewritten file (clang-format writes in place).
- Line/column numbers in the findings returned to the agent refer to the
  pre-format layout, so the agent's follow-up `Edit` can target the wrong line.
- On a `Write` of a new file, both race against each other and against nothing
  else, so the outcome is nondeterministic run to run.

Suggested fix: register a single command for the matcher that runs them in
sequence (`format_cpp.py && lint_cpp.py`, or have `lint_cpp.py` invoke
`clang-format -i` first). This also makes the "format then lint" ordering
documented in the README actually true.

## 3. "The agent cannot weaken its own gates" is not enforced in ring 0 (Medium)

Nothing prevents an `Edit`/`Write` to `.claude/hooks/*.py` or
`.claude/settings.json`. Hook scripts are re-read on every invocation, so an
edit to `gate_stop.py` takes effect at the very next `Stop`. The only protection
is the `CLAUDE.md` rule that `/evolve` is the sole path — i.e. a prompt, which
is exactly the class of control the architecture document says must not be
relied on. (`settings.json` is partially protected because Claude Code snapshots
hook configuration at session start, but the scripts it points at are not.)

Suggested fix: extend `guard_journal.py` into a general `guard_paths.py` that
vetoes `Edit|Write|MultiEdit` on `.claude/hooks/`, `.claude/settings.json` and
the harness-native skills unless an explicit override environment variable is
set (e.g. `HARNESS_ALLOW_SELF_EDIT=1`, set only by the `/evolve` flow after user
approval). The override leaves residue, consistent with the "escape hatches
must leave residue" principle.

## 4. The append-only journal is only guarded against `Edit`/`Write` (Medium)

`guard_journal.py` runs on file-editing tools only. `sed -i`, `truncate`,
`> evolution/journal.ndjson`, `git checkout -- evolution/journal.ndjson` and
similar through `Bash` are unguarded. The append-only property therefore holds
against accidental edits, not against any Bash command.

Suggested fix: add a `PreToolUse` matcher on `Bash` that exits 2 when the
command string references `journal.ndjson` with a write-shaped operator
(`>`, `>>` outside `tools/journal_note.sh`, `sed -i`, `tee`, `truncate`,
`mv`/`cp` onto it, `git checkout --`). A conservative regex with a documented
override is better than nothing; perfect shell parsing is not required.

## 5. Timeout budget in `gate_stop.py` exceeds the hook timeout (Medium)

`settings.json` gives the Stop hook 900 s. The hook may run three steps
(configure, build, ctest) at `STEP_TIMEOUT = 420` s each, i.e. up to 1260 s. If
the harness kills the hook at 900 s mid-ctest, the result is a non-blocking
hook error: the stop proceeds, the stamp is not written, and nothing is
journaled. Fail-open is the intended convention, but here it is reached by
accident rather than by design and leaves no residue.

Suggested fix: derive step budgets from one total (e.g. total 840 s, configure
120 s, remaining split between build and test), and journal a
`gate_timeout` entry before giving up so `/evolve` can see it.

## 6. `stop_hook_active` disables blocking for *any* red, not the same red (Low)

Once the gate has blocked once, every subsequent failure in the same turn is
advisory, including a new, different failure introduced while fixing the first.
This is a deliberate loop guard and the trade-off is reasonable, but it should
be stated in the README table (currently it says "agent may not end its turn
until the suite is green", which is only true for the first block).

Optional refinement: allow a second block if the failure signature (classified
kind + first marker line) differs from the one that triggered the first block;
cap at two blocks total.

## 7. `capture_failure.py` classifier — minor precision issues (Low)

- `\bFAILED\b` as the last-resort build marker will also fire on any echoed text
  containing the word (commit messages, test names, `grep FAILED` output) when
  the command chain happens to start with a build verb.
- `"error:"` matches `git`'s own `error:` lines in chained commands
  (`cmake ... && git push`).
- `strip_heredocs()` handles `<<EOF` / `<<'EOF'` / `<<-EOF` but not `<<"EOF"`.
- `tool_response` for `Bash` is treated as `{stdout, stderr}`; the fallback
  (`json.dumps`) covers the string form, which is correct, but then the JSON
  escaping (`\n` as two characters) means the 500/1500-char windows cover less
  text than intended.

None of these is worth blocking on; they are the kind of thing `/evolve` is
designed to tune. Documenting the known false-positive classes next to the regex
(as was done for the 2026-07-03 fix) is sufficient.

## 8. `_journal.py` — partial-write and portability (Low)

- `os.write()` can return fewer bytes than requested; the result is discarded.
  For a 2 KB line on a local filesystem this does not happen in practice, but a
  loop until all bytes are written costs three lines and removes the caveat.
- `O_APPEND` atomicity is a POSIX property of the file offset; interleaving of
  two large concurrent writes is still theoretically possible on some
  filesystems (NFS in particular). If the journal is ever on a network mount,
  switch to `fcntl.flock`. Not a concern for the current single-machine use.

## 9. Path handling edge cases (Low)

- `gate_stop.changed_files()` parses porcelain v1 output and strips a single
  pair of quotes. Paths with spaces, non-ASCII, or escape sequences are
  C-quoted by git and will be mis-parsed. Use `git status --porcelain=v1 -z`
  and split on NUL, or `git diff --name-only` + `git ls-files --others
  --exclude-standard` (also `-z`).
- `changed_files()` assumes `CLAUDE_PROJECT_DIR` is the git root. If the
  harness is opened from a subdirectory, porcelain paths (relative to the
  repo root) will not resolve against `project`.
- `format_cpp.py` tests `os.path.isfile(path)` on the raw (possibly relative)
  path; `lint_cpp.py` correctly absolutises first. Harmless today, but make
  them consistent.
- `SKIP_PREFIXES` includes `plans/`, which does not exist in the repo.

## 10. Journal schema consistency (Low)

`static_analysis` entries omit `session_id`, `cwd` and `cmd`; `gate-stop`
entries set `cwd` to `""`. `/evolve` tolerates this, but a single schema
(`ts, type, session_id, cwd, cmd, excerpt, tags` always present, extra fields
per type) would make clustering and any future tooling simpler. The lint hook
has `session_id` available on stdin and could include it at no cost.

## 11. Registration hygiene (Informational)

- `MultiEdit` has been folded into `Edit` in recent Claude Code releases; the
  matcher entry is harmless and can stay for backward compatibility.
- `format_cpp.py` relies on the default hook timeout (60 s) while its internal
  `clang-format` timeout is 30 s — fine.
- The hooks import `_journal` via the implicit script-directory `sys.path`
  entry. The demos rely on this detail (copying the hook to a temp dir so the
  stub wins). It works, but a comment in `_journal.py` noting that the import
  path is load-bearing for the demos would save someone a surprise.

## What the documentation claims vs. what the code does

| Claim (README / ARCHITECTURE) | Status |
|---|---|
| Stop gate "reruns the sanitized suite itself; never asks the agent" | True, except after a commit (finding 1) |
| "format then lint" ordering on each edit | Not guaranteed — hooks run in parallel (finding 2) |
| "The agent cannot weaken its own gates" | Prompt-enforced only in ring 0 (finding 3) |
| Journal is append-only | Against `Edit`/`Write` only (finding 4) |
| "Agent may not end its turn until the suite is green" | First block only; thereafter advisory (finding 6) |
| Stamp makes repeated stops cost ~0.04 s | True |
| Every hook fails open on missing toolchain / own crash | True, consistently applied |

## Suggested order of work

1. Finding 2 (sequential format→lint) — a one-line `settings.json` change.
2. Finding 1 (HEAD-aware stamp) — small, closes the largest hole.
3. Findings 3 and 4 (path guard for hooks/skills; Bash guard for the journal)
   — one new hook, shared logic.
4. Finding 5 (timeout budget) and the README corrections for finding 6.
5. Everything else via `/evolve` as it surfaces.
