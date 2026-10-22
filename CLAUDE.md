# DingoEngine — Claude Context

DingoEngine is a C++20 game engine on NVRHI, targeting Vulkan (primary), DirectX 11 and DirectX 12, on Windows and Linux. User code lives in `Layer` subclasses pushed onto an `Application`-owned `LayerStack`; games are usually thin layers driving Scene/ECS scripts. Everything is in namespace `Dingo`.

## How we work

- **Roles.** The owner (KingoBoiii) sets direction, decides design questions, merges every PR and runs the GPU checks on Windows (RX 7900 XTX, VS 2026, Vulkan/DX11/DX12). Claude writes most of the code, mostly in cloud sessions on Linux with no GPU (Mesa llvmpipe under Xvfb).
- **Where work comes from.** Either a milestone plan or a group of GitHub issues:
  - **Plans**: `.claude/plans/YYYY-MM-DD-<topic>-plan.md`, with the owner's decisions at the top and phases (P1, P2… or A, B…). Each phase appends "As built" notes to the plan, and those override the design. The PR body carries a phases checklist; a later run continues from the first unchecked phase. One phase per session (see Token budget).
  - **Issues**: defects are labelled `bug`, limitations and requests `enhancement`. A run takes one label's group. Leave issues labelled `wontfix` or `routine:skip` alone, and anything that needs hardware the session lacks (multi-GPU, a gamepad).
  - **The scheduled routine** works both through the `routine-run` skill: one PR phase (Mode A) or one issue group (Mode B) per run, claimed with the `routine:claimed` label.
- **Branches.** `feat/<topic>`, `fix/bug-<topic>`, `feat/enhancement-<topic>`, `vX.Y.Z` for point releases. A cloud session's assigned `claude/…` branch overrides these. Bring master in with a merge commit; never rebase or force-push a branch someone else has. Stack a PR on another only when both rewrite the same code, and say so at the top of the PR.
- **Commits.** Conventional, lowercase: `type(scope): summary`, scoped by engine area (`renderer3d`, `asset`, `scene`, `post`, `animation`, `audio`, `graphics`, `core`, `build`, `ci`). One commit per issue, ending in `(#N)`. The body says why, not what.
- **PRs.** Opened only when the owner asks (or by a routine run). Write the body with the `pr-writeup` skill: summary and `Closes #N` lines; **What changed** (table: issue | commit | change); **Left out**; **Needs testing on a real GPU: Yes/No** with exactly what to run on which backend; **Run log** (Did / Verification / Questions I answered myself, as *Q – Options – Chose* / Skipped / Next). The owner appends "GPU verification (owner)" after running it, on Windows, with the `pr-gpu-test` skill.
- **Decisions.** When the owner isn't there to ask, decide and record it in the PR's *Questions I answered myself*. Changes to public behaviour or a plan's design go to the owner first when they are around.
- **Review.** Before the final push, one fresh review agent reads the whole diff. Fix every High and Medium, and fix or explain each Low in the PR. A milestone gets a full report in `.claude/reviews/YYYY-MM-DD-<name>-review.md`; findings left open become issues.
- **Docs travel with the code.** A public API change updates its `docs/*.md` page in the same PR, and a changed engine pattern updates the matching `.claude/reference/` file. A bug found in passing becomes an issue, not a TODO.
- **Showcases.** Each milestone ships or upgrades an example in `examples/` that uses the new feature, often with a `--check` mode.

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

Keep it to the smallest check that proves the change. CI (`build-linux.yml` with the full smoke test, `build-windows.yml`) runs only on pushes to master. A PR gets only `lint.yml`, a syntax-only compile of its changed files with GCC and MSVC (warnings reported, never fatal), and, when it touches `include/` or `docs/`, the `API reference` workflow (Doxygen, fails on any warning). So these steps are its only build-and-run check before the owner runs the GPU backends.

