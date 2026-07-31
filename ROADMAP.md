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
and is scheduled as the character-fidelity milestone **v0.8** (below).

## v0.5 — Audio & Gameplay-Grade Physics
The engine foundation is now in place: v0.4.1 moved 3D rendering and the Jolt-backed `Physics3D` world into the scene/ECS, and **v0.4.2 landed the scene rework** — the `OnStart`/`OnUpdate`/`OnStop` lifecycle, `SceneManager`-driven transitions, and the `SceneRenderer` (camera + lights read from ECS components) — so the world already renders and simulates through the scene behind a real renderer abstraction. v0.5 delivers the two remaining pieces of gameplay-supporting engine work:

- **Audio**: a proper audio engine — a real backend (miniaudio), `AudioSource` / `AudioListener` ECS components, and 3D positional sound for footsteps, combat, and ambience. (The engine had no audio at all before this milestone.)
- **Gameplay-grade physics**: the DungeonCrawler3D prototype fakes combat and movement with distance checks and raw velocity. v0.5 promotes them to first-class physics — a reusable **character controller** for player and enemy movement (capsule collider plus ground/step handling), **ray and shape casts** for melee hits and line-of-sight, and the per-body **position / angular control** `Physics3D` previously lacked. This is the depth behind v0.4.1's initial physics-in-the-ECS wiring. Rounding it out from the same wave: `ScreenPointToRay` + ground-plane picking, script-requested scene transitions, and an emissive material channel.

**Example game**: [EchoVault](examples/EchoVault/) — a compact 3D course of floating platforms and vaults built specifically to exercise the two new systems together: capsule character-controller movement (slopes, stairs, moving **kinematic platforms** that carry the player), ray/shape-cast gameplay (a patrolling sentry with line-of-sight detection, hit checks), and 3D positional audio you navigate *by* (chiming collectibles, humming platforms, ambient loops, footsteps). The full dungeon-crawler **game** originally slotted here is developed in its own project on prebuilt engine releases — the engine repo ships the example, the game ships on its own schedule.

## v0.5.1 — Input Rework & Gamepad Support
A point release that replaces the input layer wholesale. The old `Input` mixed live GLFW polling with callback state and had `IsKeyDown`/`IsKeyPressed` semantics **inverted** relative to every other engine — a long-standing footgun.

- **Reworked input core**: a frame-coherent snapshot with the conventional naming — `Is...Pressed` (edge), `Is...Down` (held), plus new `Is...Released` / `Is...Up` — applied uniformly to keys, mouse buttons, and gamepad buttons. All examples and the built-in `F3`/`F4` overlay toggles migrated.
- **Mouse upgrades**: scroll wheel (`GetMouseScrollDelta`) and per-frame cursor movement (`GetMouseDelta`), with new `MouseMovedEvent` / `MouseScrolledEvent` layer events.
- **Gamepad support**: up to 16 controllers via GLFW's gamepad-mapping database — `GamepadButton` / `GamepadAxis` codes (Xbox naming with PlayStation aliases), edge/held button queries, deadzone-filtered axes and stick vectors (configurable radial deadzone, triggers remapped to [0, 1]), and `GamepadConnectedEvent` / `GamepadDisconnectedEvent`.

**Example**: [EchoVault](examples/EchoVault/) gains full controller play — analog left-stick / d-pad movement, `(A)` to jump and confirm menus.

## v0.6 — Asset Pipeline & Hot-Reload
The centralized **`AssetManager`** — the engine-owned registry and owner of file-backed assets, configured via `ApplicationParams::Assets` and documented in [docs/asset-pipeline.md](docs/asset-pipeline.md):

