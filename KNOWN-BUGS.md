# Known Bugs

Open defects and sharp edges in DingoEngine. Companion to [ROADMAP.md](ROADMAP.md) (what gets built
next) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (missing capabilities, ranked). This file is only
for things that are **wrong or surprising in code that already ships**.

- **Verified against `VERSION` 0.7.2 on 2026-10-02.** Every entry below carries a `file:line` anchor
  confirmed in that pass. Code drifts — re-confirm before fixing, and delete the entry when it's gone.
  K10 and K11 are deliberate deferrals, not oversights; K16 and K17 are the two limits v0.7's
  lighting ships with, K18 a defect found reviewing its lit materials that predates it, and K19 one
  found reviewing Candlewick, all added on 2026-10-01. v0.7 and v0.7.1 closed no entry; v0.7.2
  closed K14 and K20, moved K16's `Renderer3D.cpp` lines, and added K21, found reviewing it.
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
  v0.7.1, filed and fixed together by `ApplicationParams::UpdateInBackground`).
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

---

## K10 — GLM is the one third-party dependency that leaks into public headers {#k10}

**Limitation** — 20 headers under `include/`

Every other vendored library is fully hidden from client code — ImGui, EnTT, Box2D, Jolt, NVRHI, GLFW
and miniaudio each appear in **zero** public headers, behind facades and opaque handles. GLM appears in
20, so any game linking the engine must also put GLM on its include path and match its version, and the
engine cannot change math libraries without breaking every consumer.

This is a known, deliberate deferral rather than an oversight. **Fix**: engine-owned vector/matrix
types in the public API, with GLM confined to `src/` — a large, breaking change best done alongside
another API break.

## K11 — Dragging a window to a display driven by another GPU is not handled {#k11}

**Limitation** — `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp:349-363`,
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

**Limitation** — `src/DingoEngine/Graphics/Renderer3D.cpp:482-511`

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

**Limitation** — `src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl:139-143`

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

**Defect** — `src/DingoEngine/Graphics/Material.cpp:65-73`,
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
`src/DingoEngine/Graphics/Material.cpp:172`, `src/DingoEngine/Graphics/NVRHI/NvrhiPipeline.cpp:26`

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

---

## Not tracked here

- **Missing capabilities** (no skeletal animation, no runtime UI layer, no mesh culling or
  instancing) are features, not bugs — they live in
  [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md), ranked and dependency-sequenced, and most are now scheduled
  in [ROADMAP.md](ROADMAP.md) at v0.8–v1.0.
- **Closed findings** stay in their dated review under `.claude/reviews/`, each with the commit that
  fixed it and how it was verified. Don't re-file them here.
- **Behaviour that surprises but is correct**: lights, specular and emissive live in the engine's
  lit shader (`src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl`), so they reach only materials
  drawn with it — the built-in default material and any made by `Renderer3D::CreateLitMaterial`. A
  material with a *custom* shader gets none of them unless its own shader implements them (the lights
  are in the scene UBO at binding 0). Working as designed, but routinely mistaken for a broken material.
