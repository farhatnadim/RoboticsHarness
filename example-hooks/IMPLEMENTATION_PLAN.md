# Three-ring feedback architecture — implementation plan

As of 2026-10-01. Companion to [ARCHITECTURE.md](ARCHITECTURE.md) (the design)
and [HOOKS_REVIEW.md](HOOKS_REVIEW.md) (the current-state findings).

This plan takes RoboticsHarness from its current single enforcement ring (Claude
Code agent hooks) to the full three-ring architecture, with GitLab CI as ring 2
and Jira as the tracking and telemetry layer, in four phases over roughly eight
weeks.

## Where we are

Ring 0 exists and works; rings 1 and 2 do not exist, and the check logic lives
inside the hook scripts rather than in shared scripts. Seven Claude Code hooks
(`.claude/hooks/`) format and lint edited C++ files, guard the append-only
journal, journal build/test/sanitizer failures, and run the ASan/UBSan debug
suite before the agent may stop. Nothing runs at commit, push, or merge time,
and there is no issue tracker in the loop.

The hooks review found five gaps that Phase 1 closes before any outer ring is
built on top of them:

| # | Gap | Effect | Severity |
|---|---|---|---|
| 1 | `gate_stop.py` derives changed files from `git status` | A commit during the turn empties the list; the suite never runs | High |
| 2 | `format_cpp.py` and `lint_cpp.py` share one PostToolUse matcher | Claude Code runs them in parallel; clang-tidy reads a half-formatted file | High |
| 3 | No guard on `.claude/hooks/*.py` or `settings.json` | The agent can edit its own gates; scripts are re-read on every call | Medium |
| 4 | Journal guard covers `Edit`/`Write` only | `sed -i`, `>` through Bash bypass append-only | Medium |
| 5 | Stop hook step budget 3 × 420 s vs 900 s hook timeout | Timeout kills the hook with no journal entry | Medium |

## Target architecture

```
          scripts/checks/format.sh · lint.sh · test-debug.sh · rt-check.sh
                          (one definition of each check)
               ┌──────────────────┼──────────────────┐
               ▼                  ▼                  ▼
   Ring 0 · agent hooks   Ring 1 · pre-commit   Ring 2 · GitLab CI
   each edit, turn end    commit, push          MR pipeline, master
   touched file           staged diff           full tree, both presets
   fails open             fails open            fails closed (merge blocked)
   seconds                < 1 minute            minutes
         │ friction            │ bypass residue       │ ci_failure   │ Bug
         ▼                     ▼                      ▼ (master)     ▼ (master)
   ┌───────────────────────────────────────┐   ┌─────────────────────────────┐
   │ evolution/journal.ndjson → /evolve    │──▶│ Jira project RH             │
   │ append-only; hooks, CI bot,           │   │ GitLab for Jira Cloud app   │
   │ journal_note.sh write                 │   │ dedup on sha + job          │
   │ clusters → rules, hooks, scripts,     │   │ issue key in branch / MR    │
   │ backlog                               │   │ title / commit → dev panel  │
   └───────────────────────────────────────┘   └─────────────────────────────┘
```

Every ring calls the same scripts; what the rings find flows down to two sinks.
The journal feeds `/evolve`, which improves the policy; Jira holds the work and
the unattended `master` failures. Jira and GitLab are linked only by issue keys.

## State of the art

Every layer this plan needs exists as a mature, documented tool; the only custom
code is the shared check scripts and the thin adapters that call them.

