# Known Bugs

Open defects and sharp edges in DingoEngine. Companion to [ROADMAP.md](ROADMAP.md) (what gets built
next) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (missing capabilities, ranked). This file is only
for things that are **wrong or surprising in code that already ships**.

- **Verified against `VERSION` 0.8.0 on 2026-10-03.** Every entry below carries a `file:line` anchor
  confirmed in that pass. Code drifts — re-confirm before fixing, and delete the entry when it's gone.
  K10 and K11 are deliberate deferrals, not oversights; K16 and K17 are the two limits v0.7's
  lighting ships with, K18 a defect found reviewing its lit materials that predates it, and K19 one
  found reviewing Candlewick, all added on 2026-10-01. v0.7 and v0.7.1 closed no entry; v0.7.2
  closed K14 and K20, moved K16's `Renderer3D.cpp` lines, and added K21, found reviewing it. v0.8
  closed no entry. It moved the lines of K16 to K19, corrected K11's swap-chain lines, raised K10's
  header count from 20 to 24 (`Entity.h` came with v0.7.1; `AnimationClip.h`, `Animator.h` and
  `Skeleton.h` with v0.8), and added K22 to K25 on 2026-10-03: the three edges its P7 and P8 reviews
  left in the animator, and the shear the v0.7.1 hierarchy drops.
- **Not a review log.** Findings from a dated review pass live in `.claude/reviews/`; the v0.6.0 pass
  (`2026-07-29-v0.6.0-review.md`) is fully closed out — 4 Critical, 10 High, 12 Medium, 7 refactors and
  21 Lows all fixed — so nothing here comes from it.
- **IDs are never reused.** A fixed entry is deleted rather than renumbered, so gaps are expected:
  fixed in v0.6.2 were K1 (`Application::OnDestroy` never reached a derived override), K2 and K3
  (text could not draw its Latin-1 glyphs and had no UTF-8 decode) and K6 (the `.cache` directory
  followed the working directory). Fixed in v0.6.3 were K4 (a default `AssetHandle` passed
  `IsValidAssetHandle`), K5 (`Debug-ASan` did not link), K7 (`Font::Create` ignored the asset root),
  K8 (assigning a physics component aliased its handle), K9 (`Renderer3D` dropped batch overflow),
  K12 (the swap-chain depth was discarded between render-pass instances) and K13 (the swap-chain
  colour attachment used `LOAD_OP_NONE`). Fixed in v0.7.0: K15 (a long frame dropped 3D bodies
  through their colliders). Fixed in v0.7.2: K14 (the test framework crashed when its window was
  minimized) and K20 (from v0.7.0 a minimized window skipped every layer's `OnUpdate` with no way
  to opt out, so games that pump a network or a simulation there froze; found bumping Headstone to
  v0.7.1, filed and fixed together by `ApplicationParams::UpdateInBackground`). Fixed in v0.8.2,
  both found and fixed together on a machine with no GPU: K26 (DirectX 12 refused WARP, the only
  adapter there, and asserted) and K27 (Vulkan without a driver crashed on a null instance instead
  of reporting it).
- **The codebase carries no `TODO`/`FIXME`/`HACK` markers**, so nothing below came from scavenging
  in-source notes. Every entry was found by reading the code, or by hitting it while building a game on
  the engine.

**Status key** — **Defect**: behaves incorrectly, should be fixed. **Limitation**: behaves as written,
but silently costs correctness or portability. **Latent**: real, but nothing in-tree triggers it yet.

