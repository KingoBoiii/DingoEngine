# Agent models

Each file in `.claude/agents/` is one role from CLAUDE.md "Sub-agent selection": its model, effort, tools and rules.

## Why these picks

Haiku returns facts, not judgement, so it searches and runs commands but never edits engine code or reviews. Sonnet is the default for everyday work, the main session included. Opus is reserved for engine internals because a subtle mistake there only shows on a real GPU, so engine-core code gets an Opus design first and an Opus review after, with Sonnet writing it in between; Fable is the owner's call. For judgement work, lower Opus's effort before switching to a cheaper model. Judge cost per finished task: a cheaper agent whose work has to be redone isn't cheaper, and if a Sonnet agent's change fails review twice, redo it on Opus.
