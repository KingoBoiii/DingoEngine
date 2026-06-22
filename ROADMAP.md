# DingoEngine Roadmap

## v0.1 — Core Foundation
Core components, windowing, input, and a basic rendering pipeline. ImGui for debugging UI.

**Example game**: [FlappyBird](examples/FlappyBird/) — 2D sprites, input, state machine, collision, and audio. The reference implementation for the engine's 2D capabilities.

## v0.2 — Extended Rendering Pipeline
Extended graphics and rendering pipeline across three fronts:

- **Multi-API back-end**: DirectX 11 and DirectX 12 support alongside Vulkan. The D3D12 implementation via NVRHI (`DirectX12GraphicsContext`) is activated and stabilised; a DirectX 11 path is added for broader hardware compatibility.
- **3D rendering**: A `Renderer3D` API for drawing meshes with perspective cameras. Includes glTF/OBJ model loading, a material system, and basic lighting (Phong or PBR-ready).
- **Rendering thread**: Rendering work is moved to a dedicated thread with a thread-safe command queue, decoupling game logic from GPU submission and enabling overlapped CPU/GPU work.

**Example game**: [Breakout 3D](examples/Breakout3D/) — a 3D take on the classic Breakout/Arkanoid. Showcases the perspective camera, 3D mesh rendering (bricks, ball, paddle), and manual AABB collision without requiring a physics engine.

## v0.3 — Scenes & ECS
Scene management and entity component system (ECS) using the EnTT library.

**Example game**: [Space Invaders](examples/SpaceInvaders/) — a grid of invader entities, player bullets, and destructible shields, each represented as ECS entities. Multiple scenes (menu, game, game over) demonstrate the scene management system.

## v0.4 — Physics & Collision
2D rigid body simulation, AABB/circle collision, and a physics world integrated into the ECS (e.g. `RigidBody2DComponent`, `BoxCollider2DComponent`). FlappyBird's manual collision logic can be replaced as a showcase.

**Example game**: [Angry Birds](examples/AngryBirds/) — slingshot projectiles, destructible block structures, and circle/box rigid bodies. Showcases the physics world and compound collision shapes in a physics-heavy scenario.

## v0.5 — Audio System
Proper audio engine (OpenAL or miniaudio), `AudioSource` / `AudioListener` ECS components, and 3D positional audio. Replaces the hardcoded .ogg playback currently in the FlappyBird example.

**Full game release**: *Micro Roguelike* — a compact, real-time dungeon-crawling roguelike with procedurally generated levels, room-by-room combat, loot drops, and permadeath. A deliberately small-scope game that draws on every system built so far: 3D rendering and model loading for dungeons, enemies, and items (v0.2), ECS entities for the player, monsters, projectiles, and pickups (v0.3), physics-driven collision for movement, melee, and projectiles (v0.4), and audio for ambience and combat feedback (v0.5). **Released on Itch.io.**

## v0.6 — Asset Pipeline & Hot-Reload
Centralized `AssetManager` with UUID-based handles, background/async loading, and hot-reload of shaders and textures during development.

**Example game**: Top-down shooter — a wave-based arena shooter with many sprites, particle-like effects, and shader variants. Hot-reloading shaders and swapping textures at runtime serve as the primary development showcase.

## v0.7 — Scripting
C# scripting via Mono or .NET CoreCLR, or Lua. Allows game logic to live outside the engine binary and be iterated on without recompiling.

**Example game**: Tower Defense — towers and enemies with fully scriptable placement, targeting, and pathfinding behavior in C#/Lua. Demonstrates live logic iteration without engine recompilation.

## v0.8 — Networking & Multiplayer
Multiplayer foundation: reliable transport layer (UDP or WebSocket-based), client-server architecture, lobby and session management, and networked ECS state synchronisation. Designed to be game-agnostic so any future project can opt into online co-op.

**Example**: Extend *Micro Roguelike* (v0.5) with 2–4 player online co-op using the new networking layer. Serves as the real-world stress test for the entire stack.

## v0.9 — Advanced Rendering & Performance
Shadows (cascaded shadow maps), a post-processing stack (bloom, tone mapping, SSAO/GTAO), a GPU particle system, and profiling integration (Optick or Tracy). Brings the visual and performance bar up to match the ambition of the v1.0 game.

**Example**: Visual upgrades pass over *Micro Roguelike* — dynamic shadow-casting lights, bloom on atmospheric effects, and particle-based combat and spell VFX.

## v1.0 — Stability & Polish
Performance profiling (Optick or Tracy), full API documentation, cross-platform validation (Linux + Vulkan), and a thorough pass over every system for correctness, ergonomics, and long-term maintainability.

**Full game release**: *Dungeon Crawler* — a dungeon crawler with combat, loot, and character progression, with online co-op built-in from launch. Co-op runs on the v0.8 networking layer. The combination of real-time combat, procedural or handcrafted levels, and networked play makes this the capstone stress test for the engine: scripted game logic (v0.7), hot-loaded assets (v0.6), advanced visuals (v0.9), and networked state sync (v0.8). **Released on Itch.io, with Steam as a stretch goal.**
