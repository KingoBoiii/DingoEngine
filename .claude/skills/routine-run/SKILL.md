---
name: routine-run
description: Do one autonomous maintenance run on DingoEngine — either the next unfinished phase of an open PR (Mode A) or one group of open GitHub issues (Mode B) — claiming the work, implementing it as code, verifying it, pushing, and writing the run log. Use when the scheduled routine fires, or when the owner says "do a routine run", "take the next issue group", "continue PR #N's next phase", or names an issue group or PR to work this way.
---

# Routine run

You are working on KingoBoiii/DingoEngine in a Linux cloud sandbox with no GPU and no Windows toolchain. Read CLAUDE.md first.

Each run does exactly **one unit of work**: one phase of one PR, or one issue group. The owner expects **working code** from every run, not plans. Nobody answers questions during a run: whenever you would ask, pick the option you judge best and record it in the run log as `Q: … — Options: … — Chose: … because …`.

If the run was started with an argument (a PR number, an issue label, or issue numbers), work that instead of choosing in sections 3 and 4. Sections 0, 2 and 6 still apply.

## 0. Off-limits — check first, every run

- A PR or issue labelled `routine:skip` belongs to the owner. Never claim it, check out its branch, push to it, comment on it or edit it, and never remove the label. Its branch is off-limits too.
- Issues such a PR closes or fixes (`closes #N` / `fixes #N`, or it says it fixes them) are off-limits too. An issue it merely mentions is not.

## 1. Tooling

- GitHub GraphQL is blocked here, so `gh pr list/view` and `gh issue list` fail with 403. Use the GitHub MCP tools, or REST through `gh api repos/KingoBoiii/DingoEngine/...`. If you can't read PRs and issues or can't push at all, stop and report that as the run result.
- Claim label: `routine:claimed`. Create it if missing (colour `FBCA04`, description "A scheduled agent is working on this").

## 2. Claiming — mandatory, so two runs never take one item

- Before any work on a PR or issue, add `routine:claimed` to it and comment `🔒 Claimed by routine run <UTC timestamp> for <phase or group>`.
- Skip anything already carrying `routine:claimed`, unless its latest claim comment is more than 5.5 hours old (a crashed run). Then comment `Stale claim from <timestamp> taken over` and claim it yourself.
- For an issue group, claim every issue in it, and the PR once it exists.
- When you finish, **always** remove the label from everything you claimed, also when you fail or give up (then say why in a comment). Keep every other label the item had.

## 3. Mode A — open PRs with unfinished phases (always check first)

1. List open PRs, dropping every `routine:skip` one. For each, find its phases: a `- [ ]` / `- [x]` checklist in the description, or a linked `.claude/plans/*.md` whose phases have checklists or "done" notes. A PR without phases is ignored.
2. A PR is **workable** when it has an unfinished phase, isn't claimed or `routine:skip`, and neither its description nor its plan says the next step needs a GPU, a Windows machine, other hardware, or the owner. Record every PR you skip and why.
3. Take the workable PR with the lowest number and claim it.
4. `git fetch origin` and `git checkout -B <branch> origin/<branch>`. Init submodules only if you need them to build.
5. Read the plan in full, including its "As built" notes (they override the design), plus the CLAUDE.md reference file and `docs/` page for the area you touch.
6. Implement exactly the **next** unfinished phase, never more than one, in full, as code. Only if it truly can't fit in this run, stop at a coherent sub-step, commit it as `<type>(<scope>): <phase> part 1 — …`, and leave the box unticked with a note of what remains.
7. Verify (section 5), then commit and push (section 6).
8. Tick the phase's box in the PR description with the short commit hash (add "*not yet built or run*" if it wasn't), and update the plan's "As built" notes in the same commit when the plan uses them.
9. CI runs only on pushes to master, so a PR branch has none to wait for; say so in the run log.
10. Post the run log as a PR comment (section 7).

## 4. Mode B — no workable PR, so open issues

