# DingoEngine Roadmap — after 1.0

[ROADMAP.md](ROADMAP.md) took the engine from v0.1 to v1.0. This one picks up after the v1.0.0 tag.
From here every release follows [Semantic Versioning](docs/versioning.md): a 1.x minor release
only adds, and anything that changes existing API, how a game looks or how it simulates waits for
2.0. So each item below says where it can go, and the 2.0 list collects what can't ship any earlier.

The order is a proposal, not a schedule. Releases ship when they're done, as before. Each milestone
gets its own plan in `.claude/plans/` before it starts, and ships or upgrades an example that uses
it.

## Decisions (owner, 2026-10-18)

| # | Question | Decision |
|---|---|---|
| R1 | How does the editor (its own repo) get at ImGui? | **It builds against the engine's copy**, with an engine hook to host its panels. The engine's static library already links ImGui, so a second copy in the same executable would break the one-definition rule. The editor compiles against the same `vendor/` ImGui, and the engine exposes the context and a panel hook without putting ImGui types in a public header. The 1.3 plan settles the details. |
| R2 | Does PBR stay opt-in for good? | **Opt-in through 1.x, the default in 2.0.** Two lighting models forever would double the shader work behind every lighting change. |
| R3 | Where do engine-support APIs for the editor live? | **Ordinary public API under SemVer**, including editor-only hooks. There is no separate editor header set with a looser promise, so an editor need waits for a minor release like any other addition. |

## v1.0.x — Bug sweep

Patch releases for the bugs filed while finishing 1.0. A patch may fix a filed bug even when that
changes the picture, so the UV flips belong here, each with a **For games** note in the release.

