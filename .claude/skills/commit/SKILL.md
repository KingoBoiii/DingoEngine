---
name: commit
description: Stage changed files, generate a conventional commit message, and push to origin when the argument is `push`
argument-hint: "[push]"
---

Run `git status`, `git diff` (staged and unstaged), and `git log --oneline -5` in parallel to understand what has changed.

Then:
1. Stage any unstaged changes that are appropriate to commit (use specific file names, not `git add -A`)
2. Generate a concise conventional commit message based on the diff (format: `type(scope): description`)
3. Commit with the message, ending it with the attribution trailers this session specifies (`Co-Authored-By:` naming the model actually running, plus `Claude-Session:` when one is given). Never hardcode a model name.
4. Only when the argument is `push` (`$ARGUMENTS`): push to origin

If there is nothing to commit, say so and stop.
