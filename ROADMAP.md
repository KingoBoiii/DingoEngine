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

Alongside the migration, [DungeonCrawler3D](examples/DungeonCrawler3D/) also gained an
**animated low-poly character**. Its hero and skeletons are no longer rendered as spheres
but as multi-part figures built from separate OBJ part meshes — head, torso, arms, legs,
and a sword — loaded with the v0.2 model loader (`Model::LoadFromFile`) and assembled into
a procedural rig: an idle/walk cycle (legs and arms swinging about their joints), an
**attack swing** with anticipation → strike → recovery and a forward body lunge, a
gripped sword that swings with the weapon arm, and a hit-reaction recoil. It is driven
entirely from the example's `ScriptableEntity` scripts on top of the new `SceneRenderer`.
Because the model loader bakes node transforms (no skinned/skeletal playback), the rig
animates by transforming part *entities* each frame — a proper **skeletal-animation
system** (skinned meshes, animation clips, a blend tree) remains future engine work,
slated to land with the character fidelity push of v0.5+.

## v0.5 — Audio & Gameplay-Grade Physics
The engine foundation is now in place: v0.4.1 moved 3D rendering and the Jolt-backed `Physics3D` world into the scene/ECS, and **v0.4.2 landed the scene rework** — the `OnStart`/`OnUpdate`/`OnStop` lifecycle, `SceneManager`-driven transitions, and the `SceneRenderer` (camera + lights read from ECS components) — so the world already renders and simulates through the scene behind a real renderer abstraction. v0.5 delivers the two remaining pieces of gameplay-supporting engine work:

- **Audio**: a proper audio engine — a real backend (miniaudio), `AudioSource` / `AudioListener` ECS components, and 3D positional sound for footsteps, combat, and ambience. (The engine had no audio at all before this milestone.)
- **Gameplay-grade physics**: the DungeonCrawler3D prototype fakes combat and movement with distance checks and raw velocity. v0.5 promotes them to first-class physics — a reusable **character controller** for player and enemy movement (capsule collider plus ground/step handling), **ray and shape casts** for melee hits and line-of-sight, and the per-body **position / angular control** `Physics3D` previously lacked. This is the depth behind v0.4.1's initial physics-in-the-ECS wiring. Rounding it out from the same wave: `ScreenPointToRay` + ground-plane picking, script-requested scene transitions, and an emissive material channel.

**Example game**: *EchoVault* — a compact 3D course of floating platforms and vaults built specifically to exercise the two new systems together: capsule character-controller movement (slopes, stairs, moving **kinematic platforms** that carry the player), ray/shape-cast gameplay (a patrolling sentry with line-of-sight detection, hit checks), and 3D positional audio you navigate *by* (chiming collectibles, humming platforms, ambient loops, footsteps). The full dungeon-crawler **game** originally slotted here is developed in its own project on prebuilt engine releases — the engine repo ships the example, the game ships on its own schedule.

## v0.5.1 — Input Rework & Gamepad Support
A point release that replaces the input layer wholesale. The old `Input` mixed live GLFW polling with callback state and had `IsKeyDown`/`IsKeyPressed` semantics **inverted** relative to every other engine — a long-standing footgun.

- **Reworked input core**: a frame-coherent snapshot with the conventional naming — `Is...Pressed` (edge), `Is...Down` (held), plus new `Is...Released` / `Is...Up` — applied uniformly to keys, mouse buttons, and gamepad buttons. All examples and the built-in `F3`/`F4` overlay toggles migrated.
- **Mouse upgrades**: scroll wheel (`GetMouseScrollDelta`) and per-frame cursor movement (`GetMouseDelta`), with new `MouseMovedEvent` / `MouseScrolledEvent` layer events.
- **Gamepad support**: up to 16 controllers via GLFW's gamepad-mapping database — `GamepadButton` / `GamepadAxis` codes (Xbox naming with PlayStation aliases), edge/held button queries, deadzone-filtered axes and stick vectors (configurable radial deadzone, triggers remapped to [0, 1]), and `GamepadConnectedEvent` / `GamepadDisconnectedEvent`.

**Example**: *EchoVault* gains full controller play — analog left-stick / d-pad movement, `(A)` to jump and confirm menus.

## v0.6 — Asset Pipeline & Hot-Reload
The centralized **`AssetManager`** — the engine-owned registry and owner of file-backed assets, configured via `ApplicationParams::Assets` and documented in [docs/asset-pipeline.md](docs/asset-pipeline.md):

- **UUID handles & path dedup**: every asset path (relative to a configurable **asset root**, retiring the cwd-relative asset trap) maps to a stable 64-bit `AssetHandle`; loading the same file twice returns the same handle and the same object instead of re-reading the file and re-creating GPU resources. Typed access (`GetTexture` / `GetShader` / `GetModel` / `GetFont` / `GetAudioClip`, or `Get<T>`), a `Ready`/`Failed` state machine, and a failure contract that keeps failed loads registered so a later reload can recover. `Texture::CreateFromFile` was aligned with the Model/Font nullptr-on-failure contract along the way.
- **Background loading**: `LoadAsync` decodes textures and audio clips on a loader thread and finalizes GPU uploads on the main thread inside the engine's per-frame pump; shader/model/font requests fall back to amortized main-thread loads (one per frame) so a loading screen keeps animating. `GetPendingCount()` drives progress bars.
- **Hot-reload** (dev-only, opt-in): loaded textures and shaders are timestamp-watched and reloaded **in place** — textures swap contents inside the same `Texture` object (any dimensions), and shaders recompile from source past the bytecode disk cache, bump a generation counter, and every `Pipeline`/`RenderPass` built from them lazily rebuilds at bind time. A shader compile error keeps the previous program running instead of crashing the app.

**Example game**: [ArenaShooter](examples/ArenaShooter/) — a wave-based top-down arena shooter that async-loads every asset behind a progress bar, plays all its audio through manager handles, and renders its animated background with a file-based shader: edit the shader or a sprite PNG while the game runs and watch it update live. The engine test app also gained an interactive **Asset Manager Test** (`test/`, run with `--test=asset`) covering dedup, typed access, the failure contract, and both async paths.

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