1. List open issues. The REST endpoint also returns PRs: drop entries with a `pull_request` field. Exclude `wontfix`, `routine:skip`, claimed issues, and issues an open PR closes or fixes.
2. **Group by label.** An issue with several labels goes in the group of its *most unique* label, the one carried by the fewest open non-wontfix issues (`enhancement, headstone` → `headstone`). Ties: `bug`, then `headstone`, then alphabetical.
3. **Pick one group**, in this order: `bug`, `headstone`, then the smallest remaining group. A PR holds at most 10 issues; a larger group is split, lowest numbers first, and later runs take the rest. Leave out issues that are too vague or need a GPU or other hardware to be done right, and record why.
4. Branch from the latest `origin/master` as `<type>/<group>-<short-topic>`: `fix/bug-default-graphics-api`, `feat/headstone-fog-vsync-audio-buses`, `feat/enhancement-physics3d-sensors`.
5. **Fix the issues in this run**: as many as fit, in code, one commit per issue (`fix(graphics): default GraphicsParams to Vulkan (#109)`). Keep the matching `docs/` page, CLAUDE.md and `.claude/reference/` file in step when a public API or engine pattern changes (that may be its own `docs:` commit). Keep going until every issue is done or the run is genuinely out of room; a short run that leaves work undone is a failure.
6. **Never open a plan-only PR.** A plan file is allowed only alongside at least one fully implemented issue. If issues remain, the PR gets a `## Phases` checklist: the issues done in this run ticked with their hashes, one unticked phase per remaining issue or small cluster, so later runs continue it through Mode A. Design notes those phases need go in `.claude/plans/<YYYY-MM-DD>-<group>-plan.md`, committed with the first issue's work.
7. Open a PR to `master` titled `<type>(<group>): <summary>`, with its description written by the **`pr-writeup`** skill (Closes lines, What changed, Left out, Needs testing on a real GPU, Run log). Claim the PR.

## 5. Verification

Follow CLAUDE.md "Verifying changes": build Debug on Linux for the projects you touched, run the affected test cases under Xvfb, and the `--check` of any example you changed. CI runs the full smoke test only after the merge, on master.

- The SessionStart hook has already set the toolchain up (CLAUDE.md "Build and run", Cloud sessions): start at `./Generate-Linux.sh`. Only if the hook reported a failure, run `scripts/build-linux-toolchain.sh` yourself.
- If the branch can't build on Linux, review the code very carefully for compile errors on MSVC and GCC, and say plainly in the commit body and the run log: "not yet built or run".
- Before committing, a **fresh sub-agent** (not the one that wrote the code; model and effort from CLAUDE.md "Sub-agent selection": Sonnet medium for ordinary diffs, Opus high or xhigh for engine-internals diffs) reviews the diff for correctness. Fix every High and Medium it finds; fix or explain each Low in the run log.

## 6. Commits, pushes and hard rules

- Exactly one commit per phase (Mode A) or per issue (Mode B); an extra commit only to fix build or CI breakage of that same work, named as such. Conventional format `<type>(<scope>): <description>`, present tense, concise. Never mention Claude or AI in the subject or body text; the session's attribution trailers still go at the end.
- Never force-push, never rebase published history, never push to `master`, never merge a PR.
- Never modify anything under `vendor/`. Never merge, close or delete PRs, issues or branches. Never change repository settings, secrets or labels other than adding and removing `routine:claimed`; never touch `routine:skip`.
- Session title, if you have a tool to set it: `Routine: PR #<n> <phase>` or `Routine: <group> issues → PR #<n>`.
- If nothing is workable in either mode, change nothing and write only the run log as the session's final message.

## 7. Run log — always

Write it with the **`pr-writeup`** skill's Run log section: **Did**, **Verification**, **Needs testing on a real GPU** (Yes/No and exactly what to run), **Questions I answered myself**, **Skipped**, **Next**.

- Mode A: post it as a comment on the PR.
- Mode B: put it in the PR description.
- Always also give it as the session's final message.
