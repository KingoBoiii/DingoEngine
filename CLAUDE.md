# DingoEngine — Claude Context

DingoEngine is a C++20 game engine on NVRHI, targeting Vulkan (primary), DirectX 11 and DirectX 12, on Windows and Linux. User code lives in `Layer` subclasses pushed onto an `Application`-owned `LayerStack`; games are usually thin layers driving Scene/ECS scripts. Everything is in namespace `Dingo`.

## How we work

- **Roles.** The owner (KingoBoiii) sets direction, decides design, merges every PR and runs the GPU checks on Windows (RX 7900 XTX, VS 2026, Vulkan/DX11/DX12). Claude writes most code in Linux cloud sessions without a GPU (llvmpipe under Xvfb).
- **Where work comes from.** Plans in `.claude/plans/YYYY-MM-DD-<topic>-plan.md` (a `**Status**:` first line, the owner's decisions on top, "As built" notes override the design; one phase per session), or GitHub issues by label (`bug`, `enhancement`). The routine takes both through `routine-run`.
- **Branches.** `feat/<topic>`, `fix/bug-<topic>`, `feat/enhancement-<topic>`, `vX.Y.Z` for point releases; an assigned `claude/…` branch wins. Bring master in with a merge commit; never rebase or force-push a branch someone else has. Stack a PR only when both rewrite the same code, and say so at its top.
- **Commits.** Conventional, lowercase `type(scope): summary`, one per issue ending in `(#N)` (scopes: the `commit` skill).
- **PRs.** Only when the owner asks or a routine runs. Body via `pr-writeup`; the owner adds GPU results via `pr-gpu-test`.
- **Decisions.** With the owner away, decide and record it under the PR's *Questions I answered myself*; with them around, public-behaviour or plan-design changes go to them first.
- **Review.** Before the final push one fresh review agent reads the whole diff: fix every High and Medium, fix or explain each Low in the PR. A milestone gets `.claude/reviews/YYYY-MM-DD-<name>-review.md`; open findings become issues.
- **Docs travel with the code.** A public API change updates its `docs/*.md` page in the same PR; a changed engine pattern, its `.claude/reference/` file. A bug found in passing becomes an issue, not a TODO.
- **Showcases.** Each milestone ships or upgrades an `examples/` project using the feature, often with a `--check` mode.

## Keywords

| The owner says | Do this |
|---|---|
| **done** | "I merged the PR. Mark this session done and clean it up." Fetch and confirm the PR is merged (if it isn't, say so and stop). Then unsubscribe from its PR activity, cancel any pending check-in, delete the merged branch locally and on origin if it's still there, reply with one line naming the PR and merge commit, and archive the session if the tool exists. |
| **handoff** | Run the `session-handoff` skill. |

## Build and run

- **Windows**: `./vendor/premake/bin/premake5.exe vs2026` from the repo root, then MSBuild `DingoEngine.slnx` (or one `.vcxproj`) with `/p:Configuration=Debug /p:Platform=x64`. Never run `Generate-Windows.bat` from a non-interactive shell: it ends in `PAUSE`.
- **Linux** (GCC ≥ 13, x86-64): `./Generate-Linux.sh`, then `make -j"$(nproc)" config=debug [Project]`. Outputs go to `build/bin/<Config>-linux-x86_64/`.
- **Configs**: `Debug`, `Debug-ASan`, `Release`, `Distribution`. `--profile` on premake compiles Tracy in.
- **Requirements**: `VULKAN_SDK` set. On Linux it must be LunarG's tarball or `scripts/build-linux-toolchain.sh`'s build of the same pieces (Ubuntu's headers and shaderc are too old), plus the static assimp 6.0.4 in `vendor/assimp/lib/linux-x86_64/`.
- **Cloud sessions**: the SessionStart hook (`.claude/hooks/session-start.sh`) installs the apt packages, inits the submodules, builds the toolchain once (sdk.lunarg.com is blocked there; the container is cached after the hook), copies assimp into place and sets `VULKAN_SDK` and `VK_DRIVER_FILES` (llvmpipe). Start at `./Generate-Linux.sh`. If its start-up line says the setup failed, read the log it names; don't rebuild the toolchain by hand unless that fails too.
- **Run**: the engine is a static lib, and the examples and `Dingo-TestFramework` (the start project) are executables. Run an example with its source directory as the cwd, so `assets/` resolves. The test app runs from `test/` and takes `--test=<name substring>` to start on one case, e.g. `--test=shadow`.
- More (Linux packaging and the tester bundle, ASan, profiling, versioning, devices without a GPU): [.claude/reference/build-details.md](.claude/reference/build-details.md).

## Verifying changes

The smallest check that proves the change. CI runs only on pushes to master; a PR gets `lint.yml` (syntax-only compile of changed files) and, if it touches `include/` or `docs/`, `API reference` (Doxygen, fails on any warning).

- **Docs, comments, plans, CI or scripts no build runs**: run nothing, set up no toolchain. For `include/` or a guide: `mkdir -p build/docs && doxygen docs/Doxyfile` leaves `build/docs/warnings.txt` empty.
- **Code changes**: the build-and-test steps load from `.claude/rules/verify.md`.
- **Don't** run the full smoke test, other configurations, A/B or mutation runs, or performance captures unless asked; master's CI smoke-tests after the merge.
- **Never claim GPU, D3D11/D3D12 or Windows results**; list them under "Needs testing on a real GPU".

## Sub-agent selection

The main session (Sonnet, medium; `.claude/settings.json`) orchestrates. Roles are project agents in `.claude/agents/` (`README.md` there says why each model); call them by `subagent_type`.

| Need | Agent |
|---|---|
| One known file or symbol | No agent: `Grep`/`Read` |
| Search many files | `code-explorer` (haiku, low) |
| Run a build or test | `build-runner` (haiku, low) |
| One contained change | `implementer` (sonnet, medium); parallel with `isolation: "worktree"` if files don't overlap |
| Design a phase | built-in `Plan`, `model: sonnet`; result in the plan file with `file:line` anchors |
| Design an engine-core change | `engine-designer` (opus, high) |
| Pre-push review | `fresh-reviewer` (sonnet, medium) or the `code-review` skill |
| Review engine internals | `engine-reviewer` (opus, xhigh; `effort: high` without renderer, threading or synchronisation) |
| GPU, Jolt, netcode, save-format, race bugs | `gpu-bug-hunter` (opus, xhigh) |
| Any other bug | `bug-hunter` (sonnet, medium) |
| Milestone or cross-cutting design | `fable-advisor` (fable, high, read-only), only when the owner asks |
| Claude Code questions | `claude-code-guide` |

Rules the agent files can't carry:
- A self-contained prompt: goal, files, the rule that applies, what to return. Don't delegate and then search the same thing yourself. One agent per independent job; continue it with SendMessage for follow-ups on the same work.
- Never two agents on one file; parallel only when their work doesn't overlap.
- Reports aren't shown to the owner: relay what matters, check code claims before acting, review `implementer` code before committing it.

## Token budget

- **One session, one job**: a plan, each phase, issue group, PR test or release; the plan's "As built" notes and the PR's phases checklist carry state. When done, say so in one line and suggest **handoff** and a new session. Past ~200k context or a day old, stop at the next clean point (phase committed and pushed); `CLAUDE_AUTOCOMPACT_PCT_OVERRIDE=20` in `.claude/settings.json` is that limit: change both together.
- **The model fits the job**: Sonnet by default; for engine internals (the Opus rows above) say so at the start and suggest the owner switch to Opus.
- **Read narrowly**: the area's `.claude/reference/` file, then Grep, then only the lines you need. Never read a source file over 40 KB whole (`find src include test examples -name '*.[ch]*' -size +40000c`).
- **Long output** goes to a scratchpad file; read only errors, warnings in files you changed, and `[FAIL]`, `VUID-` and `LOG ERROR` lines.

## Rules that break things if forgotten

- Never modify `vendor/` (`.claude/settings.json` denies it). `.claude/hooks/guard-git.sh` blocks force pushes, pushes to `master` and rebases of a pushed branch.

The rest load from `.claude/rules/` when a matching file is touched: C++, public API, build and shipping, verification, rendering, shadows and post, animation, DingoUI, engine invariants.

## Comments — keep them to a minimum

Do not write code comments unless absolutely necessary. A comment must earn its place by stating a load-bearing "why" — a constraint, invariant, or non-obvious consequence the code cannot express itself. Never narrate what the next line does, never leave change-tracking or review commentary ("added X", "fixed Y"), and apply the same restraint to scripts, YAML, shaders, and example code. When in doubt, leave it out.

The `///` documentation rule for `include/DingoEngine/` loads from `.claude/rules/public-api.md`.

## Reference — read when the task needs it

Read the area's file before changing it. Each has one `##` heading per topic and an index on top: Grep the heading, Read that range.

| File | Contents |
|---|---|
| [.claude/reference/engine-patterns.md](.claude/reference/engine-patterns.md) | Application, layers, input, assets, audio, events |
| [.claude/reference/scenes-ecs-physics.md](.claude/reference/scenes-ecs-physics.md) | Scenes, components, hierarchy, physics |
| [.claude/reference/rendering.md](.claude/reference/rendering.md) | Shaders, renderers, lighting, animation, post, shadows, Vulkan/NVRHI |
| [.claude/reference/build-details.md](.claude/reference/build-details.md) | Packaging, executables, profiling, ASan, versioning |
| [.claude/reference/project-history.md](.claude/reference/project-history.md) | Source tree, examples, releases |
| `.claude/skills/session-meter/`, `usage-meter/` | Mods, not skills, loaded only when the session starts at the repo root: meter bands (README in each) |
| [ROADMAP.md](ROADMAP.md) | v0.1 → v1.0 plan, out-of-band modules |
| [LINUX-SUPPORT.md](LINUX-SUPPORT.md) | Linux status; why the toolchain is built from source |
| `docs/` | One page per feature (`docs/README.md`) |
| `.claude/plans/`, `.claude/reviews/` | As built notes win; trust a review's fix-status section, check `git log` |

