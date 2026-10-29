---
name: session-handoff
description: "End a DingoEngine session cleanly: write the work done into the plan's As built notes and the PR body (phases checklist, Run log), commit and push that, and print a ready-to-paste handoff prompt so a fresh session picks up exactly where this one left off. Trigger when the owner says \"handoff\", \"session handoff\", \"context is getting full\", \"let's continue in a new session\", \"wrap this up for next time\", \"hand this off\", \"prep a handoff prompt\", \"I need to start a fresh session\", or otherwise signals the session should end and the work resume later. Also when asked to \"summarize this conversation for the next agent\"."
---

# Session Handoff

The goal is to let a completely fresh session, with no memory of this conversation, resume the work without friction or regression.

In this repo state travels in the repository, not in memory (CLAUDE.md "How we work"). Most sessions run in a cloud container whose auto memory (under `~/.claude/projects/` on that machine) is gone when the container is reclaimed. So the durable record is:

1. **The plan's As built notes** (`.claude/plans/YYYY-MM-DD-<topic>-plan.md`): what each phase actually built, which overrides the design.
2. **The PR body**: the phases checklist a later run continues from, and the Run log (`pr-writeup` skill).
3. **The handoff prompt, printed in chat**: task-specific state for resuming this one piece of work.

## Workflow

Do these in order. Don't skip the reflection: a good handoff needs actual thought about what happened, not a rephrasing of the last message.

### 1. Reflect on the conversation

- What was the owner actually trying to accomplish? The real goal, not just the last request.
- What concrete progress was made? Files edited, decisions locked in, questions resolved, commits pushed.
- What's still open, and what would the owner want done next?
- What surprised you: dead ends, gotchas, constraints that weren't obvious, corrections the owner made.

### 2. Write the state into the repository

- **Plan**: if the work belongs to a plan, add or extend the "As built" notes for the phase worked on: what was built, where (`file:line` or commit), what differs from the design and why, and what is left of the phase.
- **PR**: if a PR exists for the branch, update its body with the `pr-writeup` skill: tick the finished phases, extend the Run log (Did / Verification / Questions I answered myself / Skipped / Next). If no PR exists and the owner hasn't asked for one, put the same Run log at the end of the plan's As built notes instead.
- Commit (`docs(plan): as built for <phase>` or similar, CLAUDE.md "Commits") and push to the session's branch. Uncommitted state is lost with the container.
- If auto memory is available in this session, also save durable repo-wide learnings there (a convention, a gotcha that matters beyond this task). Read it first and don't duplicate entries. Never put task state there.

### 3. Compose the handoff prompt

Write a prompt the owner can paste verbatim into a new session, in this order:

**Goal**: one or two sentences on what the owner is ultimately trying to accomplish.

**Progress so far**: concrete work completed, with file paths, commits and decisions. Point at the plan's As built notes and the PR rather than repeating them.

**Next steps**: what's left, in priority order, with enough for the next session to start (the first unchecked phase, the next issue group).

**Key context & gotchas**: constraints the owner emphasised, approaches tried and rejected and why, anything the repo doesn't record.

### 4. Print the handoff prompt in chat

Before it, say in one or two sentences what you committed and pushed (and saved to memory, if anything). Then output the prompt in a single fenced code block at the very end of the response, with nothing after it.

## Handoff prompt template

Adapt headings if the work doesn't fit neatly, but keep the sections.

```
You're picking up a session that was handed off because the prior context was getting long. Read this fully before acting.

## Goal
<one or two sentences on the real goal>

## Progress so far
- <concrete thing done, with file path, commit or decision reference>
- <…>
- State: plan `.claude/plans/<file>` As built notes, PR #<n> phases checklist and Run log, branch `<branch>`.

## Next steps (in order)
1. <next action, with enough detail to start>
2. <…>

## Key context & gotchas
- <non-obvious constraint, preference, or dead end>
- <…>

```

## Principles

**The repo is the memory.** Anything the next session needs beyond this one task goes into the plan, the PR or CLAUDE.md's reference files, all of which survive the container. The handoff prompt stays lean.

**Don't summarize for its own sake.** Skip whatever doesn't help the next session act.

**Be concrete.** "Finish the shadows phase" is useless; "P4's cascade refit is in `Renderer3D.cpp` `PrepareShadowCascades`, the `--shadow=budget` check still fails on the third cascade" is actionable.

**Preserve the owner's voice on open questions.** If the owner was leaning a direction but hadn't decided, say so.
