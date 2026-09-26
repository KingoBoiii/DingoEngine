# Known Bugs

Open defects and sharp edges in DingoEngine. Companion to [ROADMAP.md](ROADMAP.md) (what gets built
next) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (missing capabilities, ranked). This file is only
for things that are **wrong or surprising in code that already ships**.

- **Verified against `VERSION` 0.6.0 on 2026-07-31.** Every entry below carries a `file:line` anchor
  confirmed in that pass. Code drifts — re-confirm before fixing, and delete the entry when it's gone.
- **Not a review log.** Findings from a dated review pass live in `.claude/reviews/`; the v0.6.0 pass
  (`2026-07-29-v0.6.0-review.md`) is fully closed out — 4 Critical, 10 High, 12 Medium, 7 refactors and
  21 Lows all fixed — so nothing here comes from it.
- **IDs are never reused.** A fixed entry is deleted rather than renumbered, so gaps are expected:
  fixed in v0.6.2 were K1 (`Application::OnDestroy` never reached a derived override), K2 and K3
  (text could not draw its Latin-1 glyphs and had no UTF-8 decode) and K6 (the `.cache` directory
  followed the working directory).
- **The codebase carries no `TODO`/`FIXME`/`HACK` markers**, so nothing below came from scavenging
  in-source notes. Every entry was found by reading the code, or by hitting it while building a game on
  the engine.

**Status key** — **Defect**: behaves incorrectly, should be fixed. **Limitation**: behaves as written,
but silently costs correctness or portability. **Latent**: real, but nothing in-tree triggers it yet.

| # | Issue | Kind | Area |
|---|---|---|---|
| [K4](#k4) | A default-constructed `AssetHandle` passes `IsValidAssetHandle` | Defect | Asset |
| [K5](#k5) | The `Debug-ASan` configuration does not link | Defect | Build |
| [K7](#k7) | `Font::Create` ignores the asset root that `AssetManager` honours | Limitation | Asset |
| [K8](#k8) | Assigning one entity's physics component onto another's aliases its handle | Defect · Latent | Scene |
| [K9](#k9) | `Renderer3D` drops geometry on batch overflow in shipping builds | Limitation | Rendering |
| [K10](#k10) | GLM is the one third-party dependency that leaks into public headers | Limitation | API |
| [K11](#k11) | Dragging a window to a display driven by another GPU is not handled | Limitation | Vulkan |

---

## K4 — A default-constructed `AssetHandle` passes `IsValidAssetHandle` {#k4}

**Defect** — `src/DingoEngine/Core/UUID.cpp:13-16`, `include/DingoEngine/Asset/AssetTypes.h:15-17`

`AssetHandle` is a `UUID`, and `UUID::UUID()` **generates a fresh random 64-bit value**. The asset
layer meanwhile defines `k_InvalidAsset = AssetHandle(0)` and `IsValidAssetHandle(h)` as
`(uint64_t)h != 0`. The two disagree: a default-constructed handle — an uninitialised struct member, a
`resize()`d vector slot — is a random non-zero value that **reports itself valid** and then resolves to
nothing.

This is worse than a null-check that fails, because the guard reads as if it works. A caller doing the
right thing (`if (IsValidAssetHandle(m_Handle))`) gets a false positive.

**Fix**: give handles a null default (`AssetHandle` defaulting to 0, with random generation moved to an
explicit `UUID::Generate()`), so "default-constructed" and "invalid" mean the same thing.

## K5 — The `Debug-ASan` configuration does not link {#k5}

**Defect** — `premake5.lua:32-36`

The `Debug-ASan` configuration adds `-fsanitize=address` to both build and link options, but never
defines `_DISABLE_STRING_ANNOTATION` / `_DISABLE_VECTOR_ANNOTATION` (absent repo-wide). Without them,
MSVC's annotated STL containers don't link against the **non-ASan** Vulkan SDK prebuilts the engine
links, so the configuration fails at link time and cannot be used.

Even once it links, the ASan runtime DLL must reach the output directory or the app dies at startup
with `0xC0000135` (the same failure mode as a missing assimp DLL).

**Fix**: add both defines to the `Debug-ASan` filter, and copy the ASan runtime alongside the other
post-build DLL copies.

## K7 — `Font::Create` ignores the asset root that `AssetManager` honours {#k7}

**Limitation** — `src/DingoEngine/Graphics/Font.cpp:185,196-197`

`Font::Create` stores the path it is handed verbatim, so it is interpreted relative to the working
directory. `AssetManager::GetFont` resolves against the configured **asset root**. Both are public and
supported, and they disagree about what a relative path means — so moving a font load from one API to
the other silently changes which file (if any) is found, and a wrong cwd loses all on-screen text with
no crash.

**Fix**: route `Font::Create` through the same path resolution, or document the split explicitly in
[docs/asset-pipeline.md](docs/asset-pipeline.md) and steer games to the manager.

## K8 — Assigning one entity's physics component onto another's aliases its handle {#k8}

**Defect · Latent** — `include/DingoEngine/Scene/Components.h` (every component with a `Runtime*` handle)

Six components carry a live runtime handle: `RigidBody2DComponent`, `BoxCollider2DComponent`,
`CircleCollider2DComponent`, `RigidBody3DComponent`, `CharacterController3DComponent` and
`AudioSourceComponent`. Since v0.6.2 `Entity::AddComponent` clears the handles of whatever it adds, so
copying a component onto another entity — `b.AddComponent(a.GetComponent<RigidBody3DComponent>())` —
is safe, as is `Scene::DuplicateEntity`.

Assigning onto a component that is already live still copies the handle:
`b.GetComponent<RigidBody3DComponent>() = a.GetComponent<RigidBody3DComponent>();` makes B drive A's
body and orphans B's own, and whichever entity is destroyed second frees A's body twice.

The reset can't move into the components' copy constructor or assignment: EnTT relocates components
by copy and move when its storage compacts, and those must carry the handle, or every live entity loses
its body the first time another entity is destroyed.

**Workaround**: copy the settings field by field. **Fix**: split the handle out of the settings (an
engine-owned runtime component beside the user-facing one), so assigning settings can never touch it.

## K9 — `Renderer3D` drops geometry on batch overflow in shipping builds {#k9}

**Limitation** — `include/DingoEngine/Graphics/Renderer3D.h:34`, `src/DingoEngine/Graphics/Renderer3D.cpp:248-265`

When a material's batch exceeds `MaxVertices` / `MaxIndices` (65,536 by default), the remaining meshes
for that material are **discarded for the rest of the scene**. There is a hard-fail path, but
`AssertOnOverflow` defaults to `false` *and* asserts compile out of Release and Distribution, so a
shipping build warns once, increments `Statistics::DroppedMeshes`, and carries on rendering an
incomplete world.

Mitigations that do exist: the warn-once log, the counter, and the F4 renderer panel surfacing
`Dropped : N` with a hint. None are visible in a Distribution build with the overlay off.

**Fix (game-side, today)**: raise the caps via `ApplicationParams.Renderer3D`, and set
`AssertOnOverflow` in Debug. **Fix (engine)**: auto-flush the batch on overflow the way `Renderer2D`
does, rather than dropping — see [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) #3.

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