- **UUID handles & path dedup**: every asset path (relative to a configurable **asset root**, retiring the cwd-relative asset trap) maps to a stable 64-bit `AssetHandle`; loading the same file twice returns the same handle and the same object instead of re-reading the file and re-creating GPU resources. Typed access (`GetTexture` / `GetShader` / `GetModel` / `GetFont` / `GetAudioClip`, or `Get<T>`), a `Ready`/`Failed` state machine, and a failure contract that keeps failed loads registered so a later reload can recover. `Texture::CreateFromFile` was aligned with the Model/Font nullptr-on-failure contract along the way.
- **Background loading**: `LoadAsync` decodes textures and audio clips on a loader thread and finalizes GPU uploads on the main thread inside the engine's per-frame pump; shader/model/font requests fall back to amortized main-thread loads (one per frame) so a loading screen keeps animating. `GetPendingCount()` drives progress bars.
- **Hot-reload** (dev-only, opt-in): loaded textures and shaders are timestamp-watched and reloaded **in place** — textures swap contents inside the same `Texture` object (any dimensions), and shaders recompile from source past the bytecode disk cache, bump a generation counter, and every `Pipeline`/`RenderPass` built from them lazily rebuilds at bind time. A shader compile error keeps the previous program running instead of crashing the app.

**Example game**: [ArenaShooter](examples/ArenaShooter/) — a wave-based top-down arena shooter that async-loads every asset behind a progress bar, plays all its audio through manager handles, and renders its animated background with a file-based shader: edit the shader or a sprite PNG while the game runs and watch it update live. The engine test app also gained an interactive **Asset Manager Test** (`test/`, run with `--test=asset`) covering dedup, typed access, the failure contract, and both async paths.

## v0.7 — Lighting & Shading
v0.6 made assets first-class; v0.7 does the same for **light**. Everything the engine has ever
rendered has been lit by exactly one directional light: `DirectionalLightComponent` carries a
direction and an ambient scalar — no colour, no intensity — the `SceneRenderer` takes the
*first one it finds* in the scene, and it reaches the mesh shader as a single `vec4` in the scene
UBO. A game that wants a torch, a lamp, or a muzzle flash has no choice but to fake it on the CPU:
the external dungeon crawler spends roughly 40% of its game controller re-tinting wall, floor and
prop albedo every frame to imitate torch pools, and eventually had to hand-write its own per-pixel
lighting shader to escape that. This milestone retires that entire category of workaround — and
gives v0.9's shadow maps and bloom a real light abstraction to attach to instead of inventing one
late.

- **Real light types**: a `PointLightComponent` (position from the entity's transform, plus colour,
  intensity and range with distance attenuation) and a `SpotLightComponent` (direction with
  inner/outer cone falloff). `DirectionalLightComponent` gains the **colour and intensity it never
  had** and stops being implicitly one-per-scene.
- **A capped forward multi-light path**: the `SceneRenderer` gathers lights each frame and selects
  the N most relevant (nearest / brightest, with off-screen lights culled) into a light array in the
  scene UBO at binding 0, and the lit shader loops them per pixel. A fixed budget deliberately keeps
  v0.4.2's per-material batching and binding layout intact — no deferred pass, no G-buffer. When the
  budget overflows, selection is documented and warned about rather than silently different frame to
  frame.
