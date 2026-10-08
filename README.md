# DingoEngine

A C++20 game engine built on top of [NVRHI](https://github.com/NVIDIAGameWorks/nvrhi) with Vulkan as the primary graphics back-end.

## Features

- **Rendering** — Vulkan-backed graphics pipeline via NVRHI; batched 2D renderer (quads, circles, MSDF text)
- **Windowing & Input** — GLFW window management with a frame-coherent keyboard/mouse/gamepad input system (edge + held queries, mouse scroll/delta, up to 16 controllers with deadzone-filtered sticks)
- **Layer System** — Stack-based middleware architecture for organizing application logic
- **Event System** — Type-safe, decoupled event dispatcher (window, keyboard, mouse, gamepad)
- **ImGui Integration** — Built-in debug/overlay UI layer with docking and viewport support
- **Font Rendering** — MSDF atlas-based text rendering via `msdf-atlas-gen`
- **Asset Utilities** — Texture, shader, and buffer creation with a fluent params/builder API
- **Scenes & ECS** — entity-component scenes with `ScriptableEntity` behaviours and a `SceneManager` for multi-scene games; supports both 2D and 3D entities (ECS backend kept internal)
- **2D Physics** — Box2D-backed rigid-body simulation wired into the ECS (`RigidBody2D` / `BoxCollider2D` / `CircleCollider2D` components, gravity, forces/impulses; physics backend kept internal)
- **3D Physics & Scene** — Jolt-backed `Physics3D`, usable standalone or wired into the ECS (`Transform3D` / `MeshRenderer` / `RigidBody3D` / `Box`+`SphereCollider3D` components), with 3D meshes drawn through `Renderer3D` and a perspective camera (physics backend kept internal)
- **3D Lighting** — forward-lit `Renderer3D` with coloured directional, point and spot lights (up to 32 point/spot lights per scene, the most relevant picked each frame), ambient light, Blinn-Phong specular, and lit materials with emissive and an albedo texture. Lights are ECS components (`PointLightComponent` / `SpotLightComponent` / `AmbientLightComponent` / `DirectionalLightComponent`), and the lit shader hot-reloads in Debug builds when asset hot-reload is enabled
- **Transform Hierarchy** — 3D and 2D parent-child transforms (`Entity::SetParent`), world values computed per pass, subtree destroy/duplicate, and physics that follows parents
- **Skeletal Animation** — skinned glTF/FBX models drawn with GPU skinning; an `Animator` with cross-fades, `Blend1D` blends, masked layers and one-shots; clip events (code or a `.events` file beside the model) delivered to scripts; joint sockets; retargeting by joint name; models and their events hot-reload in place; an F7 Animation tab

## Documentation

Usage guides for building games with the engine live in [docs/](docs/README.md):

- [Getting Started](docs/getting-started.md) — prerequisites, linking the `.lib` + headers, and a minimal app
- [Application & Layers](docs/application-and-layers.md) — entry point, lifecycle, input, events
- [2D Rendering](docs/rendering-2d.md) — quads, circles, text, textures, fonts
- [Scenes & ECS](docs/scenes-and-ecs.md) — entities, components, systems, and scene management
- [2D Physics](docs/physics-2d.md) — rigid bodies, colliders, gravity, and forces/impulses
- [3D Physics](docs/physics-3d.md) — the Jolt-backed `Physics3D`, standalone or ECS-integrated, including mesh colliders
- [Asset Pipeline](docs/asset-pipeline.md) — the `AssetManager`, UUID handles, async loading, and hot-reload
- [Lighting](docs/lighting.md) — directional, point and spot lights, the light budget, specular, and lit materials
- [Animation](docs/animation.md) — skinned models, the animator, blending and layers, timeline events, joint sockets, and model hot-reload

## Roadmap

Currently at **v0.8.3**, which closes every open bug issue, from animation event ranges to Vulkan frame ordering, and adds what *Headstone* asked for: text that turns and keeps its z, scenes rendered into textures, saving textures as PNGs, and gamepad rumble. Before it, v0.8.2 let DirectX 12 start on a machine without a GPU and v0.8.1 fixed back-face culling for custom materials. All three sit on v0.8.0: the animation engine work (skinned models, an animator with blending, layers and timeline events, joint sockets, and in-place model hot-reload) and its example game, *Marionette*. Before it, v0.7.0 shipped the lighting engine and *Candlewick*, v0.7.1 the transform hierarchy, and v0.7.2 a choice to keep updating in the background (`UpdateInBackground`). Every milestone ships with an example game that exercises it — see
[ROADMAP.md](ROADMAP.md) for the full plan, the point releases (v0.4.1–v0.4.3, v0.5.1, v0.6.1–v0.6.3, v0.7.1–v0.7.2, v0.8.1–v0.8.3) and what each
example is built to demonstrate.

| Version | Milestone | Example game | Status |
|---|---|---|---|
| v0.1 | Core Foundation — windowing, input, basic rendering pipeline | `FlappyBird` | shipped |
| v0.2 | Extended Rendering Pipeline — D3D11/D3D12 back-ends, `Renderer3D`, render thread | `Breakout3D` | shipped |
| v0.3 | Scenes & ECS | `SpaceInvaders` | shipped |
| v0.4 | Physics & Collision — Box2D 2D and Jolt 3D, then 3D inside the ECS | `AngryBirds`, `DungeonCrawler3D` | shipped |
| v0.5 | Audio & Gameplay-Grade Physics — miniaudio, character controller, ray/shape casts | `EchoVault` | shipped |
| v0.6 | Asset Pipeline & Hot-Reload — `AssetManager`, async loading, live reload | `ArenaShooter` | shipped |
| v0.7 | Lighting & Shading — point/spot lights on a capped forward multi-light path, specular | `Candlewick` | shipped |
| v0.7.1 | Transform Hierarchy — parent-child transforms in 3D and 2D, world-space rendering, lights, audio and physics | `DungeonCrawler3D`, `EchoVault` | shipped |
| **v0.8** | **Animation & Character Fidelity** — GPU-skinned meshes, clips, blending and layers, timeline events, joint sockets | `Marionette` | shipped |
| v0.9 | Shadows, Post-processing & VFX | *Candlewick* upgrade | planned |
| v1.0 | Stability, Performance & Polish — docs, Linux validation, culling + instancing | *Dungeon Crawler* (full release) | planned |

**Shipped out of band**: **scripting** (C# or Lua) and **networking/multiplayer** are optional
**modules** layered onto a released engine rather than numbered milestones — neither blocks the
version train, and no single-player game carries a transport layer it never uses. Online co-op is
therefore not part of the 1.0 launch.

## Getting Started

**Prerequisites**
- Windows 10/11 with Visual Studio 2026, or Linux (x86-64; verified on Ubuntu 24.04) with GCC 13 or newer
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.4 installed and the `VULKAN_SDK` environment variable set (on Linux, LunarG's tarball and its `setup-env.sh`; distro packages are too old)

**1. Clone the repository**

```bash
git clone --recursive https://github.com/KingoBoiii/DingoEngine.git
```

If cloned non-recursively, fetch submodules afterwards:

```bash
git submodule update --init
```

**2. Generate project files**

On Windows, run [Generate-Windows.bat](Generate-Windows.bat) from the root directory. This will invoke Premake5 and produce a Visual Studio solution with all projects and dependencies configured.

On Linux, first build assimp 6.0.4 as a static library into `vendor/assimp/lib/linux-x86_64/` (see [Getting Started](docs/getting-started.md#option-a--integrate-from-source-recommended)), then run [Generate-Linux.sh](Generate-Linux.sh), which runs the repo's premake (`vendor/premake/bin/premake5 gmake`). VS Code users get Linux build tasks and gdb launch entries in `.vscode`.

**3. Build & run**

On Windows, open the generated `DingoEngine.slnx` in Visual Studio, set one of the example projects (`FlappyBird`, `Breakout3D`, `DungeonCrawler`, `SpaceInvaders`, `AngryBirds`, `DungeonCrawler3D`, `EchoVault`, `ArenaShooter`, `Candlewick`, or `Marionette`) as the startup project, and build.

On Linux, run `make -j"$(nproc)" config=debug` (or `release`, `distribution`, `debug-asan`; add a project name to build just that one), then start an example from its own directory so its `assets/` resolve:

```bash
cd examples/FlappyBird && ../../build/bin/Debug-linux-x86_64/FlappyBird/FlappyBird
```

Linux has been verified headless on Mesa's software Vulkan driver; real GPUs and desktops are still being validated (see [LINUX-SUPPORT.md](LINUX-SUPPORT.md)).

## Examples

| Project | Description |
|---|---|
| `FlappyBird` | Complete 2D game — sprites, input, collision, audio, score rendering |
| `Breakout3D` | 3D Breakout — perspective camera, mesh rendering, manual AABB collision |
| `DungeonCrawler` | Top-down 2D slice — tile collision, chasing enemies, melee combat, loot |
| `SpaceInvaders` | Scene/ECS showcase — EnTT entities and a multi-scene `SceneManager` |
| `AngryBirds` | 2D physics showcase — slingshot launching, destructible block towers, and pig targets on the Box2D-backed physics world |
| `DungeonCrawler3D` | 3D dungeon-crawler prototype — the first ECS-integrated 3D scene: **procedurally generated** dungeons (rooms + corridors), player/enemies/walls as `RigidBody3D` entities on the Jolt-backed `Physics3D`, **melee combat** (SPACE) with enemy health + a player health bar, treasure to collect, a follow camera, drawn via `Renderer3D`; run with `--night` for a dark dungeon lit by a lantern and point-lit treasure |
| `EchoVault` | v0.5 showcase — capsule **character controller** on floating platforms (slopes, stairs, moving kinematic platforms), ray/shape-cast gameplay (patrolling sentry line-of-sight), and **3D positional audio** you navigate by, with full gamepad play; since v0.7 its orbs and sentries are lit emissive materials that carry point lights |
| `ArenaShooter` | v0.6 showcase — wave-based top-down shooter driven entirely by the **`AssetManager`**: async loading behind a progress bar, all sprites/audio/fonts via UUID handles, and **live hot-reload** (edit `assets/shaders/background.glsl` or a sprite PNG while it runs) |
| `Candlewick` | v0.7 showcase — a stealth crawl through a dark keep where every light is a gameplay object: the lantern you carry is a **point light whose radius is your oil**, **wardens** carry point lights and see through **spot-light vision cones** tested with the renderer's own light query (`GetLightAttenuation`), so the cone drawn on the floor is the cone that catches you, a **game-side light budget** keeps the decorative flames and the gameplay lights inside the engine's 32, and **lit emissive braziers** (hold to light) are the checkpoints; light the Chapel altar to win. Move with WASD / arrows / left stick, snuff or relight the lantern with Q / (X), hold E / (A) beside a brazier, pause with Esc / Start; synthesised 3D positional audio, full gamepad play, and `--debug-cone` to draw what the wardens test |
| `Marionette` | v0.8 showcase — a melee duel against three escalating opponents where no combat timing lives in code: every wind-up, hit window, combo window and dodge's invulnerability is a **timeline event** in a `.events` file beside the clips, swords and hit spheres ride **joint sockets**, a **masked upper-body layer** blocks while the legs keep walking, a **`Blend1D`** locomotion blend feeds positional footsteps from step events, and **one set of clip libraries** is retargeted onto four characters. Three AI tiers read the opponent's wind-up through a reaction delay and parry, block or dodge it; edit a `.events` file while it runs (`--hot-reload`, or `--live-edit-demo`) and the fight changes live. Move with WASD / left stick, light attack J / left mouse / X, heavy K / right mouse / Y, hold Shift / right bumper to block (tap to parry), dodge with Space / A, pause with Esc / Start; `--debug-hitbox` draws the hit and hurt spheres, `--check` runs its built-in checks, and `--autoplay` or `--tournament=N` play AI against AI |

## Project Structure

```
include/DingoEngine/    Public API
src/DingoEngine/        Engine implementation
vendor/                 Third-party dependencies (submodules)
examples/               Example projects
```

## Dependencies

| Library | Purpose |
|---|---|
| [NVRHI](https://github.com/NVIDIAGameWorks/nvrhi) | Graphics API abstraction (Vulkan / D3D12) |
| [GLFW](https://github.com/glfw/glfw) | Windowing and input |
| [GLM](https://github.com/g-truc/glm) | Math (vectors, matrices, quaternions) |
| [spdlog](https://github.com/gabime/spdlog) | Logging |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug UI |
| [stb](https://github.com/nothings/stb) | Image loading |
| [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) | Font SDF atlas generation |
| [EnTT](https://github.com/skypjack/entt) | Entity-component system (scenes) |
| [Box2D](https://github.com/erincatto/box2d) | 2D rigid-body physics simulation |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 3D rigid-body physics simulation |

## Credits

*Marionette*'s characters, weapons and animations: KayKit by Kay Lousberg, [www.kaylousberg.com](https://www.kaylousberg.com) — CC0. Each pack's licence text is kept in `examples/Marionette/assets/`.

The test app's Fox (`test/assets/models/Fox`): model by PixelMannen, rigging and animation by @tomkranis, glTF conversion by @AsoboStudio with @scurest — CC-BY 4.0, from the [Khronos glTF Sample Models](https://github.com/KhronosGroup/glTF-Sample-Models/tree/main/2.0/Fox).
