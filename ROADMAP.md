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
2D rigid body simulation, AABB/circle collision, and a physics world integrated into the ECS (e.g. `RigidBody2DComponent`, `BoxCollider2DComponent`). FlappyBird's manual collision logic can be replaced as a showcase. A standalone, Jolt-backed `Physics3D` world (3D rigid bodies, box/sphere colliders, forces/impulses) also lands here, driven directly by the game for now — its ECS integration arrives in v0.4.1 (below).

**Example games**: [Angry Birds](examples/AngryBirds/) — slingshot projectiles, destructible block structures, and circle/box rigid bodies; showcases the 2D physics world and compound collision shapes in a physics-heavy scenario. [DungeonCrawler3D](examples/DungeonCrawler3D/) — in v0.4 this began life as `examples/Physics3D/`, a standalone demo driving the Jolt world *by hand*: a tower of dynamic boxes knocked down by fired spheres, each rendered at its simulated transform. Its evolution into an ECS-driven scene is the story of v0.4.1, below.

## v0.4.1 — 3D in the Scene & ECS
A point release that pulls 3D *inside* the scene system, bringing the v0.2 3D rendering and the v0.4 `Physics3D` world under the same ECS that has driven 2D since v0.3. This was the bulk of what v0.5 originally scoped as its first two fronts; landing it early lets v0.5 concentrate on the game itself.

- **`Renderer3D`**: a batched, directional-lit mesh renderer with built-in box and sphere primitives (`GetBoxMesh` / `GetSphereMesh`), drawn with a `PerspectiveCamera` — the 3D counterpart to `Renderer2D`.
- **3D ECS components**: `Transform3DComponent` (position, quaternion rotation, scale), `MeshRendererComponent` (drawn through `Renderer3D`), `RigidBody3DComponent`, and `BoxCollider3DComponent` / `SphereCollider3DComponent` (collider size as a fraction of the transform's scale). 2D and 3D entities coexist in the same `Scene`.
- **Scene-driven physics & rendering**: the `Scene` now builds Jolt bodies from 3D entities, steps the world, and writes simulated transforms back to `Transform3DComponent` — with **per-dimension worlds spun up lazily**, so a 2D-only scene never creates a 3D world. Per-entity control lands as `glm::vec3` overloads (`SetLinearVelocity`, `ApplyImpulse`, `ApplyForce`), alongside `OnRender3D` and `GetPhysics3D()`. The `Physics3D` interface slots in behind the scene exactly as Box2D does in 2D — fulfilling the promise in the [3D physics docs](docs/physics-3d.md).
- **DirectX 3D depth fix**: 3D depth rendering corrected on the D3D11 and D3D12 back-ends (swap-chain depth attachment, non-shader-resource depth textures, no clip-space fixup), so 3D scenes now render correctly across all three APIs.

**Example game**: [DungeonCrawler3D](examples/DungeonCrawler3D/) — the old standalone `Physics3D` demo, rewritten as the **first ECS-integrated 3D scene** and renamed to match. The player, enemies, and walls are all `RigidBody3D` entities the scene simulates and renders; dungeons are **procedurally generated** (rooms + connecting corridors, `DungeonGenerator.h`), and a SPACE-triggered **radial melee swing** damages nearby enemies (60 HP each) while the player has a health bar (100 HP), contact damage, and brief post-hit invulnerability. It is the 3D sibling of the top-down [DungeonCrawler](examples/DungeonCrawler/) slice — and the seed the v0.5 game grows from.

## v0.4.2 — Scene lifecycle, SceneManager & the SceneRenderer
A scene-system rework on two fronts, pulled forward from v0.5:

- **Scene lifecycle & SceneManager-driven transitions.** `Scene` gains an explicit
  `OnStart` / `OnUpdate` / `OnStop` lifecycle — `OnStart` brings up physics
  (`OnPhysicsStart`), `OnStop` tears it down (`OnPhysicsStop`), and `IsRunning()` reports
  the state. `SceneManager` becomes the default way to drive scenes: a single
  `SetActiveScene` stops the outgoing scene and starts the incoming one, and the manager
  is the single update + render entry point for the active scene (retiring the old "only
  the Game scene got `OnUpdate`" footgun). `CreateScene` no longer auto-activates.
- **A `SceneRenderer`.** Per-scene rendering moves behind one `Render(scene)` call
  (driven by `SceneManager::OnRender()`), reading the active **camera** and **lights** from
  ECS components — a unified `CameraComponent` (orthographic or perspective, its view taken
  from the camera entity's transform) and a `DirectionalLightComponent` — and dispatching to
  `Renderer2D` / `Renderer3D` itself, unifying the 2D and 3D entry points. It's the seam
  later milestones (v0.9 shadows / post-processing / particles) plug into.

**Examples**: all four scene-based examples — [Space Invaders](examples/SpaceInvaders/),
[Angry Birds](examples/AngryBirds/), [DungeonCrawler](examples/DungeonCrawler/), and
[DungeonCrawler3D](examples/DungeonCrawler3D/) — were migrated onto the lifecycle and the
camera/light components as the showcase.

## v0.5 — First Playable: 3D Dungeon Crawler (Singleplayer)
The milestone where the DungeonCrawler3D prototype becomes an actual, shippable game. The engine foundation is now in place: v0.4.1 moved 3D rendering and the Jolt-backed `Physics3D` world into the scene/ECS, and **v0.4.2 landed the scene rework** — the `OnStart`/`OnUpdate`/`OnStop` lifecycle, `SceneManager`-driven transitions, and the `SceneRenderer` (camera + lights read from ECS components) — so the world already renders and simulates through the scene behind a real renderer abstraction. That leaves v0.5 to concentrate on the game itself, with two remaining pieces of supporting engine work:

- **Audio**: the audio engine originally scoped for this milestone ships with the game — a proper backend (miniaudio or OpenAL), `AudioSource` / `AudioListener` ECS components, and 3D positional sound for footsteps, combat, and dungeon ambience, retiring the hardcoded `.ogg` playback in FlappyBird.
- **Gameplay-grade physics**: the prototype fakes combat and movement with distance checks and raw velocity. To make the slice genuinely playable, v0.5 promotes them to first-class physics — a reusable **character controller** for player and enemy movement (capsule collider plus ground/step handling), **ray and shape casts** for melee hits and line-of-sight, and the per-body **position / angular control** `Physics3D` still lacks. This is the depth behind v0.4.1's initial physics-in-the-ECS wiring.

**Full game release**: *Dungeon Crawler* (singleplayer — first version) — a compact, atmospheric first-/third-person 3D dungeon crawler, and the 3D evolution of the top-down [DungeonCrawler](examples/DungeonCrawler/) slice. Deliberately a **vertical slice**, not content-complete: one biome, a handful of enemy types, procedurally assembled (or handcrafted) rooms and corridors, real-time melee/ranged combat, enemy AI (chase + simple pathfinding), loot drops with a small inventory, and light character progression — the full explore → fight → loot → descend loop. It exercises the whole stack: 3D rendering and model loading (v0.2) now driven through the new `SceneRenderer`, ECS entities for the player, monsters, projectiles, and pickups (v0.3, extended to 3D in v0.4.1), physics-driven movement and combat (v0.4, brought into the scene in v0.4.1 and made gameplay-grade here), and the new audio. It is the singleplayer foundation that v0.8 extends with co-op and v1.0 grows into the capstone. **Released on Itch.io, and Early Access on Steam.**

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
