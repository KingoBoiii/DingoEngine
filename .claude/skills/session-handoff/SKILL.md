---
name: session-handoff
description: Summarize the current conversation, distill durable learnings to long-term (auto)memory, and emit a ready-to-paste handoff prompt so a fresh agent can pick up exactly where this one left off. Trigger whenever the user says "handoff", "session handoff", "context is getting full", "let's continue in a new session", "wrap this up for next time", "save what you've learned", "hand this off", "prep a handoff prompt", "I need to start a fresh session", or otherwise signals they want to end the current context window and resume work later. Also trigger when the user asks you to "summarize this conversation for the next agent" or similar.
---

# Session Handoff

The goal of a handoff is to let a completely fresh agent — one with no memory of this conversation — resume the user's work without friction or regression. That means two distinct outputs:

1. **Durable learnings → memory** Reusable knowledge that would help *any* future session: user preferences, project conventions, API gotchas, non-obvious patterns, things you'd want to know the next time you worked on anything in this domain.
2. **Handoff prompt → printed in chat**. Task-specific state for resuming *this specific* piece of work: what the user was trying to do, what's done, what's left, what to watch out for.

The distinction matters. Memory is a long-lived knowledge base that compounds across sessions; the handoff prompt is ephemeral scaffolding for exactly one resumption. Putting task-specific minutiae into memory pollutes it; putting general insights only into the handoff prompt wastes them.

## Workflow

Do these in order. Don't skip the reflection step — a good handoff requires actually thinking about what happened, not just rephrasing the last message.

### 1. Reflect on the conversation

Before writing anything, take a pass through the conversation and mentally answer:

- What was the user actually trying to accomplish? (The real goal, not just the literal last request.)
- What concrete progress was made? Which files were created or edited, which decisions were locked in, which questions were resolved?
- What's still open? What would the user want done next if they had infinite time?
- What surprised you or the user? Dead ends, gotchas, constraints that weren't obvious upfront, corrections the user made, preferences they expressed.
- What's *reusable* beyond this task? A preference ("user wants minimal explanations"), a convention ("this repo uses pnpm, not npm"), a gotcha ("API X silently truncates responses over 10k tokens") — things that would matter in a different session too.

### 2. Write durable learnings to auto memory

**What to save:** only insights that are reusable across future sessions. If you find yourself writing something that only makes sense in the context of this specific task, it doesn't belong here — it belongs in the handoff prompt. 

**Respect existing memory content:** read before writing. Don't duplicate existing entries; extend or refine them.

### 3. Compose the handoff prompt

Write a prompt the user can paste verbatim into a new session. It should contain, in this order:

**Goal** — one or two sentences on what the user is ultimately trying to accomplish. The *why*, not just the *what*.

**Progress so far** — concrete work completed. Reference file paths, decisions made, problems solved. Be specific enough that a fresh agent can orient quickly; concise enough that it doesn't become a dump of the whole conversation.

**Next steps** — what's left to do, ordered by priority. For each, say enough that the next agent knows how to start (not necessarily how to finish).

**Key context & gotchas** — non-obvious things the fresh agent needs to know. Things like: constraints the user emphasized, approaches that were tried and rejected and why, external systems or credentials involved, preferences for tone or format.

**Pointer to memory** — one line telling the next agent to check memory for durable context.

### 4. Print the handoff prompt in chat

Output the handoff prompt inside a single fenced code block so the user can copy it cleanly. Put it at the very end of your response. Before the code block, briefly list (in one or two sentences) what you saved to memory and where, so the user knows what happened. Don't add postamble after the code block.

## Handoff prompt template

Use this structure. Adapt headings if the work doesn't fit neatly, but keep the sections.

```
You're picking up a session that was handed off because the prior context was getting long. Read this fully before acting.

## Goal
<one or two sentences on the real goal>

## Progress so far
- <concrete thing done, with file path or decision reference>
- <…>

## Next steps (in order)
1. <next action, with enough detail to start>
2. <…>

## Key context & gotchas
- <non-obvious constraint, preference, or dead end>
- <…>

```

## Principles

**Err on the side of more memory, less prompt.** If a piece of context would plausibly matter in another session, put it in memory. The handoff prompt should be lean — anything the next agent can re-derive by reading memory or the repo doesn't need to be in the prompt.

**Don't summarize for its own sake.** A handoff isn't a meeting minutes exercise. Skip things that don't help the next agent act. If nothing meaningful happened in a portion of the conversation, don't mention it.

**Be concrete.** "Refactor the auth flow" is useless; "extract `validateJWT` from `middleware/auth.ts` into a shared `lib/jwt.ts` module — we got as far as the extraction but haven't updated the three call sites in `routes/`" is actionable.

**Preserve the user's voice on open questions.** If the user was leaning a particular direction but hadn't committed, say so. Don't launder their uncertainty into false precision.