---
description: Stage changed files, generate a conventional commit message, and push to origin
---

Run `git status`, `git diff` (staged and unstaged), and `git log --oneline -5` in parallel to understand what has changed.

Then:
1. Stage any unstaged changes that are appropriate to commit (use specific file names, not `git add -A`)
2. Generate a concise conventional commit message based on the diff (format: `type(scope): description`)
3. Commit with the message, including the co-author trailer: `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>`
4. Push to origin

If there is nothing to commit, say so and stop.
