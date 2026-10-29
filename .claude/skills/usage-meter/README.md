# usage-meter

A terminal UI mod, not a skill: a Claude Code plugin (`.claude-plugin/plugin.json` plus the hooks module named in `hooks/hooks.json`) that draws an always-on band above the prompt with the 5-hour rate-limit usage, the limit and the countdown to its reset.

- Claude Code loads it automatically from the project's `.claude/skills/<name>` folder, so it loads only when the session's primary working directory is the repository root. A session started in a subfolder doesn't show it.
- It has no tests. Check the module after an edit with `claude plugin validate .claude/skills/usage-meter`; `claude plugin test .claude/skills/usage-meter` runs any `*.test.ts(x)` added later.