| # | Issue | Kind | Area |
|---|---|---|---|
| [K10](#k10) | GLM is the one third-party dependency that leaks into public headers | Limitation | API |
| [K11](#k11) | Dragging a window to a display driven by another GPU is not handled | Limitation | Vulkan |
| [K16](#k16) | Point and spot lights pop in and out at the light-budget edge | Limitation | Renderer3D |
| [K17](#k17) | Overlapping bright lights clip to white until tone mapping lands | Limitation | Renderer3D |
| [K18](#k18) | A texture created at a freed texture's address is not noticed by a material | Defect | Graphics |
| [K19](#k19) | A wireframe material on a GPU without `fillModeNonSolid` builds a line-mode pipeline the device never enabled | Defect | Vulkan |
| [K21](#k21) | The first Vulkan frame after startup, or after a minimized stretch, is not ordered after its image acquire | Latent | Vulkan |
| [K22](#k22) | Binding an animator to another skeleton drops its open event ranges without their `RangeEnd` | Latent | Animation |
| [K23](#k23) | A disabled `AnimatorComponent` keeps reporting its last update's events | Latent | Animation |
| [K24](#k24) | An animation layer that freezes while its weight is 0 freezes a stale pose | Latent | Animation |
| [K25](#k25) | A reparent that keeps the world transform drops the shear a non-uniformly scaled parent puts on a rotated child | Limitation | Scene |

---

## K10 — GLM is the one third-party dependency that leaks into public headers {#k10}

**Limitation** — 24 headers under `include/`

Every other vendored library is fully hidden from client code — ImGui, EnTT, Box2D, Jolt, NVRHI, GLFW
and miniaudio each appear in **zero** public headers, behind facades and opaque handles. GLM appears in
24, so any game linking the engine must also put GLM on its include path and match its version, and the
engine cannot change math libraries without breaking every consumer.

This is a known, deliberate deferral rather than an oversight. **Fix**: engine-owned vector/matrix
types in the public API, with GLM confined to `src/` — a large, breaking change best done alongside
another API break.

## K11 — Dragging a window to a display driven by another GPU is not handled {#k11}

**Limitation** — `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp:352-370`,
`src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp:87,389`

Device selection is surface-aware at **startup**: the context probes a surface and picks a GPU that can
present to it, covering the hybrid-graphics laptop case. Moving the window *after* that — onto a display
driven by a different GPU — is detected and logged as an error, but not recovered from; the engine
cannot migrate a live device and swap chain to another adapter.

Resolution, DPI and colour-space changes on the same GPU *are* handled (the swap chain recreates from
`currentExtent`, and `OUT_OF_DATE` / `SUBOPTIMAL` trigger recreation).

**Fix**: recreate the device and all GPU resources on adapter change — expensive, and rare enough in
practice that logging may remain the right answer.

## K16 — Point and spot lights pop in and out at the light-budget edge {#k16}

**Limitation** — `src/DingoEngine/Graphics/Renderer3D.cpp:767-796`

When more point and spot lights reach the view than `Renderer3DCapabilities::MaxLocalLights` (32 by
default, and at most 32), `EndScene` keeps the brightest as seen from the camera and drops the rest. The
choice is deterministic — ties go to the light the camera is nearer to relative to its range, then
to the earlier-submitted one, so a still scene picks the same lights every frame — but it is a hard cut: as the camera moves, a light that crosses the budget edge appears or
vanishes at full strength instead of fading. The first overflow logs one warning per renderer lifetime;
`Statistics::DroppedLights` and the F4 Renderer tab show how many were dropped in the latest scene.

Only lights that survive the frustum cull compete, so short ranges and fewer lights in view keep a scene
under the budget.

**Fix** (deferred): a rank-fade band, so a light ranked just inside the budget fades out over a few
ranks instead of switching off.

## K17 — Overlapping bright lights clip to white until tone mapping lands {#k17}

**Limitation** — `src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl:184-188`

The lit shader adds ambient and every light's diffuse and specular and writes the sum straight to the
frame, so any channel above 1.0 clamps. There is no HDR target and no tone mapping before v0.9, so
stacking bright lights, or a bright light on a surface the sun already lights, burns out to white and
loses its colour. The default light adds up to exactly 1.0 face-on, so it is a scene that sums
intensities past 1.0 that clips.

Keep the summed intensity at a surface near 1.0 — lower `Intensity`, or dim the sun in a scene lit by
point lights.

**Fix**: an HDR scene target and a tone-mapping pass in v0.9's post-processing stack
([ROADMAP.md](ROADMAP.md)).

## K18 — A texture created at a freed texture's address is not noticed by a material {#k18}

**Defect** — `src/DingoEngine/Graphics/Material.cpp:75-84`,
`src/DingoEngine/Graphics/NVRHI/NvrhiRenderPass.cpp:40-91`, `include/DingoEngine/Graphics/Texture.h:116-125`

`Material::SetTexture` returns early when the slot already holds the same pointer. `NvrhiRenderPass`
tells a hot-reloaded texture from the one it baked by a per-object generation
(`Texture::GetGeneration()`, bumped by `Reinitialize`), and a new `Texture` starts that count at 0. So a
texture that is freed and replaced by a new one at the same address looks unchanged to both: the
material keeps its cached render pass, and the pass keeps binding the old GPU texture.

Scenario: `AssetManager::Unload` a texture a material uses, `Load` another file so that the new
`Texture` lands at the freed address (a heap-dependent coincidence, so it reproduces unreliably), then
`SetTexture` with it on the same slot. The material goes on drawing the old image. Drawing the
material at all between the `Unload` and the `SetTexture` is a use-after-free read: the cached pass
reads the freed `Texture`'s generation at bind time.

Not new in v0.7: it was deferred, as pre-existing, from the review of v0.7's lit materials. **Workaround**
(documented on `Material::SetTexture`): clear the slot with `SetTexture(slot, nullptr)` before setting
the new texture, which drops the cached passes. **Fix**: draw every texture's generation from one global
counter (or give each a unique id) and compare that instead of the pointer.

## K19 — A wireframe material on a GPU without `fillModeNonSolid` builds a line-mode pipeline the device never enabled {#k19}

**Defect** — `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp:533`,
`src/DingoEngine/Graphics/Material.cpp:188`, `src/DingoEngine/Graphics/NVRHI/NvrhiPipeline.cpp:26`

The Vulkan device requests `fillModeNonSolid` only where the GPU reports it, which is right, but nothing
downstream knows whether it did. `Material` still passes `FillMode::Wireframe` to its pipeline, and
`NvrhiPipeline` maps it to `RasterFillMode::Line` (`VK_POLYGON_MODE_LINE`), which
VUID-VkPipelineRasterizationStateCreateInfo-polygonMode-01507 forbids without the feature. With the
validation layers on, that is an error per pipeline; without them the driver decides: it may draw filled,
draw lines, or crash.

Every desktop GPU that passes the engine's unconditional device features (geometry and tessellation
shaders, BC compression) also reports `fillModeNonSolid`, so in practice only a wireframe material on
unusual hardware is exposed (Candlewick's `--debug-cone` is the only one in-tree). D3D11 and D3D12 have no
such requirement. Found reviewing Candlewick; the fix that enabled the feature (`764a09c`) is not at fault.

**Fix**: record the capability on the graphics context (say `GraphicsContext::SupportsWireframe()`), and
have `NvrhiPipeline` fall back to `Solid` with a one-time warning when it is missing.

## K21 — The first Vulkan frame after startup, or after a minimized stretch, is not ordered after its image acquire {#k21}

**Latent** — `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp:232`,
`src/DingoEngine/Graphics/NVRHI/NvrhiTexture.cpp:138-146`, `src/DingoEngine/Graphics/NVRHI/NvrhiGraphicsBuffer.cpp:76-84`,
`src/DingoEngine/Graphics/Renderer.cpp:210-213`

`AcquireNextImage` queues its semaphore wait on the graphics queue as soon as the image is acquired,
and NVRHI attaches a queued wait to the next submission from any command list. Between the render
thread's acquire and the next frame's `Execute`, the main thread can submit a list of its own: a
texture, or a `DirectUpload` buffer, written outside a frame. That happens for every asset loaded in
`OnAttach`, before the first frame, and since v0.7.2 for async asset loads and textures created in
`OnUpdate` while a minimized app keeps updating (`UpdateInBackground`). That submission takes the wait, so the frame that draws
into the image is not ordered after the acquire.

A second gap shares the cause. When the acquire fails because the window was minimized (a (0,0)
surface), the first frame after the restore records into the old swap chain's image at the stale
index and executes before the queued resize recreates the swap chain. Its present is skipped.

Harmless in practice: the image was acquired long before it is drawn, and the default validation
layers report nothing. Synchronization validation would flag it. Found reviewing v0.7.2; both gaps
predate it.

**Fix**: keep the acquired semaphore pending in the swap chain and queue its wait just before the
frame's `Execute` on the render thread. On a restore, apply the pending resize and acquire before the
first frame records.

## K22 — Binding an animator to another skeleton drops its open event ranges without their `RangeEnd` {#k22}

**Latent** — `src/DingoEngine/Graphics/Animator.cpp:145-150,169-176`,
`src/DingoEngine/Scene/Systems/AnimationSystem.cpp:67`

`Animator::SetSkeleton` ends in `ResetToRest`, which clears every layer's states and open event ranges
(`:175`) and the frame's event list (`:169`), and sends nothing for what it cleared.
`AnimationSystem::EnsureAnimator` calls it whenever an entity's model has a different skeleton from the
one its animator is bound to: a game that assigns another `Model` to the `SkinnedMeshRendererComponent`,
or a hot-reload that changed the model's joints (a reload that keeps every joint's name and parent keeps
the `Skeleton`, and the ranges with it). A range that had begun then never ends. A game that opens a
hitbox on `RangeBegin` and shuts it on `RangeEnd` leaves it open, and `IsEventActive` turns false with
no event to say why. Every other way a range stops (a seek, a state that stops leading, events that
changed under the clip) sends its `RangeEnd`, through `CloseRanges` (`Animator.cpp:848-853`) or the
events-changed check in `CollectEvents`, so this is the one hole in "a range that opened always ends".

Nothing in-tree changes a skeleton while a range is open; the Animation Test's one rebind check has none
open. Left by the P8 review (the v0.8 plan's "As built in P8"). **Fix**: keep the ranges through
`ResetToRest` and set each layer's `CloseRangesOnUpdate`, so the next `Update` ends them the way a seek
does. That `RangeEnd` would name a clip of the old model, which a swap may have freed, so it needs a
null `Clip` (`AnimationSystem.cpp:100-103` already tolerates one).

## K23 — A disabled `AnimatorComponent` keeps reporting its last update's events {#k23}

**Latent** — `include/DingoEngine/Graphics/Animator.h:189`, `src/DingoEngine/Graphics/Animator.cpp:615`,
`src/DingoEngine/Scene/Systems/AnimationSystem.cpp:143-147`

`Animator::GetEventsThisFrame` and `ForEachEventThisFrame` return the list the animator's last `Update`
built, and only the next `Update` clears it (`Animator.cpp:615`). `AnimationSystem::Update` skips an
animator whose `AnimatorComponent::Enabled` is false before it updates, records or delivers anything
(`:143-144`), so a script that polls such an animator through `Scene::GetAnimator` reads the frame it was
disabled on again and again: a footstep, or a `RangeBegin`, repeats every frame for as long as the
component stays off. `OnAnimationEvent` and the F7 tab's event log are unaffected, because both sit after
the `Enabled` check (`:146-147`). `IsEventActive` keeps answering true for a range open at the pause,
which is the honest answer for a clip held mid-range.

Nothing in-tree polls a disabled animator. Left by the P8 review (the v0.8 plan's "As built in P8").
**Fix**: have `AnimationSystem` empty the list of an animator it skips, through a small `Animator` call
for it (`m_Events` is private).

## K24 — An animation layer that freezes while its weight is 0 freezes a stale pose {#k24}

**Latent** — `src/DingoEngine/Graphics/Animator.cpp:306-319`, `:356-362`, `:883-896`

A layer plays at most four states at once (`k_MaxStates`). A `Play`, `Stop` or `PlayOneShot` with a
fade that would make a fifth state replaces the four by one frozen pose, the mix so far, so a burst of
calls never pops (`Push`). For a
layer above 0 that mix is read from the layer's cached `Pose` (`FreezeSource`), and `Evaluate` refreshes
`Pose` only for a layer it evaluates, which leaves out a layer whose weight is 0 (`:887`). So a layer
that freezes at weight 0 holds the last pose it showed (the rest pose, if it never showed one), not the
mix its four states had reached, although their times kept advancing. When the weight comes up, the
masked joints fade from that pose, which no state was ever playing, into the new state. Layer 0 is not
affected: it has no weight and samples straight into `m_LocalPoses`.

It takes a fifth state on a layer at weight 0 (four fading calls on an empty layer above 0, which
starts with a transparent one) while the earlier fades still run, which nothing
in-tree does. Left by the P7 review (the v0.8 plan's "As built in P7"). **Fix**: evaluate a layer that is
about to freeze whatever its weight, or have `Push` sample the layer's states into `Pose` before it
freezes them.

## K25 — A reparent that keeps the world transform drops the shear a non-uniformly scaled parent puts on a rotated child {#k25}

**Limitation** — `src/DingoEngine/Scene/Systems/HierarchySystem.cpp:296-301,303-321`,
`src/DingoEngine/Scene/Entity.cpp:56-60`

A rotated child under a parent with a non-uniform scale is sheared in world space, and its world matrix,
which the renderer draws as it is, cannot be written as a position, rotation and scale. Everything that
does want those three goes through `HierarchySystem::Decompose`, which keeps the position, orthonormalises
the axes from X and drops the shear (`:314-320`) without a warning: `GetWorldRotation` and `GetWorldScale`, the
physics bake and write-back, and `keepWorldTransform`. `SetParent(parent, true)` and `RemoveParent(true)`
solve for the local transform that reproduces the old world under the new parent (`SetLocalFromWorld`), so
when that local is itself sheared (a rotated child moved under a non-uniformly scaled parent, or a sheared
one moved anywhere) the child changes shape in the call that promises to leave it where it was. A body
baked on a sheared child collides unsheared.

It dates from v0.7.1's hierarchy. docs/scenes-and-ecs.md describes it under "Shear", with the workaround:
keep a parent's scale uniform and put its scaled mesh on a child of its own. It is listed because nothing
warns. **Fix** (deferred): warn in `Reparent` when recomposing the solved local misses the world transform
by more than a tolerance (reparenting is rare, so the extra product costs nothing). Keeping the shear
needs a matrix-valued local transform, a breaking change to the components.

---

## Not tracked here

- **Missing capabilities** (no runtime UI layer, no mesh culling or instancing) are features, not
  bugs — they live in [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md), ranked and dependency-sequenced, and
  most are now scheduled in [ROADMAP.md](ROADMAP.md) at v0.9–v1.0.
- **Closed findings** stay in their dated review under `.claude/reviews/`, each with the commit that
  fixed it and how it was verified. Don't re-file them here.
- **Behaviour that surprises but is correct**: lights, specular and emissive live in the engine's
  lit shader (`src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl`), so they reach only materials
  drawn with it — the built-in default material and any made by `Renderer3D::CreateLitMaterial`. A
  material with a *custom* shader gets none of them unless its own shader implements them (the lights
  are in the scene UBO at binding 0). Working as designed, but routinely mistaken for a broken material.