- Textures upside down: models ([#272](https://github.com/KingoBoiii/DingoEngine/issues/272)) and
  `Mesh::CreateSphere` ([#273](https://github.com/KingoBoiii/DingoEngine/issues/273)).
- Hearthvale's `--freeze` checks ([#276](https://github.com/KingoBoiii/DingoEngine/issues/276),
  [#277](https://github.com/KingoBoiii/DingoEngine/issues/277)).
- Test and CI flakes: the shared `%TEMP%` reload race
  ([#274](https://github.com/KingoBoiii/DingoEngine/issues/274)), D3D 1-px flakes and the Δ1 speckle
  ([#224](https://github.com/KingoBoiii/DingoEngine/issues/224),
  [#225](https://github.com/KingoBoiii/DingoEngine/issues/225)), the Linux Release hang on close
  ([#186](https://github.com/KingoBoiii/DingoEngine/issues/186)), Marionette's unchecked memory growth
  ([#239](https://github.com/KingoBoiii/DingoEngine/issues/239)).
- Render CPU regressions accepted for 1.0
  ([#226](https://github.com/KingoBoiii/DingoEngine/issues/226),
  [#231](https://github.com/KingoBoiii/DingoEngine/issues/231),
  [#232](https://github.com/KingoBoiii/DingoEngine/issues/232)).
- Linux on real GPUs and desktops ([#247](https://github.com/KingoBoiii/DingoEngine/issues/247),
  L26). It needs hardware, so it waits for the owner or a tester.

## v1.1 — Engine Layers and Scenes on Disk

### Engine layers

Games keep linking one library, `DingoEngine`. Inside it, each folder is laid out as if it were a
library of its own: it uses only the layers below it, never one beside or above it. Today the
bottom depends on the top. The public `Core/Application.h` includes the renderers,
`SceneRenderer` and DingoUI. `AssetManager` creates textures, models, fonts and sounds itself.
Physics includes `Graphics/Mesh.h`, and DingoUI and the ImGui panels include `Scene`.

| Layer (bottom to top) | Folders | May use |
|---|---|---|
| Core | `Core/` (log, file system, VFS, saves, crash handler, timer, UUID), `Events/` | nothing |
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
  - The Application layer starts the subsystems. 1.1's component reflection uses the same
    registration.
- **The cycles to break**:
  - Asset ↔ Graphics, and Asset → Audio: loaders get registered from above.
  - Physics → Graphics: a mesh collider takes vertices and indices.
  - UI → Scene: world-space widgets take a position from the caller, and the scene panels move up
    to Scene.
  - Core → everything: `Application` and `Settings` move up a layer.
- **Source compatible**: a public header that moves to its layer's folder (`Application.h`,
  `Input.h`, the key codes, `PerspectiveCamera.h`, `Ray.h`) leaves a forwarding header at its old
  path. Public calls stay as they are, so a 1.0 game compiles unchanged.
- **Enforced**: a script in `lint.yml` checks every `#include` against the table above and fails
  the PR on a forbidden one. Building each layer as an internal library merged into
  `DingoEngine` would enforce it in the linker too. The plan weighs that against the extra build
  steps.
- **A side effect to measure, not a promise**: once nothing references a subsystem a game doesn't
  use, the linker can leave it out. A 2D game, for example, would carry no Jolt.

### Scenes on disk

A scene can't be saved today: `SaveData` stores only what a game puts in it. This half of the
milestone is what an editor needs first, and games want it on its own for level files and prefabs.

- **Scene serialization**: write and read a whole scene (entities, hierarchy, every built-in
  component, asset references by id) to a text format that diffs well, plus a packed form for
  shipping. It loads through the asset VFS, so `.dpak` packages carry scenes too.
- **Component reflection**: name, type, range and default for each field of the built-in
  components, registered next to `DE_INSTANTIATE_COMPONENT`. The serializer walks it, and later the
  editor's inspector does. Games register their own components the same way.
- **Prefabs**: a saved subtree instanced into a scene, built on `Scene::DuplicateSubtree`, with
  per-instance overrides.
- **More than one script per entity** (today an entity holds one).
- **Versioned scene files**: a file records its format version, and a newer engine reads an older
  file. The format is public behaviour from 1.1 on.

**Example**: Hearthvale's valley, or one of the older examples' levels, loaded from scene files
instead of built in code, with a `--check` that saves, reloads and compares.

## v1.2 — Physically Based Rendering

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

## v1.3 — Editor Support

The engine side of the editor, released together with the editor repo's first version. The editor
itself lives in its own repo, pins `DingoEngine ^1.3` and ships out of band (see Modules).

- **Viewport**: render a scene into a texture an ImGui window shows, with an editor camera that
  isn't an entity.
- **Picking**: an entity-id target, so a click selects what's under the cursor. Games can use it too.
- **Gizmos**: helpers for translating, rotating and scaling through the hierarchy.
- **Play and stop**: snapshot a scene, run it, and restore it on stop, with character controllers
  rebuilt on start as today.
- **Asset browser support**: listing assets, generating thumbnails, and the existing hot-reload as
  the live link.
- **The ImGui host hook** (R1), public API under SemVer like the rest (R3).

**Example**: the editor opening and saving the 1.1 example's scenes.

## v1.4 — Gameplay and Shipping

The gaps games hit first, collected from the docs' Limits sections and the crash-report review.

- **Fixed-step physics, opt-in**: physics stepped on `OnFixedUpdate`'s clock. 1.x keeps frame-delta
  stepping as the default ([versioning](docs/versioning.md#simulation-timing)).
- **2D physics queries**: ray casts, overlap queries and contact callbacks, which 3D has and 2D
  lacks.
- **Audio**: bus effects (low-pass, reverb), ducking, and volume fades.
- **Crash reports**:
  - Game context: an `Application` call to attach the scene, save slot, settings and the game's
    own lines to the report.
  - An opt-in upload hook: the game decides whether and where reports go, and nothing leaves the
    machine by default, as today.
  - Linux minidumps, alongside the text backtrace.
- **Shipping**: a Linux window icon at run time, and the build-time hooks a store-integration
  module needs.

**Example**: Angry Birds on 2D contact callbacks and ray casts instead of velocity checks, plus a
fixed-step physics mode in one example.

## v1.5 — Renderer Scale

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
- **Fixed-step physics as the default.**
- **Removing every API deprecated through 1.x.**
- **A scene UBO and material binding layout redesigned** around PBR and clustered lighting,
  instead of growing by appending.
- Anything else a 1.x milestone finds it can't do without breaking, added here as it comes up.

## Modules — shipped out of band

Versioned separately and layered on a released engine, as ROADMAP.md describes.

- **Editor**: its own repo. Starts once 1.1's serialization lands, and its first version ships with
  1.3.
- **Scripting**: C# or Lua game logic, with the Tower Defense showcase. It goes well with the editor:
  changes without recompiling.
- **Networking & multiplayer**: transport, client-server, replicated ECS state, and 2–4 player co-op
  retrofitted onto an example. The v1.0 API pass kept the replication seams intact.
- **Store integration**: Steamworks (achievements, cloud saves, overlay) and itch.io butler uploads,
  so games without them don't carry the SDKs.