- **A shading pass worth lighting**: the current shader is pure Lambert plus an ambient lift, so
  extra lights would have nothing to catch — a specular/roughness term lands with them, and the v0.5
  emissive channel becomes the natural companion to a co-located point light ("this object *is* the
  light source"). The lit shader also **moves out of the `Renderer3D.cpp` string literal** onto v0.6's
  file-backed shader path, so it hot-reloads: light falloff and specular response become things you
  tune with the game running.

**Example game**: *Candlewick* — a stealth crawl through a dark keep, built so that every light in
the scene is a gameplay object rather than set dressing. The player carries one lantern whose radius
*is* a burning resource; wardens patrol with their own moving point lights and see through
spot-light vision cones (the cone drawn and the detection tested from the same data, reusing v0.5's
shape casts); braziers with emissive cores are both the checkpoints and the only way to see a room.
Snuffing your lantern hides you and blinds you at once. It stresses the light budget honestly —
many small static flames, a handful of moving ones — and it is *played* rather than looked at.

## v0.8 — Animation & Character Fidelity
This one is a debt the roadmap has carried since v0.4.2. That milestone gave DungeonCrawler3D's hero
a body instead of a sphere, and admitted in the same breath that a real skeletal-animation system
"remains future engine work, slated to land with the character fidelity push of v0.5+" — a promise
v0.5, v0.5.1, v0.6 and v0.7 have all walked past. In the meantime the workaround hardened into the
house style: `Model` loads flat submeshes with baked node transforms and **no bone data at all**, and
entities have **no parent-child relationship**, so every animated character in every project is a
*pile of entities* — seven part-entities for the DungeonCrawler3D hero, 13–24 per character in the
external dungeon crawler — each part's world transform recomputed by hand in game code every frame,
against pivot offsets reverse-engineered out of the model exporter. v0.8 ends that.

- **Transform hierarchy**: a parent/child relationship between entities plus a propagation pass, so a
  child transform is finally *relative*. This is the half that pays off immediately and entirely
  independently of skinning — it deletes the per-part world math games write today (~76 lines in one
  rig alone) and the exporter pivot arithmetic feeding it. Attach points — a sword in a hand, a light
  on a lantern, a turret on a hull — become parenting instead of per-frame bookkeeping.
- **Skinned meshes**: the model loader reworked past static-only — bone hierarchies, vertex weights
  and inverse-bind matrices read from glTF/FBX, with skinning done on the GPU via a joint-matrix
  palette. This is the loader change v0.6 makes affordable rather than painful: rigs become
  `AssetManager`-owned, UUID-handled, hot-reloadable assets like everything else, instead of a
  directory of part OBJs and a generator script.
- **Clips, blending & animation events**: animation clips as assets, an `AnimatorComponent` that
  plays, loops and cross-fades them, and enough of a blend tree for what games actually need — idle
  ↔ walk ↔ run driven by a speed parameter, an upper-body action layered over locomotion, and a
  one-shot that returns to whatever was playing underneath. Plus **events on the timeline** (footstep
  here, hitbox live from here to here), so a swing's damage window comes from the animation instead of
  a hand-tuned timer that drifts every time the art changes.

**Example game**: *Marionette* — a close-quarters duel against an escalating opponent, built so that
no combat timing lives in game code at all. Reach and hit windows come from timeline events on the
clips; telegraphs and recoveries are cross-fades long enough to read and react to; locomotion blends
on one speed parameter while a parry layers over the top; and the same clips retarget across three
fighters of different proportions. If the animation is wrong the fight is wrong — which is precisely
the pressure this milestone needs to be tested under.

## v0.9 — Shadows, Post-processing & VFX
The visual milestone — and the first one that inherits its dependencies instead of inventing them.
v0.7 gives it lights worth casting shadows from, v0.5's emissive channel and v0.7's light budget give
bloom something bright to bleed, and v0.8 gives it animation timelines to hang effects on. It stays
deliberately about **what the frame looks like**; the renderer throughput work that the old
"& Performance" title implied — and never actually scheduled — moves to v1.0.

- **Shadows**: cascaded shadow maps for the directional light, plus shadow casting for a bounded
  subset of v0.7's point/spot budget (omnidirectional shadows are the expensive kind, so the cap *is*
  the design). This is also where lighting stops being decoration: in a game built on light,
  occlusion is gameplay.
- **A post-processing stack**: bloom, tone mapping and SSAO/GTAO, run as a real chain over the scene
  target rather than as one-off effects. Tone mapping isn't cosmetic here — the moment v0.7 lets N
  lights sum past 1.0 the choice is mapping that range or clipping it, and today the engine clips.
- **GPU particles**: an emitter/particle system on the GPU, driven from ECS components, with spawn
  hooks on v0.8's animation timeline so a spell's burst comes from the clip instead of a timer.
