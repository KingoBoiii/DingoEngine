# DingoEngine Roadmap

## v0.1 — Core Foundation
Core components, windowing, input, and a basic rendering pipeline. ImGui for debugging UI.

**Example game**: [FlappyBird](examples/FlappyBird/) — 2D sprites, input, state machine, collision, and audio. The reference implementation for the engine's 2D capabilities.

## v0.2 — Extended Rendering Pipeline
Extended graphics and rendering pipeline across three fronts:

- **Multi-API back-end**: DirectX 11 and DirectX 12 support alongside Vulkan. The D3D12 implementation via NVRHI (`DirectX12GraphicsContext`) is activated and stabilised; a DirectX 11 path is added for broader hardware compatibility.
- **3D rendering**: A `Renderer3D` API for drawing meshes with perspective cameras. Includes glTF/OBJ model loading, a material system, and basic lighting (Phong or PBR-ready).
- **Rendering thread**: Rendering work is moved to a dedicated thread with a thread-safe command queue, decoupling game logic from GPU submission and enabling overlapped CPU/GPU work.

**Example game**: Breakout 3D — a 3D take on the classic Breakout/Arkanoid. Showcases the perspective camera, 3D mesh rendering (bricks, ball, paddle), and manual AABB collision without requiring a physics engine.

## v0.3 — Scenes & ECS
Scene management and entity component system (ECS) using the EnTT library.

**Example game**: Space Invaders — a grid of invader entities, player bullets, and destructible shields, each represented as ECS entities. Multiple scenes (menu, game, game over) demonstrate the scene management system.

## v0.4 — Physics & Collision
2D rigid body simulation, AABB/circle collision, and a physics world integrated into the ECS (e.g. `RigidBody2DComponent`, `BoxCollider2DComponent`). FlappyBird's manual collision logic can be replaced as a showcase.

**Example game**: Angry Birds — slingshot projectiles, destructible block structures, and circle/box rigid bodies. Showcases the physics world and compound collision shapes in a physics-heavy scenario.

## v0.5 — Audio System
Proper audio engine (OpenAL or miniaudio), `AudioSource` / `AudioListener` ECS components, and 3D positional audio. Replaces the hardcoded .ogg playback currently in the FlappyBird example.

**Example game**: 2D Platformer — a Mario-inspired side-scroller with jump SFX, coin pickups, background music, and enemy stomp sounds. Every major audio feature (BGM, positional SFX, AudioSource/AudioListener components) gets a natural demonstration.

## v0.6 — Asset Pipeline & Hot-Reload
Centralized `AssetManager` with UUID-based handles, background/async loading, and hot-reload of shaders and textures during development.

**Example game**: Top-down dungeon explorer — a tile-based dungeon with many sprites, tilemaps, and shaders. Hot-reloading shaders and swapping textures at runtime serve as the primary development showcase.

## v0.7 — Scripting
C# scripting via Mono or .NET CoreCLR, or Lua. Allows game logic to live outside the engine binary and be iterated on without recompiling.

**Example game**: Tower Defense — towers and enemies with fully scriptable placement, targeting, and pathfinding behavior in C#/Lua. Demonstrates live logic iteration without engine recompilation.

## v0.8 — Editor
A standalone `DingoEditor` application built on ImGui: scene hierarchy panel, component inspector, asset browser, and a viewport with transform gizmos (ImGuizmo).

**Example game**: Level-based platformer — levels authored entirely inside DingoEditor using the scene hierarchy, component inspector, and transform gizmos. The shipped levels serve as a reference for editor-driven workflows.

## v0.9 — Serialization
Scene save/load via YAML (yaml-cpp), a prefab system, and project files. Ties the editor to persistent, shareable scenes.

**Example game**: Sokoban — a puzzle game where levels are stored as YAML scenes, crates and goal tiles are prefabs, and player progress is saved and loaded. Exercises every serialization feature cleanly.

## v1.0 — Stability & Polish
Performance profiling integration (Optick or Tracy), full API documentation, cross-platform validation (Linux + Vulkan), and a complete example game shipped alongside the engine.

**Example game**: Mini action RPG — the capstone example. A small but complete 3D RPG that draws on every engine system: 3D rendering, ECS, physics, audio, scripting, editor-built levels, and serialised save data.
