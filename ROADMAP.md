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
2D rigid body simulation, AABB/circle collision, and a physics world integrated into the ECS (e.g. `RigidBody2DComponent`, `BoxCollider2DComponent`). FlappyBird's manual collision logic can be replaced as a showcase. A standalone, Jolt-backed `Physics3D` world (3D rigid bodies, box/sphere colliders, forces/impulses) also lands here, driven directly by the game for now — its ECS integration arrives in v0.5.

**Example games**: [Angry Birds](examples/AngryBirds/) — slingshot projectiles, destructible block structures, and circle/box rigid bodies; showcases the 2D physics world and compound collision shapes in a physics-heavy scenario. [DungeonCrawler3D](examples/DungeonCrawler3D/) — began as a tower of dynamic boxes knocked down by firing spheres; in v0.4.1 it became the first ECS-integrated 3D scene (a dungeon-crawler prototype), rendering each body at its simulated Jolt transform.

## v0.5 — First Playable: 3D Dungeon Crawler (Singleplayer)
The milestone where the engine's 3D pieces converge into an actual game. So far 3D rendering (v0.2) and the Jolt-backed `Physics3D` (v0.4) have lived *outside* the scene system — meshes are drawn and bodies stepped by hand (as in Breakout3D and Physics3D), while the ECS (v0.3) is 2D-only. v0.5 closes that gap on three fronts:

- **3D scene & ECS**: extend the v0.3 scene to 3D — a full 3D transform (position, quaternion rotation, scale), a `MeshRendererComponent` drawn through the v0.2 `Renderer3D`, a perspective `CameraComponent`, and basic light components — so dungeons, enemies, and props become ECS entities the scene renders itself. This is the "ECS-integrated 3D scene" the [3D physics docs](docs/physics-3d.md) foreshadow.
- **Physics in the scene**: wire the standalone `Physics3D` world into the ECS the way Box2D is in 2D — `RigidBody3D` plus box/sphere/capsule collider components, a reusable **character controller** for player and enemy movement, and ray/shape casts for melee hits and line-of-sight. The v0.4 world slots in behind the scene exactly as its docs promised.
- **Audio**: the audio engine originally scoped for this milestone ships with the game — a proper backend (miniaudio or OpenAL), `AudioSource` / `AudioListener` ECS components, and 3D positional sound for footsteps, combat, and dungeon ambience, retiring the hardcoded `.ogg` playback in FlappyBird.

**Full game release**: *Dungeon Crawler* (singleplayer — first version) — a compact, atmospheric first-/third-person 3D dungeon crawler, and the 3D evolution of the top-down [DungeonCrawler](examples/DungeonCrawler/) slice. Deliberately a **vertical slice**, not content-complete: one biome, a handful of enemy types, procedurally assembled (or handcrafted) rooms and corridors, real-time melee/ranged combat, enemy AI (chase + simple pathfinding), loot drops with a small inventory, and light character progression — the full explore → fight → loot → descend loop. It exercises the whole stack: 3D rendering and model loading (v0.2), ECS entities for the player, monsters, projectiles, and pickups (v0.3), physics-driven movement and combat (v0.4), and the new audio. It is the singleplayer foundation that v0.8 extends with co-op and v1.0 grows into the capstone. **Released on Itch.io, and Early Access on Steam.**

## v0.6 — Asset Pipeline & Hot-Reload
Centralized `AssetManager` with UUID-based handles, background/async loading, and hot-reload of shaders and textures during development.

**Example game**: Top-down shooter — a wave-based arena shooter with many sprites, particle-like effects, and shader variants. Hot-reloading shaders and swapping textures at runtime serve as the primary development showcase.

## v0.7 — Scripting
C# scripting via Mono or .NET CoreCLR, or Lua. Allows game logic to live outside the engine binary and be iterated on without recompiling.

**Example game**: Tower Defense — towers and enemies with fully scriptable placement, targeting, and pathfinding behavior in C#/Lua. Demonstrates live logic iteration without engine recompilation.

## v0.8 — Networking & Multiplayer
Multiplayer foundation: reliable transport layer (UDP or WebSocket-based), client-server architecture, lobby and session management, and networked ECS state synchronisation. Designed to be game-agnostic so any future project can opt into online co-op.

**Example**: Extend the *Dungeon Crawler* (v0.5) with 2–4 player online co-op using the new networking layer. Serves as the real-world stress test for the entire stack.

## v0.9 — Advanced Rendering & Performance
Shadows (cascaded shadow maps), a post-processing stack (bloom, tone mapping, SSAO/GTAO), a GPU particle system, and profiling integration (Optick or Tracy). Brings the visual and performance bar up to match the ambition of the v1.0 game.

**Example**: Visual upgrades pass over the *Dungeon Crawler* — dynamic shadow-casting lights, bloom on atmospheric effects, and particle-based combat and spell VFX.

## v1.0 — Stability & Polish
Performance profiling (Optick or Tracy), full API documentation, cross-platform validation (Linux + Vulkan), and a thorough pass over every system for correctness, ergonomics, and long-term maintainability.

**Full game release**: *Dungeon Crawler* (1.0) — the content-complete evolution of the v0.5 singleplayer vertical slice: full combat, loot, and character progression across many levels, with online co-op built-in from launch on the v0.8 networking layer. The combination of real-time combat, procedural or handcrafted levels, and networked play makes this the capstone stress test for the engine: scripted game logic (v0.7), hot-loaded assets (v0.6), advanced visuals (v0.9), and networked state sync (v0.8). **Released on Itch.io, with Steam as a stretch goal.**