- **Profiling integration** (Optick or Tracy), so each new pass can be measured as it lands rather
  than audited afterwards.

**Example**: a visual *and gameplay* upgrade pass over *Candlewick* (v0.7) — the same keep, now with
shadow-casting lanterns, so geometry throws shadows you can hide in and a warden's vision cone is
broken by cover; braziers bloom and their flames become particles. Adding shadows to a stealth game
about light doesn't merely make it prettier, it changes what the player can *do* — the honest test of
whether the feature is real. *Marionette* (v0.8) takes the particle half, with impact and footfall
VFX fired straight from animation events.

## v1.0 — Stability, Performance & Polish
Performance profiling (Optick or Tracy), full API documentation, cross-platform validation
(Linux + Vulkan), and a thorough pass over every system for correctness, ergonomics, and long-term
maintainability. It also takes on the **renderer throughput work** that has been implied since v0.9
was called "Advanced Rendering & Performance" but was never scheduled anywhere:

- **The vertex budget**: `Renderer3D` CPU-transforms *every submitted vertex, every frame*
  (`Renderer3D.cpp:276`), so the vertex count simply **is** the frame budget, and overflowing
  `MaxVertices` drops geometry. Persistent/static batching for never-moving geometry and **GPU
  instancing** for repeated meshes retire that. The draw-call plumbing already exists —
  `CommandList::Draw` takes an `instanceCount`, hardcoded to 1 — so the missing piece is persistent
  buffers, not the API.
- **Culling**: frustum and distance culling, so what gets submitted is bounded by what's *visible*
  rather than by what exists. Games do this by hand today, nulling a mesh per entity.
- **Material sharing**: a shared-material path so the first custom material in a scene doesn't
  fragment the single-batch fast path.

Doing this last is deliberate: optimising a renderer is measurement work, and by v1.0 there is
finally a full frame to measure — lights, skinned characters, shadows and a post chain all present —
instead of a moving target.

**Full game release**: *Dungeon Crawler* (1.0) — the content-complete evolution of the v0.5 singleplayer vertical slice: full combat, loot, and character progression across many levels. The combination of real-time combat and procedural or handcrafted levels makes this the capstone stress test for the engine: hot-loaded assets (v0.6), a fully lit world (v0.7), animated characters (v0.8), and advanced visuals (v0.9). Online co-op is **no longer part of the 1.0 launch** — it follows as a post-release update once the networking module lands. **Released on Itch.io, with Steam as a stretch goal.**

---

## Modules — shipped out of band
Not every system earns a slot in the version train. Some ship as optional **modules**: separately
versioned add-ons that layer onto a released engine, so they neither block a milestone nor force
every game to carry their dependencies.

- **Scripting** *(formerly v0.7)* — C# via .NET CoreCLR or Mono, or Lua: game logic living outside
  the engine binary and iterated on without recompiling. It moved out of the sequence because it is
  **additive to the existing `ScriptableEntity` model rather than a prerequisite for anything after
  it** — nothing in v0.8–v1.0 depends on a hosted runtime — and because embedding one is a large
  enough dependency to be worth opting into. It targets whichever release is current when it lands,
  and brings its showcase with it: **Tower Defense**, with tower placement, targeting and enemy
  pathfinding written entirely in script.
- **Networking & multiplayer** *(formerly v0.8)* — a reliable transport layer (UDP or WebSocket-based),
  client-server architecture, lobby and session management, and replicated ECS state, kept
  game-agnostic so any project can opt into online co-op. It leaves the version train for a different
  reason than scripting: not because it is small, but because **every single-player game would
  otherwise pay for it**, and because the engine reaching 1.0 should mean "stable, documented and
  complete" rather than "still waiting on a transport layer". Replicated state *does* need seams inside
  the scene, so the v1.0 API pass should leave those hooks intact rather than paper over them. It ships
  with the showcase it always had: **2–4 player online co-op** retrofitted onto an existing example.
