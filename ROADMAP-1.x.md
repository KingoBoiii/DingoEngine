# DingoEngine Roadmap — after 1.0

[ROADMAP.md](ROADMAP.md) took the engine from v0.1 to v1.0 and holds the notes of every release
since. This one is the plan from here.
From here every release follows [Semantic Versioning](docs/versioning.md): a 1.x minor release
only adds, and anything that changes existing API, how a game looks or how it simulates waits for
2.0. So each item below says where it can go, and the 2.0 list collects what can't ship any earlier.

The order is a proposal, not a schedule. Releases ship when they're done, as before. Each milestone
gets its own plan in `.claude/plans/` before it starts, and ships or upgrades an example that uses
it.

## Shipped so far

Two minor releases came from other work before the milestones below started, so those moved down
two numbers (the first planned one, scenes on disk, is now 1.3). Their notes are
in [ROADMAP.md](ROADMAP.md#v110--joints-ragdolls--fixed-tick).

- **v1.1.0, Joints, Ragdolls & Fixed Tick**: constraints and motors, breakable constraints,
  collision groups, active ragdolls, the opt-in fixed simulation tick (`SceneParams::FixedTick`)
  and microphone capture. The fixed tick was the fixed-step physics item of Gameplay and
  Shipping (then 1.4, now 1.6).
- **v1.2.0, Crash Reporting**: opt-in crash and error reports to a Sentry-compatible server, in
  every Release and Distribution build, and stop signals that end a windowed app through `Close()`.
  It covered most of that milestone's crash-report items.

## Decisions (owner)

| # | Question | Decision |
|---|---|---|
| R1 | How does the editor (its own repo) get at ImGui? | **It builds against the engine's copy**, with an engine hook to host its panels. The engine's static library already links ImGui, so a second copy in the same executable would break the one-definition rule. The editor compiles against the same `vendor/` ImGui, and the engine exposes the context and a panel hook without putting ImGui types in a public header. The editor-support plan (now 1.5) settles the details. |
| R2 | Does PBR stay opt-in for good? | **Opt-in through 1.x, the default in 2.0.** Two lighting models forever would double the shader work behind every lighting change. |
| R3 | Where do engine-support APIs for the editor live? | **Ordinary public API under SemVer**, including editor-only hooks. There is no separate editor header set with a looser promise, so an editor need waits for a minor release like any other addition. |
| R4 | Do the engine layers ship in 1.x? (2026-11-12) | **No, in 2.0.** It redesigns the architecture and drops includes games can lean on. 1.x adds only the hooks its milestones need: the component registry in 1.3 and the panel hook in 1.5. |

## Patch releases — Bug sweep

Patches on the latest minor release for the bugs still open from 1.0. A patch may fix a filed
bug even when that changes the picture, with a **For games** note in the release. The UV flips
([#272](https://github.com/KingoBoiii/DingoEngine/issues/272),
[#273](https://github.com/KingoBoiii/DingoEngine/issues/273)), Hearthvale's `--freeze` checks
([#276](https://github.com/KingoBoiii/DingoEngine/issues/276),
[#277](https://github.com/KingoBoiii/DingoEngine/issues/277)), the `%TEMP%` reload race
([#274](https://github.com/KingoBoiii/DingoEngine/issues/274)), the D3D 1-px flakes
([#224](https://github.com/KingoBoiii/DingoEngine/issues/224)) and
[#231](https://github.com/KingoBoiii/DingoEngine/issues/231) are closed.

**Next: v1.2.1**, the Linux hang on close
([#186](https://github.com/KingoBoiii/DingoEngine/issues/186)), fixed on master after 1.2.0
([#325](https://github.com/KingoBoiii/DingoEngine/pull/325)): an X11 window presents on its own xcb
connection, so the close event is no longer lost.

Still open:

- The D3D Δ1 speckle ([#225](https://github.com/KingoBoiii/DingoEngine/issues/225)) and
  Marionette's unchecked memory growth
  ([#239](https://github.com/KingoBoiii/DingoEngine/issues/239)).
- Render CPU regressions accepted for 1.0
  ([#226](https://github.com/KingoBoiii/DingoEngine/issues/226),
  [#232](https://github.com/KingoBoiii/DingoEngine/issues/232)).
- Linux on real GPUs and desktops ([#247](https://github.com/KingoBoiii/DingoEngine/issues/247),
  L26). It needs hardware, so it waits for the owner or a tester.

## v1.3 — Scenes on Disk

A scene can't be saved today: `SaveData` stores only what a game puts in it. This is what an editor
needs first, and games want it on its own for level files and prefabs.

- **Scene serialization**: write and read a whole scene (entities, hierarchy, every built-in
  component, asset references by id) to a text format that diffs well, plus a packed form for
  shipping. It loads through the asset VFS, so `.dpak` packages carry scenes too.
- **Component reflection**: name, type, range and default for each field of the built-in
  components, registered next to `DE_INSTANTIATE_COMPONENT`. The serializer walks it, and later the
  editor's inspector does. Games register their own components the same way. 1.1's
  `Constraint3DComponent` and `RagdollComponent` refer to other entities and to `.ragdoll` assets,
  so entity references are saved by UUID and resolved after the whole scene loads.
- **Prefabs**: a saved subtree instanced into a scene, built on `Scene::DuplicateSubtree`, with
  per-instance overrides.
- **More than one script per entity** (today an entity holds one).
- **Versioned scene files**: a file records its format version, and a newer engine reads an older
  file. The format is public behaviour from 1.3 on.
- **Additive hooks only.** The component registry is new public API next to the existing code; no
  header moves and no include goes away. The engine-layers restructuring that would make it the
  general registration pattern waits for 2.0 (see below). The plan is
  `.claude/plans/2026-11-12-v1.3-scenes-on-disk-plan.md`.

**Example**: Hearthvale's valley, or one of the older examples' levels, loaded from scene files
instead of built in code, with a `--check` that saves, reloads and compares.

## v1.4 — Physically Based Rendering

Lit materials are normalised Blinn-Phong, with no metalness, normal maps or reflections. PBR comes
as an opt-in shading model, so every existing material keeps its look (see R2).

- **Metal/rough GGX shading** with albedo, metallic-roughness, normal, AO and emissive maps.
  Normal maps need tangents, so the vertex format and the model importer grow them.
- **Image-based lighting**: an environment cubemap per scene, irradiance and prefiltered specular
  maps baked with compute, and a BRDF lookup table. A skybox draws the same cubemap.
- **Import**: glTF and FBX PBR material inputs through assimp, with Blinn-Phong as the fallback
  when a file has none.
- **Constraints**: the scene UBO stays append-only, and the default tone curve doesn't change in
  1.x. Every step needs Vulkan, D3D11 and D3D12 runs on the GPU machine.

**Example**: a material gallery (spheres in a metal × roughness grid, with model showpieces under
switchable environment maps), with a `--check` that compares against reference values.

## v1.5 — Editor Support

The engine side of the editor, released together with the editor repo's first version. The editor
itself lives in its own repo, pins `DingoEngine ^1.5` and ships out of band (see Modules).

- **Viewport**: render a scene into a texture an ImGui window shows, with an editor camera that
  isn't an entity.
- **Picking**: an entity-id target, so a click selects what's under the cursor. Games can use it too.
- **Gizmos**: helpers for translating, rotating and scaling through the hierarchy.
- **Play and stop**: snapshot a scene, run it, and restore it on stop, with character controllers
  rebuilt on start as today.
- **Asset browser support**: listing assets, generating thumbnails, and the existing hot-reload as
  the live link.
- **The ImGui host hook** (R1), public API under SemVer like the rest (R3).

**Example**: the editor opening and saving the 1.3 example's scenes.

## v1.6 — Gameplay and Shipping

The gaps games hit first, collected from the docs' Limits sections and the crash-report review.

- **2D physics queries**: ray casts, overlap queries and contact callbacks, which 3D has and 2D
  lacks.
- **Audio**: bus effects (low-pass, reverb), ducking, and volume fades.
- **Crash reports**, the rest after 1.2.0's upload:
  - Game context: an `Application` call to attach the scene, save slot, settings and the game's
    own lines to an event, as tags and extras.
  - Linux minidumps, alongside the text backtrace, so `SendAttachments` carries one on Linux too.
- **Shipping**: a Linux window icon at run time, and the build-time hooks a store-integration
  module needs.
- **A scene's own physics world**: append `Physics2D*`/`Physics3D*` members to `SceneParams`; null
  (the default) keeps the engine's Box2D and Jolt worlds, so nothing changes for existing games.
  `Physics3D` (`Physics/3D/Physics3D.h`) and `Physics2D` are already interfaces. It serves a
  headless server for the networking module, a deterministic or mock world for tests, and a game's
  own simple physics. If the networking module starts before 1.6, this ships in a minor release
  ahead of it.

**Example**: Angry Birds on 2D contact callbacks and ray casts instead of velocity checks.

## v1.7 — Renderer Scale

Measurement work, like v1.0's throughput pass, once PBR has made every pixel more expensive.

- **Clustered (forward+) lighting**: per-tile light lists instead of every pixel looping over all
  4 + 32 lights, so the limit on point and spot lights can rise.
- **Depth pre-pass**, opt-in, for scenes with heavy overdraw.
- **Mesh LODs**: switching to simpler meshes with distance, imported or generated.
- **Occlusion culling**: today only the view and shadow views cull.
- **Shadows**: more than one casting directional light, and translucent casters that cast partial
  shadows.

**Example**: Hearthvale's valley grown several times over, timed against the v1.0 numbers.

## 2.0 candidates

Nothing here is scheduled. 2.0 happens when enough of this builds up to justify a migration guide.

- **PBR as the default** lighting model (R2), and Blinn-Phong removed or kept as a legacy material.
- **The fixed simulation tick as the default** (`SceneParams::FixedTick`, opt-in since 1.1).
- **Removing every API deprecated through 1.x.**
- **A scene UBO and material binding layout redesigned** around PBR and clustered lighting,
  instead of growing by appending.
- **Factories return `std::unique_ptr`** (`.claude/plans/2026-11-03-smart-pointers-plan.md`,
  phase 3).
- Anything else a 1.x milestone finds it can't do without breaking, added here as it comes up.

### Engine layers (2.0)

Games keep linking one library, `DingoEngine`. Inside it, each folder is laid out as if it were a
library of its own: it uses only the layers below it, never one beside or above it. Today the
bottom depends on the top. The public `Core/Application.h` includes the renderers,
`SceneRenderer` and DingoUI. `AssetManager` creates textures, models, fonts and sounds itself.
Physics includes `Graphics/Mesh.h`, and DingoUI and the ImGui panels include `Scene`.

| Layer (bottom to top) | Folders | May use |
|---|---|---|
| Core | `Core/` (log, file system, VFS, saves, crash handler and reporter, timer, UUID), `Events/` | nothing |
| Platform | `Windowing/`, input | Core |
| Asset | `Asset/` | Core |
| Graphics | `Graphics/` | Core, Platform, Asset |
| Audio | `Audio/` | Core, Asset |
| Physics | `Physics/2D/`, `Physics/3D/` | Core |
| UI | `UI/` (DingoUI, ImGui) | Core, Platform, Asset, Graphics, Audio |
| Scene | `Scene/` | every layer above |
| Application | `Application`, `Layer`, `LayerStack`, the entry point | everything |

- **Registration instead of reaching up.** A layer exposes a hook and the layer above fills it:
  - Graphics and Audio register their loaders with the `AssetManager`.
  - Scene registers its components and systems, plus its debug panels with UI. That is the same
    panel hook the editor needs (R1).
  - The Application layer starts the subsystems. 1.3's component registry and 1.5's panel hook
    already exist by then; 2.0 makes them the general pattern.
- **The cycles to break**:
  - Asset ↔ Graphics, and Asset → Audio: loaders get registered from above.
  - Physics → Graphics: a mesh collider takes vertices and indices.
  - UI → Scene: world-space widgets take a position from the caller, and the scene panels move up
    to Scene.
  - Core → everything: `Application` and `Settings` move up a layer.
- **The scene holds data; renderers read it.** Today `Scene.h` carries rendering entry points
  (submitting lights, shadow views, overlay passes) that `SceneRenderer` wraps. In 2.0 those move
  out: a scene is entities and components, and a renderer takes a scene, whether the default
  `SceneRenderer`, a game's own, the editor viewport, or a null renderer for a server. The scene
  doesn't call a renderer interface; the dependency points from the renderer to the scene.
- **Null implementations** of physics, audio and the scene renderer for servers and CI, designed
  with the layers. Interfaces only where a second implementation exists: a public interface a
  game implements is frozen for the whole major series (it can only gain virtuals with default
  bodies), so audio, windowing and assets stay concrete.
- **Why 2.0 and not 1.x** (owner, 2026-11-12): it redesigns the engine's architecture, and it
  breaks source in a way SemVer's letter misses. `Core/Application.h` includes the renderers,
  `SceneRenderer`, `AssetManager` and DingoUI today, so a game that includes only it can use
  `Renderer3D` without including that header. Dropping those includes breaks such a game. It also
  rewrites start-up and teardown order (`Application` and `Settings` leave Core, loaders leave
  `AssetManager`). In 2.0 it shares one migration guide with the other breaks.
- **Moved headers**: a public header that moves to its layer's folder (`Application.h`, `Input.h`,
  the key codes, `PerspectiveCamera.h`, `Ray.h`) leaves a forwarding header at its old path through
  2.0, and `<DingoEngine.h>` keeps including everything.
- **Enforced**: a script in `lint.yml` checks every `#include` against the table above and fails
  the PR on a forbidden one. Building each layer as an internal library merged into
  `DingoEngine` would enforce it in the linker too. The plan weighs that against the extra build
  steps.
- **A side effect to measure, not a promise**: once nothing references a subsystem a game doesn't
  use, the linker can leave it out. A 2D game, for example, would carry no Jolt.

## Modules — shipped out of band

Versioned separately and layered on a released engine, as ROADMAP.md describes.

- **Editor**: its own repo. Starts once 1.3's serialization lands, and its first version ships with
  1.5.
- **Scripting**: C# or Lua game logic, with the Tower Defense showcase. It goes well with the editor:
  changes without recompiling.
- **Networking & multiplayer**: transport, client-server, replicated ECS state, and 2–4 player co-op
  retrofitted onto an example. The v1.0 API pass kept the replication seams intact, and 1.1's fixed
  tick gives it tick numbers and interpolation between ticks to build on.
- **Store integration**: Steamworks (achievements, cloud saves, overlay) and itch.io butler uploads,
  so games without them don't carry the SDKs.
