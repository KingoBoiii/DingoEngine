# Known Bugs

Open defects and sharp edges in DingoEngine. Companion to [ROADMAP.md](ROADMAP.md) (what gets built
next) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (missing capabilities, ranked). This file is only
for things that are **wrong or surprising in code that already ships**.

- **Verified against `VERSION` 0.6.3 on 2026-09-27.** Every entry below carries a `file:line` anchor
  confirmed in that pass. Code drifts — re-confirm before fixing, and delete the entry when it's gone.
  K10 and K11 are deliberate deferrals, not oversights; K14 was added, and anchored, on 2026-09-29.
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
  colour attachment used `LOAD_OP_NONE`). Fixed since v0.6.3: K15 (a long frame dropped 3D bodies
  through their colliders).
- **The codebase carries no `TODO`/`FIXME`/`HACK` markers**, so nothing below came from scavenging
  in-source notes. Every entry was found by reading the code, or by hitting it while building a game on
  the engine.

**Status key** — **Defect**: behaves incorrectly, should be fixed. **Limitation**: behaves as written,
but silently costs correctness or portability. **Latent**: real, but nothing in-tree triggers it yet.

| # | Issue | Kind | Area |
|---|---|---|---|
| [K10](#k10) | GLM is the one third-party dependency that leaks into public headers | Limitation | API |
| [K11](#k11) | Dragging a window to a display driven by another GPU is not handled | Limitation | Vulkan |
| [K14](#k14) | The test framework crashes when its window is minimized | Defect | Test app |

---

## K10 — GLM is the one third-party dependency that leaks into public headers {#k10}

**Limitation** — 19 headers under `include/`

Every other vendored library is fully hidden from client code — ImGui, EnTT, Box2D, Jolt, NVRHI, GLFW
and miniaudio each appear in **zero** public headers, behind facades and opaque handles. GLM appears in
19, so any game linking the engine must also put GLM on its include path and match its version, and the
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

## K14 — The test framework crashes when its window is minimized {#k14}

**Defect** — `test/src/TestLayer.cpp:266-275`, `test/src/UI/TestViewportPanel.cpp:15-16`

ImGui's GLFW backend reads the window size fresh in `NewFrame`, so a minimize that lands between a
frame's event poll and its ImGui frame gives ImGui a 0×0 display for that one frame — before
`Application` has seen the 0×0 `WindowResizeEvent` that makes it skip frames. The docked Viewport
panel's `GetContentRegionAvail()` then comes back negative (`-8×19` on a 1600×900 window), and
`TestLayer` passes it straight to `Test::Resize(uint32_t, uint32_t)` and to a post-execution
`Framebuffer::Resize` through `static_cast<uint32_t>`. The framebuffer asks Vulkan for a
4294967288-wide image, the validation layer rejects it (`maxFramebufferWidth` is 16384), and the
process dies.

Engine-independent: it reproduces identically with and without the minimized-window frame skip, and a
minimize almost always lands mid-frame. Seen only in the test app so far — FlappyBird minimizes
cleanly, and `SceneRenderer` already guards a zero height.

**Fix**: skip the resize while either viewport dimension is ≤ 0. A guard in `TestLayer` doing exactly
that was verified to make minimize/restore clean.

---

## Not tracked here

- **Missing capabilities** (no point lights, no transform hierarchy, no skeletal animation, no runtime
  UI layer, no frustum culling or instancing) are features, not bugs — they live in
  [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md), ranked and dependency-sequenced, and most are now scheduled
  in [ROADMAP.md](ROADMAP.md) at v0.7–v1.0.
- **Closed findings** stay in their dated review under `.claude/reviews/`, each with the commit that
  fixed it and how it was verified. Don't re-file them here.
- **Behaviour that surprises but is correct**: the emissive term lives in the engine's built-in lit
  shader (`Renderer3D.cpp:65`), so a material with a *custom* shader gets no emissive unless its own
  shader implements it. Working as designed, but routinely mistaken for a broken material.
