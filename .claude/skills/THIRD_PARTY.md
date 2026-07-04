# Third-party skills — attribution

The skills in this directory come in two tiers:

- **Harness-native** (`evolve/`, `new-problem/`, `rt-check/`): written for this repo;
  maintained exclusively through `/evolve` per CLAUDE.md.
- **Vendored third-party** (everything below): copied from upstream after a content
  vetting pass (grep sweep of every SKILL.md and reference file for prompt-injection /
  exfiltration patterns — all clean). Maintained by re-syncing from upstream, **not**
  through `/evolve`. License texts are in `licenses/`.

Vendored 2026-07-04.

## robotics-agent-skills — 10 skills

- **Source:** https://github.com/arpitg1304/robotics-agent-skills (Arpit Gupta)
- **License:** Apache-2.0 (`licenses/robotics-agent-skills.Apache-2.0.txt`)
- **Commit:** `54f7b578f3dc269d29c0beb623b3f2611fd3a430`
- **Skills:** `robotics-software-principles`, `robotics-design-patterns`,
  `robotics-testing`, `robot-perception`, `robot-bringup`, `robotics-security`,
  `ros1`, `ros2`, `ros2-web-integration`, `docker-ros2-development`
- **Modifications:** none (verbatim copies).

## Jeffallan/claude-skills — 2 skills

- **Source:** https://github.com/Jeffallan/claude-skills
- **License:** MIT (`licenses/jeffallan-claude-skills.MIT.txt`)
- **Commit:** `e8be415bc94d8d6ebddc2fb50e5d03c6e27d4319`
- **Skills:** `embedded-systems` (with `references/`), `cpp-pro` (with `references/`)
- **Modifications:** none (verbatim copies).

## antigravity-awesome-skills — 1 skill

- **Source:** https://github.com/sickn33/antigravity-awesome-skills
  (`skills/arm-cortex-expert`)
- **License:** MIT, repo-level (`licenses/antigravity-awesome-skills.MIT.txt`)
- **Skill:** `arm-cortex-expert`
- **Modifications:** the aggregator's boilerplate frontmatter and intro were replaced
  with a trigger-keyword description; non-standard frontmatter keys
  (`risk`/`source`/`date_added`) and a dangling `resources/implementation-playbook.md`
  reference (never shipped by the aggregator) were removed. The technical body
  (Cortex-M7 memory barriers, DMA/cache coherency, ISR patterns) is verbatim.

## Updating

Re-clone the upstream repo, re-run the vetting sweep, re-copy the skill directories,
and update the commit SHA here. Do not hand-edit vendored skills (except to re-apply
the documented `arm-cortex-expert` cleanup); improvements belong upstream.