1. **Docs, comments, plans, CI or scripts that no build runs**: nothing to run. Don't set up the toolchain for them. A change to `include/` or a guide runs `mkdir -p build/docs && doxygen docs/Doxyfile` and leaves `build/docs/warnings.txt` empty.
2. **Code**: build Debug on Linux, only the projects you touched (the engine plus the test app or example that uses it). A change to a public header or a `premake5.lua` builds every project (`make -j"$(nproc)" config=debug`), since nothing else catches a broken example before merge. No warnings in files you changed.
3. **Run only the affected feature's test case**: `cd test && timeout 60 xvfb-run -a ../build/bin/Debug-linux-x86_64/Dingo-TestFramework/Dingo-TestFramework --test=<case>`. The app stays open after its checks, so the timeout ends it. Pass = no `[FAIL]`, no `VUID-`, no `LOG ERROR` in the output. If you changed an example that has `--check`, run that too.
4. **Add a check only for new behaviour or a fixed bug**, inside the existing test case (`test/src/Tests/`, helpers in `Tests/TestChecks.h`). Pure engine code (no window or device) gets a doctest case in `test/UnitTests` instead, run with `../build/bin/Debug-linux-x86_64/Dingo-UnitTests/Dingo-UnitTests`.
5. **Don't** run the full smoke test, other configurations, A/B or mutation runs, or performance captures unless asked; master's CI runs the smoke test after the merge.
6. **Never claim GPU, D3D11/D3D12 or Windows results.** List them under "Needs testing on a real GPU" in the PR.

## Sub-agent selection

The main session runs Opus at high effort (`.claude/settings.json`) and orchestrates. Each agent call sets `model` and `effort` from this table:

| Need | Agent | Model | Effort |
|---|---|---|---|
| One known file, symbol or string | No agent: `Grep`/`Read` directly | | |
| Find where something lives across many files (call sites, every reader of a component, every place a format is written) | `Explore` ("medium" or "very thorough") | `haiku` | `low` |
| Mechanical sweeps: check docs against code, update every reference to a renamed API, fill a test case from a clear spec | `general-purpose` | `sonnet` | `medium` |
| Implement a well-specified, contained change (one issue, one phase step, a test, an example's game code) | `general-purpose`, one per change, parallel with `isolation: "worktree"` when the files don't overlap | `sonnet` | `high` |
| Design a phase, a multi-file change, or any engine-core change (render-thread or GPU synchronisation, NVRHI state and barriers, shader bindings, hierarchy and physics sync, animator internals) before writing it | `Plan`, result recorded in the plan file with `file:line` anchors | `opus` | `high` |
| Implement an engine-core change from that design | `general-purpose`, the prompt carries the design | `sonnet` | `high` |
| Pre-push review of the whole diff | One fresh `general-purpose` agent with no context from the work (or the `code-review` skill) | `opus` | `high` (`xhigh` for renderer, threading or synchronisation diffs) |
| Root-cause a hard bug (device lost, validation error, race, a wrong frame) | `general-purpose` | `opus` | `xhigh` |
| Milestone review, or a bug Opus failed on twice | `general-purpose` | `fable` | `high`, only when the owner asks |
| Questions about Claude Code, hooks, skills, the API | `claude-code-guide` | its default | |

Why these picks (prices per million input/output tokens, October 2026):
- **Haiku 5.5** ($0.10/$0.50, a fortieth of Opus) is for reading-heavy work that returns facts, not judgement. Never let it edit engine code or review.
- **Sonnet 5.5** ($2/$10, half of Opus) is the practical model for everyday coding when the task is fully specified: the prompt names the files, the rule, and what done looks like.
- **Opus 5.5** ($4/$20) is for judgement: design, review and root-causing. Engine-core code, where a subtle mistake only shows on a real GPU, gets an Opus design first and an Opus review after, so Sonnet writes it in between. For judgement work, lower Opus's effort before switching to a cheaper model.
- **Fable 5.1** ($10/$50) costs 2.5 times Opus; it is the owner's call.
- Judge cost per finished task: a cheaper agent whose work has to be redone or re-reviewed isn't cheaper. If a Sonnet agent's change fails review twice, redo it on Opus.

Rules for every agent:
- Give it a self-contained prompt: the goal, the files involved, the rule that applies from this file or a reference file, and what to return (a conclusion and `file:line`s, not file dumps).
- Run agents in parallel only when their work doesn't overlap. Never let two agents edit the same file.
- Don't delegate and then search the same thing yourself, and don't spawn an agent for a one-step lookup.
- An agent's report isn't shown to the owner; relay what matters. Check an agent's claims about code before acting on them, and review code a `sonnet` agent wrote before it is committed.

## Sub-agents

- Diff reviews, implementation, mockups and Blender/art: use model "sonnet", effort medium.
- Haiku for search, reading and running commands.
- Opus only for Vulkan/GPU root-cause debugging, Jolt NaN, netcode, save formats and engine internals.

## Token budget

Most of the cost is context re-read on every call (context size × number of calls), not the text you write. Measured: the v0.8.0 animation milestone ran three days in one session, reached 967k context and cost 13 units per prompt; single-phase sessions cost 2–5.

- **One session, one job.** A plan, each phase, each issue group, each PR test and each release get their own session. The plan's "As built" notes and the PR's phases checklist carry the state between them. When a job is done, say so in one line and suggest **handoff** plus a new session. Past ~200k context or a day old: stop at the next clean point (phase committed and pushed).
- **The session model fits the job.** Opus stays the default for engine work. For docs, plan wording, CI and scripts, release bookkeeping, PR write-ups and an example's game code, say so at the start and suggest the owner switch the session to Sonnet.
- **Agents follow "Sub-agent selection" above.** One agent per independent job. Continue an agent with SendMessage for follow-ups on the same work instead of starting a new one.
- **Read narrowly:** the `.claude/reference/` file for the area first, then Grep, then Read only the lines you need. Never read these whole (1.1k–2.3k lines each): `test/src/Tests/Scene/AnimationTest.cpp`, `Graphics/Renderer3D.cpp`, `UI/ImGui/ImGuiDebugPanels.cpp`, `Graphics/Animator.cpp`, `Graphics/Model.cpp`, `Asset/AssetManager.cpp`, `test/src/Tests/Renderer/LightingTest.cpp`.
- **Long command output:** send builds and test runs to a file in the scratchpad, then read only the errors, warnings in files you changed, and `[FAIL]`, `VUID-` and `LOG ERROR` lines.
- **Release notes are what Headstone reads for an engine bump** (`gh release view vX.Y.Z`), instead of this repo. Give each release a **For games** section: breaking API changes with their `docs/` page, behaviour changes a game would notice, and the `headstone` issues it closes.

## Code conventions

| Thing | Convention |
|---|---|
| Namespace | `Dingo` (everything) |
| Members | `m_Name` (instance), `s_Name` (static), `k_Name` or `UPPER_SNAKE_CASE` constants, `UPPER_SNAKE_CASE` macros |
| Backend classes | `Vulkan*`, `DirectX12*`, `Nvrhi*`, `Win*` |
| Log macros | engine `DE_CORE_INFO/WARN/ERROR/ASSERT`; client `DE_INFO/...` |
| Event binding | `DE_BIND_EVENT_FN(fn)`; bit flags via `DE_BIT(x)` |
| Resource creation | fluent `*Params` structs passed to a static `Create()`: `Texture::Create(TextureParams().SetWidth(512)...)` |
| PCH | `depch.h` / `depch.cpp` |

- On-screen text is UTF-8. Only the engine builds with `/utf-8`, so non-ASCII literals in the test app and examples are hex-escaped.
- Public APIs use abstract classes, a static `Create()` and opaque handles (see `Physics3D`). GLM in public headers is fine.
- Prefer `RGBA8_UNORM` for ordinary colour textures.

## Rules that break things if forgotten

**Build and shipping**
- Never modify anything under `vendor/`.
- Every executable that links the engine calls `copyAssimpRuntime()` in its `premake5.lua`, or it dies with `STATUS_DLL_NOT_FOUND` before `main` (on Linux it fails to link). A game that ships packed assets also calls `embedPackKey()` after it and passes `Dingo::Generated::GetPackageKey()` as `ApplicationParams.Assets.PackageKey`. Each example and the test app also calls `exeMetadata { Name = ..., Company = ..., Copyright = ... }` after it, with an `icon.ico` and `icon.png` beside it (`scripts/icons/make-icons.sh`), or it ships without a version resource, icon or desktop entry. Add a new example to `.github/workflows/build-release.yml`'s packaging and the `.vscode` entries too.
- Never leave a `.dpak` in `build/bin/`: the engine mounts `<exe stem>.dpak` at start-up. Re-run premake after changing a `dpak.key`.

**C++ and headers** (MSVC and GCC both have to accept it)
- EnTT, Box2D, Jolt, ImGui, miniaudio and NVRHI never appear in a public header.
- `DE_CORE_ASSERT(cond, msg)` takes a plain string only; format arguments don't compile. `DE_CORE_WARN/ERROR` do take them.
- `<Windows.h>` leaks `min`, `max`, `near` and `far` into test code and public headers. Write `(std::min)(a, b)`, `(std::max)(...)`, `(std::numeric_limits<T>::max)()`, and never name anything `near` or `far`.
- In a public header, a member named after its own type spells the type qualified: `Dingo::Mesh* Mesh`. GCC rejects `Mesh* Mesh`.
- Paths become text only through `Internal::PathToUtf8`/`PathToGenericUtf8`/`PathFromUtf8` (`Core/PathText.h`). Never call `path.string()`/`generic_string()` in the engine, or build a `path` from a `std::string`: on Windows they go through the ANSI code page and throw on a name it can't map. Games and the test app follow the same rule through the public `FileSystem::PathToUtf8`/`PathToGenericUtf8`/`PathFromUtf8`.

**Engine invariants**
- A new built-in component is registered with `DE_INSTANTIATE_COMPONENT` in `src/DingoEngine/Scene/Entity.cpp`, or clients can't use it, and added to `Scene::DuplicateSubtree`'s `CopyComponentIfExists` list (`Scene.cpp`), or duplicates silently drop it. Engine-internal components (`RuntimeComponents.h`, `HierarchyComponent`, `SocketComponent`) are never registered there.
- DingoUI internals (`src/DingoEngine/UI/DingoUI/`) never appear in a public header; `DingoUI.h` holds the params structs and the `DingoUI::` calls only. A texture or font atlas freed while DingoUI draws it goes through the `NvrhiTexture` destroy hook (`DingoUITextureHook.h`), so a new texture-destroying path must call it. `DingoUI.glsl` is an embedded shader (`s_EmbeddedShaders`).
- Public components hold settings only. Never put a live handle (body, controller, sound) on one; those live in `Scene/Systems/RuntimeComponents.h`.
- Caches and lookups key on never-reused ids (`Mesh::GetId()`, `Material::GetId()`, `Framebuffer::GetId()`, `Texture::GetId()`, `Sampler::GetId()`, `Skeleton::GetId()`, `AnimationClip::GetId()`), never on addresses: a freed object's address gets reused.
- The 3D "no body" handle is `k_InvalidBody3D` (0xFFFFFFFF), not 0.
- Scripts never step a character controller and never keep a `CharacterController3D*` across `OnStop`/`OnStart`: controllers are rebuilt on `OnStart`.
- The `AssetManager` owns what it loads: never `Destroy()` or `delete` a managed asset.
- `Application::OnDestroy()` stays in `Run()`'s exit path, never in `Destroy()`, where virtual dispatch no longer reaches the override. GPU teardown goes in `Layer::OnDetach`.
- Any new code that reads a transform as world space goes through `HierarchySystem`.
- A factory returns a ready object or `nullptr`, and `delete` frees everything; engine-only surface is private or under `Internal` (docs/conventions.md). SemVer from 1.0.0 (docs/versioning.md): an API addition waits for a minor release, a removal or rename for a major one.

**Rendering**
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` is workspace-wide: never enable SPIRV-Cross `fixup_clipspace`.
- Bump `k_ShaderCacheFormatVersion` (`NvrhiShader.cpp`) when shader compile options or the toolchain change, and `k_FontAtlasCacheFormatVersion` (`Font.cpp`) when the font charset changes.
- A new engine shader joins `s_EmbeddedShaders` in `EngineShaders.cpp`, or Release builds can't find it.
- The scene UBO (`Renderer3D::CameraData`, mirrored in `Renderer3D_Lit.glsl`) is append-only; custom shaders rely on its first 96 bytes. Material bindings: 0 = scene UBO, 1 = the material's `SetUniform` block, 2+ = textures/samplers, and the binding set must match the shader's reflection exactly.
- A volatile uniform buffer is written in every frame that binds it. If it isn't, Vulkan reads stale data and D3D12 removes the device.
- The swap-chain render pass stays `eLoad`/`eStore` for colour and depth. The Vulkan acquire wait is queued just before the drawing commands execute, never at acquire. Never call `SwapChain::SetVSync` from the main thread while the render thread runs.
- An old Debug executable runs the *current* `Renderer3D_Lit.glsl` from disk; hide the file to A/B old code.

## Comments — keep them to a minimum

Do not write code comments unless absolutely necessary. A comment must earn its place by stating a load-bearing "why" — a constraint, invariant, or non-obvious consequence the code cannot express itself. Never narrate what the next line does, never leave change-tracking or review commentary ("added X", "fixed Y"), and apply the same restraint to scripts, YAML, shaders, and example code. When in doubt, leave it out.

The exception is `include/DingoEngine/`: every public declaration gets a `///` line saying what it promises (decision D7, `docs/conventions.md` "Documentation"). The `API reference` workflow fails a PR that leaves one out.

## Reference — read when the task needs it

Read the relevant file before changing that area. They hold the detail that used to live here.

| File | Contents |
|---|---|
| [.claude/reference/engine-patterns.md](.claude/reference/engine-patterns.md) | Key patterns: entry point, layers, teardown, debug window, background updates, input, VSync, assets and hot-reload, asset packages, audio and buses, events. Also failure contracts, the vendor list and the detail behind the header, `<Windows.h>` and UTF-8 rules |
| [.claude/reference/scenes-ecs-physics.md](.claude/reference/scenes-ecs-physics.md) | Scene lifecycle, scripts, components, lights and fog, the transform hierarchy and sockets, character controllers, runtime handles, physics stepping and queries |
| [.claude/reference/rendering.md](.claude/reference/rendering.md) | Shaders and their cache, Renderer2D/3D, translucency, lighting, lit materials, skinning, the animator, events, root motion, model reload, render targets, post chain, shadows, compute, particles, and the Vulkan/NVRHI rules |
| [.claude/reference/build-details.md](.claude/reference/build-details.md) | Linux build and packaging, the new-executable checklist, profiling, Debug-ASan, versioning, devices without a GPU |
| [.claude/reference/project-history.md](.claude/reference/project-history.md) | The full source tree with every example, where issues and reviews live, and what each release shipped |
| [ROADMAP.md](ROADMAP.md) | The plan, v0.1 → v1.0, and the out-of-band modules |
| [LINUX-SUPPORT.md](LINUX-SUPPORT.md) | Linux status and Appendix A (why the toolchain is built from source; `scripts/build-linux-toolchain.sh` does it) |
| `docs/` | One page per feature (see `docs/README.md`). Keep the relevant page in step with a public API change |
| `.claude/plans/`, `.claude/reviews/` | Milestone plans (their "As built" notes win) and dated review reports (trust the fix-status section, and check `git log`) |