| Layer | Current practice (Oct 2026) | What we take from it |
|---|---|---|
| Agent-side hooks (ring 0) | Claude Code, Cursor, OpenAI Codex and VS Code Copilot all expose lifecycle hooks (session start, prompt, PreToolUse, PostToolUse, stop). Decision contracts differ: Claude Code uses a `hookSpecificOutput` JSON envelope or exit 2; Cursor uses exit codes plus JSON; Codex a top-level `permissionDecision`. All fail open by design. No cross-vendor standard exists ([Speakeasy](https://www.speakeasy.com/resources/ai-agent-hooks)). | Keep the hook scripts as thin adapters and put the policy in `scripts/checks/`, so the same checks can be wired into a second agent later without rewriting them. |
| Git hooks (ring 1) | The [pre-commit](https://pre-commit.com) framework is the de-facto standard: hooks versioned in `.pre-commit-config.yaml`, pinned by `rev`, installed per clone with `pre-commit install`. For C++, [cpp-linter-hooks](https://github.com/cpp-linter/cpp-linter-hooks) ships `clang-format` and `clang-tidy` from Python wheels (no system LLVM needed), auto-detects `compile_commands.json` in `build/`, and pins the clang version independently (`--version=21`). Alternatives: [pocc/pre-commit-hooks](https://github.com/pocc/pre-commit-hooks), [cmake-pre-commit-hooks](https://github.com/Takishima/cmake-pre-commit-hooks). | Use pre-commit with `repo: local` hooks that call our own `scripts/checks/*.sh`, not the upstream clang hooks: Principle 1 requires one definition of each check. cpp-linter-hooks is the fallback if wheel-installed clang is wanted for contributors without LLVM. |
| CI (ring 2) | GitLab merge request pipelines with *Pipelines must succeed* and approval rules as the merge gate; auto-merge and merge trains for serialised integration ([GitLab auto-merge](https://docs.gitlab.com/user/project/merge_requests/auto_merge)). Static-analysis findings surface in the MR via the CodeClimate-compatible `artifacts:reports:codequality` JSON (`description`, `check_name`, `fingerprint`, `location.path`, `location.lines.begin`, `severity`); the CodeClimate template was removed in GitLab 19.0 and the documented approach is to emit the report from your own tool ([GitLab Code Quality](https://docs.gitlab.com/ci/testing/code_quality/)). Inline MR annotations are Ultimate-only; the MR widget is on Free. | One `.gitlab-ci.yml` with stages check → build → test → notify, every job calling `scripts/checks/*.sh`. A small converter turns clang-tidy output into codequality JSON so findings appear in the MR widget on any tier; `ctest --output-junit` feeds `artifacts:reports:junit`. |
| Jira ↔ GitLab | Two integrations exist. The Atlassian-maintained Jira DVCS connector syncs branches, commits and MRs with up to 60-minute lag and no builds or deployments. The GitLab-maintained *GitLab for Jira Cloud* app syncs in real time and adds builds, deployments, feature flags and branch creation from a Jira issue. Both link by issue key in branch name, MR title/description, commit message, or Smart Commit command, up to 500 keys per entity ([GitLab Jira development panel](https://docs.gitlab.com/integration/jira/development_panel/); [Marketplace listing](https://marketplace.atlassian.com/apps/1221011/gitlab-for-jira-cloud)). All GitLab tiers. | Install the GitLab for Jira Cloud app (not the DVCS connector) so pipeline status appears on the Jira issue. Enforce an issue key in the branch name with a push rule and in the commit message with a `commit-msg` hook. |
| Failure → ticket automation | Trigger from GitLab pipeline webhooks rather than polling; create the ticket through a Jira Automation incoming-webhook rule or the Jira REST API; deduplicate on commit SHA + job name (not pipeline id, which re-tickets every retry); ticket only default-branch and release failures (MR failures are already in front of the author); map severity from branch ([Deviera](https://deviera.dev/blog/gitlab-ci-automation)). | Exactly this, implemented as a `when: on_failure` job on `master` that posts to a Jira Automation webhook and appends the same failure record to `evolution/journal.ndjson`, so `/evolve` sees CI failures alongside ring-0 ones. |

## Phase 1 — Shared check scripts and ring-0 fixes (weeks 1–2)

Phase 1 produces the single definition of each check and repairs the five
ring-0 gaps, so that rings 1 and 2 are built on a correct foundation.

- [ ] Create `scripts/checks/format.sh`, `lint.sh`, `test-debug.sh`, `rt-check.sh`. Each accepts an optional file list (no list = whole tree), exits non-zero on findings, and prints findings in the `path:line:col: message [check]` form the hooks already parse. `lint.sh` also writes `build/debug/lint.codequality.json` when `--codequality <path>` is passed.
- [ ] Rewrite `format_cpp.py`, `lint_cpp.py`, `gate_stop.py` as adapters that call the scripts; keep stdin parsing, journal writing and exit-code behaviour unchanged. Re-run `example-hooks/demos/*` to prove identical behaviour.
- [ ] Fix finding 2: register one PostToolUse command for `Edit|Write|MultiEdit` that runs format then lint sequentially.
- [ ] Fix finding 1: stamp `<HEAD>\n<digest>`; when `git status` is clean, run the suite if `HEAD` differs from the stamped `HEAD`.
- [ ] Fix findings 3 and 4: generalise `guard_journal.py` into `guard_paths.py` covering `evolution/journal.ndjson`, `.claude/hooks/`, `.claude/settings.json`, and the harness-native skills, on `Edit|Write|MultiEdit` and on `Bash` (regex for `>`, `>>`, `sed -i`, `tee`, `truncate`, `mv`, `cp`, `git checkout --` against those paths). Override: `HARNESS_ALLOW_SELF_EDIT=1`, set only by `/evolve` after approval, and journaled.
- [ ] Fix finding 5: one total budget of 840 s split across configure/build/test; journal a `gate_timeout` entry before giving up.
- [ ] Update `example-hooks/README.md` and `HOOKS_REVIEW.md` (mark findings closed).

**Gate:** all demos pass, `ctest --preset debug` green, and a deliberate
`git commit` of a red change is still blocked at Stop.

## Phase 2 — Ring 1: versioned git hooks (week 3)

Ring 1 is a `.pre-commit-config.yaml` of `repo: local` hooks that call the
Phase 1 scripts on the staged diff.

| Hook stage | Script | Scope | Budget |
|---|---|---|---|
| `pre-commit` | `format.sh --check`, `lint.sh` | staged C++ files | < 30 s |
| `commit-msg` | `jira-key.sh` | message must contain `[A-Z]+-[0-9]+` or `NOJIRA:` | < 1 s |
| `pre-push` | `test-debug.sh` | whole tree, debug preset with ASan/UBSan | < 5 min |

- [ ] Add `.pre-commit-config.yaml` with the three stages; pin `pre-commit` and document `pre-commit install --hook-type pre-commit --hook-type commit-msg --hook-type pre-push` in `README.md`.
- [ ] Add `tools/bootstrap.sh`: installs pre-commit (pipx), configures the debug preset so `compile_commands.json` exists, runs `pre-commit run --all-files` once.
- [ ] Decide clang provenance: system LLVM (current) or `cpp-linter-hooks` wheels. Default: system LLVM pinned to one major version in `scripts/checks/toolchain.env`, checked by every script; wheels are the documented fallback.
- [ ] Record `--no-verify` usage: `pre-push` writes a `bypass` journal entry when it detects `SKIP`/`--no-verify` on the previous commit (residue, per Principle 3).

**Gate:** `pre-commit run --all-files` green on `master`; a staged clang-tidy
finding blocks the commit with the same message text ring 0 shows.

## Phase 3 — Ring 2: GitLab CI (weeks 4–5)

Ring 2 is one `.gitlab-ci.yml` whose every job calls a Phase 1 script from the
committed tree, plus the merge request settings that make it the fail-closed
authority. The repository moves to GitLab (or is mirrored there) at the start of
this phase; GitHub Actions is not built.

| Stage | Job | Script / command | Runs on | MR surfacing |
|---|---|---|---|---|
| check | `format` | `format.sh --check` | every MR, `master` | job log |
| check | `lint` | `lint.sh --codequality lint.json` | every MR, `master` | `artifacts:reports:codequality` → MR widget |
| check | `parity` | `pre-commit run --all-files` | every MR | job log (proves ring 1 = ring 2) |
| build | `debug-asan` | `cmake --preset debug && cmake --build --preset debug` | every MR, `master` | — |
| build | `release` | `cmake --preset release && cmake --build --preset release` | every MR, `master` | — |
| test | `ctest-debug` | `test-debug.sh` with `ctest --output-junit` | every MR, `master` | `artifacts:reports:junit` → Tests tab |
| test | `rt-check` | `rt-check.sh` (symbol scan + malloc-guard tests) | every MR, `master` | job log |
| notify | `jira-on-failure` | `when: on_failure`, `master` only | `master` | Jira ticket + journal entry (Phase 4) |

- [ ] Write `.gitlab-ci.yml` with the stages above; one Docker image (`ubuntu` + pinned LLVM + Ninja + Eigen) built from `ci/Dockerfile` and pushed to the project registry, so CI and local toolchain versions match `scripts/checks/toolchain.env`.
- [ ] Add `tools/tidy-to-codequality.py` (clang-tidy text → CodeClimate JSON, `fingerprint` = sha1 of path + check + normalised line).
- [ ] Cache `build/debug` and `build/release` keyed on `CMakePresets.json` + `cmake/*.cmake` to keep MR pipelines under 10 minutes.
- [ ] Project settings: *Pipelines must succeed*, *All threads must be resolved*, 1 required approval, `master` protected (no direct push). Merge trains only if more than one contributor merges daily.
- [ ] Push rules: branch name `^(feature|fix|chore)/[A-Z]+-[0-9]+-` or `^docs/`; commit message must match the same key or `NOJIRA:`.
- [ ] Runner: shared SaaS runners first; tagged self-hosted runner when hardware-in-the-loop tests arrive (`tags: [hil]`).

**Gate:** an MR with a deliberate ASan out-of-bounds read is blocked from
merging; the clang-tidy finding from demo 1 appears in the MR widget; `parity`
passes on a tree where ring 1 passes.

## Phase 4 — Jira (weeks 6–7)

Jira becomes the place where work is planned and where CI failures that nobody
is watching land; the evolution journal stays the place where friction is
recorded for `/evolve`. The two are linked by issue keys, not by copying data.

**Project and conventions**

- [ ] One Jira Software project, key `RH`, issue types Epic / Story / Task / Bug, plus a `Harness` component for hook, script and CI work and `Core` / `Problems` components per code area.
- [ ] Migrate `GOALS.md` `- [ ]` items to `RH` issues; keep `GOALS.md` as the agent-readable mirror (one line per open issue, key first), regenerated by a script, never edited by hand.
- [ ] Branch naming `feature/RH-123-short-slug`; MR title `RH-123: …`; Smart Commit commands (`#comment`, `#time`, `#done`) allowed but not required.

**Integration**

- [ ] Install the GitLab for Jira Cloud app (GitLab-maintained, real-time, syncs builds and deployments). Do not use the DVCS connector.
- [ ] Verify the development panel on an `RH` issue shows branch, MR and pipeline status within one minute of a push.
- [ ] Jira Automation rule *CI failure on master*: incoming webhook → create Bug in `RH`, component `Harness`, priority from payload, assignee = last committer (mapped by GitLab username), label `ci-failure`. Deduplicate on `commit_sha + job_name` via a hidden custom field; a repeat updates the existing issue.
- [ ] `jira-on-failure` CI job posts `{commit_sha, job_name, branch, pipeline_url, excerpt}` to that webhook and appends the same record as a `ci_failure` entry to `evolution/journal.ndjson` through a protected-branch commit by the CI bot user (the only writer besides the hooks and `tools/journal_note.sh`).
- [ ] `/evolve` extension: when a cluster is accepted as a backlog item, create the `RH` issue through the Jira REST API (`POST /rest/api/3/issue`) instead of appending to `GOALS.md`; the mirror picks it up.

**Deliberately not automated**

- MR pipeline failures do not create tickets: the author sees them in the MR within minutes.
- Ring-0 and ring-1 failures do not create tickets: they are journal entries for `/evolve`.
- Issue transitions are not driven from CI; a merged MR does not auto-close the issue until a human confirms in review.

**Gate:** a deliberate `master` build failure produces exactly one `RH` Bug with
a pipeline link and one `ci_failure` journal entry; a retry of the same pipeline
produces no second Bug.

## Roadmap

```
 Phase 1 · wk 1–2      Phase 2 · wk 3        Phase 3 · wk 4–5       Phase 4 · wk 6–7
 scripts/checks/*.sh   .pre-commit-config    .gitlab-ci.yml, image  GitLab for Jira app
 hooks → adapters      commit-msg, pre-push  codequality + junit    failure → Bug + journal
 5 review gaps closed  tools/bootstrap.sh    MR merge gate          /evolve → RH issues
        ◆                     ◆                     ◆                      ◆
 demos green;          pre-commit run        ASan MR blocked;       one Bug per master
 commit still gated    --all-files green     parity job passes      failure, none on retry
```

Phases are sequential because each builds on the previous one's scripts and
settings; Phase 2 and the Phase 3 Dockerfile can overlap if a second person is
available. Durations are estimates for one maintainer.

## Risks and open questions

| Risk | Likelihood | Mitigation |
|---|---|---|
| Toolchain drift: local clang-tidy, pre-commit and CI image disagree on LLVM version | High if unmanaged | One `scripts/checks/toolchain.env` pin read by every script and the CI Dockerfile; `parity` fails on mismatch |
| MR pipeline exceeds 10 min; developers merge with `--no-verify` locally and wait on CI | Medium | Build cache keyed on presets; `release` build only on `master` and nightly if still over budget |
| Jira automation creates duplicate or noisy tickets | Medium | Dedup on `commit_sha + job_name`; `master` only; weekly review of `ci-failure` label in month one |
| GitHub → GitLab move breaks the PR workflow and the demos' `git` assumptions | Low | GitLab pull mirror first, CI on the mirror for two weeks, then cut over |
| `/evolve` writing to Jira adds a credential to the agent environment | Medium | Scoped API token for `RH` only, in the CI/CD variable store and the developer's keychain, never in the repo |
| Hardware-in-the-loop tests need a runner SaaS cannot provide | Certain, later | Tagged self-hosted runner; `hil` jobs `allow_failure: false` only on `master` |

Open questions, each needing a decision before the phase that depends on it:

- [ ] Phase 2: system LLVM or `cpp-linter-hooks` wheels for contributors without a local toolchain?
- [ ] Phase 3: GitLab.com SaaS or self-managed? (Code Quality inline annotations are Ultimate-only; the MR widget is enough on Free.)
- [ ] Phase 3: squash-merge policy, and whether merge trains are needed for a single-maintainer repo (default: no).
- [ ] Phase 4: Jira Cloud (required for the GitLab for Jira Cloud app) or Data Center (DVCS connector only, no pipeline status)?
- [ ] Phase 4: should a merged MR transition the issue automatically, or stay manual as proposed?

## Sources

- [GitLab — Jira development panel](https://docs.gitlab.com/integration/jira/development_panel/)
- [Atlassian Marketplace — GitLab for Jira Cloud](https://marketplace.atlassian.com/apps/1221011/gitlab-for-jira-cloud)
- [GitLab — Code Quality](https://docs.gitlab.com/ci/testing/code_quality/)
- [GitLab — Auto-merge and merge trains](https://docs.gitlab.com/user/project/merge_requests/auto_merge)
- [pre-commit framework](https://pre-commit.com)
- [cpp-linter/cpp-linter-hooks](https://github.com/cpp-linter/cpp-linter-hooks)
- [pocc/pre-commit-hooks](https://github.com/pocc/pre-commit-hooks); [Takishima/cmake-pre-commit-hooks](https://github.com/Takishima/cmake-pre-commit-hooks)
- [Deviera — GitLab CI automation: pipeline failures to tickets](https://deviera.dev/blog/gitlab-ci-automation)
- [Speakeasy — AI agent hooks](https://www.speakeasy.com/resources/ai-agent-hooks)
