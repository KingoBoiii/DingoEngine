---
name: pr-writeup
description: Write or update a DingoEngine pull request description in the house layout (Closes lines, What changed table, Left out, Needs testing on a real GPU, Run log with Q/Options/Chose) from the branch's commits, and work out which test cases and backends the owner must run on the GPU machine. Use whenever a PR is opened or its description refreshed, when a routine run writes its run log, or when the owner asks for a PR body, a run log, or "what needs GPU testing".
---

# PR write-up

DingoEngine PRs are read by the owner, who merges them and runs the GPU checks on Windows. The description has to tell them, without opening the diff, what changed, what was left out, what you decided on their behalf, and exactly what to run on which backend.

## 1. Gather

Run in parallel:
- `git fetch origin master` then `git log --reverse --format='%h %s%n%b' origin/master..HEAD`: the commits, with bodies.
- `git diff --stat origin/master...HEAD`: the files touched.
- The issues the commits name (`(#N)` at the end of a subject): read each one's title and labels.
- If the branch already has a PR, its current description: keep its phases checklist, any "GPU verification (owner)" section, and anything else the owner wrote, untouched.

Use what this session did for the run log: what was built and run, what passed, what the review agent found, what you decided.

## 2. Work out GPU testing

Every change to code that reaches the GPU needs the owner to run it, because sessions only have llvmpipe on Linux. Docs, plans, CI, scripts and `.claude/` changes need nothing: write **Needs testing on a real GPU: No** and one line saying why.

Otherwise map each touched area to what to run. Test cases are picked with `--test=<substring>` (case-insensitive, first match). List only the rows the diff touches:

| Touched | Run |
|---|---|
| `Graphics/Renderer2D*`, `Font*`, `Utf8.h` | `--test=color`, `--test=texture`, `--test="text test"`, `--test=circle` |
| `Graphics/Renderer3D*`, `Material*`, `Mesh*`, `Pipeline*` | `--test=mesh`, `--test=batch`, `--test=light` |
| `Light*`, `Renderer3D_Lit.glsl`, fog | `--test=light` (plus `--fog`) |
| shadows, `Shadows.glsl`, `Renderer3D_Shadow*`, probes | `--test=shadow` (plus `--shadow=probe`), `--test=anim --anim-shadow` |
| `PostProcess*`, `Post*.glsl`, `ToneMapping.glsl` | `--test=post`, `--test=light --post` |
| `Particle*` | `--test=particle` |
| compute, `GraphicsBuffer*` storage paths | `--test=compute` |
| `Framebuffer*`, `Texture*`, `TextureReadback*`, `Sampler*`, `RenderPass*`, `CommandList*` | `--test=target` |
| `Model*`, `AssimpVfsIO*` | `--test=model`, `--test=asset` |
| `Skeleton*`, `Animator*`, `AnimationClip*`, `Skinning.glsl`, `Scene/Systems/Animation*` | `--test=anim` (plus `--anim=crowd`) |
| `Asset/`, `Asset/Pack/`, VFS | `--test=asset`, `--test=pack` |
| `Audio/` | `--test=bus` |
| `Physics/3D/`, `Scene/Systems/PhysicsSync*`, colliders | `--test=collider`, `--test=hierarchy` |
| `Scene/` hierarchy, transforms | `--test=hierarchy` |
| `Physics/2D/` | `--test=hierarchy --hierarchy=2d`, AngryBirds |
| `Core/Application*`, `Windowing/`, `Core/Input*` | `--test=background`, `--test=cursor` |
| `Graphics/NVRHI/`, `SwapChain*`, `GraphicsContext*` | every test case on every backend |

Then add:
- **Backends**: Vulkan, DX11 and DX12 for anything under `Graphics/`; Vulkan with the validation layer. Name any D3D-specific risk (new HLSL from SPIRV-Cross, a new binding slot, a resource-state change).
- **Examples** whose code changed, or that use the changed feature heavily: Candlewick (shadows, probes, post, particles), Marionette (skinning, events, root motion, particles; `--check` must stay 107/107 or whatever its current count is), others by name.
- **Look at it**: any visual change the checks can't catch, with the exact flags (e.g. `--test=batch --batch-alpha=0.5`) and what the owner should see.

## 3. Write

Title: conventional, `<type>(<scope or group>): <summary>` (e.g. `fix(bug): …`, `feat(enhancement): …`, `docs: …`).

Body, in this order. Leave out a section that would be empty, except **Needs testing on a real GPU**, which is always there:

```markdown
<One or two sentences: what this PR does and why. For a routine run, start with **Group:** `<label>`.>
<If stacked: > **Stacked on #N.** why, and "merge #N first".>

Closes #N
Closes #M

## What changed

| Issue | Commit | Change |
|---|---|---|
| #N | `abc1234` | What it does, in plain words. Mark **API change** or **Behaviour change** where a game would notice. |

<Docs updated: list the docs/ pages and CLAUDE.md or .claude/reference files touched.>

## Phases            <- only when work remains for later runs
- [x] #N: `abc1234`
- [ ] #M

## Left out

- #K: why (wontfix, routine:skip, needs hardware, too vague, another PR closes it).

## Needs testing on a real GPU: Yes|No

- **Vulkan, DX11 and DX12, with validation:** `--test=…`, … Every check should PASS.
- **Look at it:** …
- **Examples:** …

## Run log

**Did:** branch, commits.

**Verification:** what was built (OS, compiler, config), what ran and the pass counts, review-agent result and which of its findings were fixed. Say plainly what was **not** run (D3D11, D3D12, any real GPU; CI runs only on pushes to master, so a PR has none). If nothing was built: "not yet built or run".

**Questions I answered myself:**
- Q: … — Options: …; …; … — Chose: … because …

**Skipped:** PRs and issues passed over, and review Lows not acted on, each with why.

**Next:** what the next run or the owner picks up.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

<session link from the session's attribution instructions>
```

Style:
- Short sentences, plain words, numbers instead of adjectives ("Animation 99 passed", not "all tests pass").
- Commits as short hashes in backticks; issues and PRs as `#N` inside the repo's own PR text is fine (GitHub links them).
- Never claim a result you didn't see. "Not run" is a normal, expected line.
- In the Run log, each self-made decision gets its own Q line, so the owner can overrule it.

## 4. Publish

- Creating a PR happens only when the owner asked for one or the `routine-run` skill says so. Base is `master` unless stacked.
- Updating: replace the description, keeping the owner's sections and the phases checklist (tick boxes, add hashes; never untick).
- A routine run in Mode A posts the run log as a PR **comment** instead of rewriting the description, and only ticks the phase box in the description.
