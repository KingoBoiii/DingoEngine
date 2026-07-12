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

## Documentation

Usage guides for building games with the engine live in [docs/](docs/README.md):

- [Getting Started](docs/getting-started.md) — prerequisites, linking the `.lib` + headers, and a minimal app
- [Application & Layers](docs/application-and-layers.md) — entry point, lifecycle, input, events
- [2D Rendering](docs/rendering-2d.md) — quads, circles, text, textures, fonts
- [Scenes & ECS](docs/scenes-and-ecs.md) — entities, components, systems, and scene management
- [2D Physics](docs/physics-2d.md) — rigid bodies, colliders, gravity, and forces/impulses
- [3D Physics](docs/physics-3d.md) — the Jolt-backed `Physics3D`, standalone or ECS-integrated

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full milestone plan (v0.1 → v1.0).

## Getting Started

**Prerequisites**
- Windows 10/11
- [Vulkan SDK](https://vulkan.lunarg.com/) installed and `VULKAN_SDK` environment variable set
- Visual Studio 2026

**1. Clone the repository**

```bash
git clone --recursive https://github.com/KingoBoiii/DingoEngine.git
```

If cloned non-recursively, fetch submodules afterwards:

```bash
git submodule update --init
```

**2. Generate project files**

Run [Generate-Windows.bat](Generate-Windows.bat) from the root directory. This will invoke Premake5 and produce a Visual Studio solution with all projects and dependencies configured.

**3. Build & run**

Open the generated `DingoEngine.slnx` in Visual Studio, set one of the example projects (`FlappyBird`, `Breakout3D`, `DungeonCrawler`, `SpaceInvaders`, `AngryBirds`, `DungeonCrawler3D`, `EchoVault`, or `ArenaShooter`) as the startup project, and build.

## Examples

| Project | Description |
|---|---|
| `FlappyBird` | Complete 2D game — sprites, input, collision, audio, score rendering |
| `Breakout3D` | 3D Breakout — perspective camera, mesh rendering, manual AABB collision |
| `DungeonCrawler` | Top-down 2D slice — tile collision, chasing enemies, melee combat, loot |
| `SpaceInvaders` | Scene/ECS showcase — EnTT entities and a multi-scene `SceneManager` |
| `AngryBirds` | 2D physics showcase — slingshot launching, destructible block towers, and pig targets on the Box2D-backed physics world |
| `DungeonCrawler3D` | 3D dungeon-crawler prototype — the first ECS-integrated 3D scene: **procedurally generated** dungeons (rooms + corridors), player/enemies/walls as `RigidBody3D` entities on the Jolt-backed `Physics3D`, **melee combat** (SPACE) with enemy health + a player health bar, treasure to collect, a follow camera, drawn via `Renderer3D` |
| `EchoVault` | v0.5 showcase — capsule **character controller** on floating platforms (slopes, stairs, moving kinematic platforms), ray/shape-cast gameplay (patrolling sentry line-of-sight), and **3D positional audio** you navigate by, with full gamepad play |
| `ArenaShooter` | v0.6 showcase — wave-based top-down shooter driven entirely by the **`AssetManager`**: async loading behind a progress bar, all sprites/audio/fonts via UUID handles, and **live hot-reload** (edit `assets/shaders/background.glsl` or a sprite PNG while it runs) |

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
