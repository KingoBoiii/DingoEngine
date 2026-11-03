# session-meter

A terminal UI mod, not a skill: a Claude Code plugin (`.claude-plugin/plugin.json` plus the hooks module named in `hooks/hooks.json`) that draws a band above the prompt with two bars, the context window's fill and the prompt cache's time left.

- Claude Code loads it automatically from the project's `.claude/skills/<name>` folder, so it loads only when the session's primary working directory is the repository root. A session started in a subfolder doesn't show it.
- The cache TTL bar defaults to `1h`. Set the `cacheTtl` option (`1h` or `5m`) in `/plugin` to match the session.
- The test is run by hand from the repository root: `claude plugin test .claude/skills/session-meter`. It runs `tests/meter.test.tsx`, which imports `claude-code/testing`. No CI job runs it.
- Check the module after an edit with `claude plugin validate .claude/skills/session-meter`.
