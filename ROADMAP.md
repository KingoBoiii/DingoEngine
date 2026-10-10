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

## v0.6.1 — Textured Meshes in Renderer3D
A point release. `Renderer3D` used to write only position, normal and colour into its batch, so a mesh's UVs never reached the GPU and every ECS mesh drew as one flat colour even with a textured `Material`. The batch vertex now carries the mesh's **`a_TexCoord` at location 3**, so a custom material can bind a texture (binding 2+) and sample it — e.g. an asset kit's shared colour atlas. The built-in default material is unchanged, and existing custom shaders keep working without edits: on Vulkan a pipeline now drops trailing vertex attributes its shader doesn't read (reflected from the SPIR-V, so no "attribute not consumed" validation warning), and D3D ignores them.

**Example**: [Gloomdelve](https://github.com/KingoBoiii/Gloomdelve) renders the Kenney Graveyard Kit with its colormap through its night-lighting material.

## v0.6.2 — Game-Workaround Cleanup
A point release that turns the workarounds the shipped games wrote around missing engine API into engine API:
- **Cursor modes**: `Input::SetCursorMode(CursorMode::Normal | Hidden | Locked)`, with raw mouse motion while Locked (`SetRawMouseMotion`, `IsRawMouseMotionSupported`). This replaces the hand-declared `extern "C" glfwSetInputMode` in Gloomdelve and DingoCraft. `GetMouseDelta()` reads zero for 2 frames after a mode change or refocus, so the games' "skip the jump" counters go away. ImGui no longer resets a Hidden cursor.
- **Window focus**: `WindowFocusEvent` and `Window::IsFocused()` replace polling `GLFW_FOCUSED` for auto-pause on alt-tab.
- **Any input**: `Input::IsAnyKeyPressed/Down` and `IsAnyMouseButtonPressed/Down` replace loops over hardcoded GLFW key ranges.
- **Executable-relative paths**: `Platform::GetExecutablePath/GetExecutableDirectory` and `FindDirectoryUpward("assets")` replace three games' own `GetModuleFileNameW` + parent-walk asset lookup.
- **3D audio attenuation**: `SoundAttenuation` (model None / Inverse / Linear / Exponential, min/max distance, rolloff, min/max gain) per sound via `SoundPlayParams::Attenuation`, on live sounds via `AudioEngine::SetAttenuation`, and as the engine-wide default via `SetDefaultAttenuation` (which the positional `PlayOneShot` also uses). `AudioSourceComponent` gains the same optional field. Defaults are unchanged. This replaces Gloomdelve's trick of placing every voice 1 m from the listener and fading it by hand.
- **Mesh colliders**: 3D physics only knew boxes, spheres and capsules, so level geometry had to be approximated with primitives. `ColliderShape3D` gains `Mesh` (the triangles themselves — terrain, ramps, stairs, a kit-built level; Static and Kinematic bodies) and `ConvexHull` (the hull of the vertices; any body type), and the ECS gains `MeshCollider3DComponent`, which by default collides as whatever the entity's `MeshRendererComponent` draws, at the transform's full scale. Each mesh's shape is baked once and shared by every body built from it at any scale, and the data is copied, so freeing the `Mesh` later is safe. Because a triangle has no thickness, bodies gain an opt-in `ContinuousCollision` flag that sweeps them along their motion, so fast projectiles cannot tunnel through a mesh, and the `Scene` now takes one Jolt collision step per 1/60 s (up to 4 a frame) instead of always one, so a low frame rate no longer drops falling bodies through thin geometry.
- **UTF-8 text** (known bugs K2 + K3): `DrawText` and `GetStringWidth` decode UTF-8 instead of walking signed bytes, so the Latin-1 half of the atlas (`é`, `ü`, `£`, `°`) finally draws, and the atlas also bakes the printable General Punctuation and `€` — em-dashes, curly quotes and ellipses no longer render as garbage. Other codepoints draw as `?`; bytes that aren't valid UTF-8 read as Latin-1. Cached atlases regenerate once.

**Test**: the test app's new **Cursor Test** (`--test=Cursor`) and the F5 Input tab's Cursor section, its **Mesh Collider Test** (`--test=collider`) — a triangle-mesh terrain bowl with a kinematic mesh lift rising through it, pelted with convex-hull pebbles, spheres and boxes, checking that the terrain answers ray casts at its true height and that nothing sinks through it, and UTF-8 lines in the **Text Test** (`--test=Text`).

## v0.6.3 — Known-Bug Sweep
A point release that closes every open entry in [KNOWN-BUGS.md](https://github.com/KingoBoiii/DingoEngine/blob/v0.6.3/KNOWN-BUGS.md) (the known-bug list of the time) except the two deliberate deferrals: K10 (GLM in public headers, waiting for the next API break) and K11 (moving a live device to another GPU).
- **Null asset handles** (K4): a default-constructed `UUID` — and so `AssetHandle` — is now 0, which is `k_InvalidAsset`; fresh ids come from `UUID::Generate()`. The default constructor used to roll a random value, so an unset handle member passed `IsValidAssetHandle` and then resolved to nothing.
- **Debug-ASan builds** (K5): the configuration links (the STL's container annotations now agree with the Vulkan SDK's non-ASan prebuilts) and every executable gets the ASan runtime DLL beside it.
- **One meaning for a relative path** (K7): the raw file factories (`Font::Create`, `Texture::CreateFromFile`, `Model::LoadFromFile`, `Shader::CreateFromFile`, `AudioEngine::LoadClip`) look a relative path up under the asset root first, as the `AssetManager` does, and fall back to the working directory, so existing `"assets/..."` calls load the same files as before.
- **Runtime handles out of the components** (K8): an entity's live physics body, 2D shapes, character controller and sound are engine-owned instead of fields on its public components, so assigning one entity's `RigidBody3DComponent` onto another's can no longer make both drive one body. **Breaking**: `RuntimeBody`, `RuntimeShape`, `RuntimeController`, `RuntimeSound` and `CharacterController3DComponent::k_InvalidControllerIndex` are gone; read the handles with `Scene::GetRuntimeBody2D/3D(entity)` and `Scene::GetRuntimeSound(entity)`.
- **No dropped meshes** (K9): a material that outgrows a `Renderer3D` batch spills into another draw call instead of losing the rest of the scene in a shipping build. Only a single mesh bigger than a whole batch is still dropped.
- **Swap-chain attachments** (K12 + K13): the depth attachment is stored and the colour attachment loaded, so both survive NVRHI's mid-frame render-pass restarts by the spec rather than by driver goodwill — RenderDoc replays a 3D scene correctly again — and a GPU without `VK_KHR_load_store_op_none` is no longer rejected at device selection.
- **`Renderer2D::GetOutput` out-of-bounds read**: the swap-chain framebuffer owns no `Texture`, so `GetOutput()` indexed an empty attachment list. `Framebuffer::GetAttachment` now returns `nullptr` for an index it doesn't have, and `GetOutput()` returns `nullptr`.

**Test**: the test app's new **Renderer3D Batch Test** (`--test=batch`), an assignment-aliasing check in the **Mesh Collider Test**, and a raw `Font::Create` root-relative path check in the **Asset Manager Test**.

## v0.7 — Lighting & Shading
v0.6 made assets first-class; v0.7 does the same for **light**. Every 3D scene the engine had rendered was lit by exactly one directional light with no colour and no intensity, so a game that wanted a torch, a lamp or a muzzle flash had to fake it on the CPU: the external dungeon crawler spent roughly 40% of its game controller re-tinting wall, floor and prop albedo every frame to imitate torch pools, and eventually hand-wrote its own per-pixel lighting shader to escape that. The engine work below retires that category of workaround and gives v0.9's shadow maps and bloom a real light abstraction to attach to instead of inventing one late. The engine work has been through a review pass, and it and the *Candlewick* example game below shipped as v0.7.0.

- **Real light types**: `Renderer3D::SubmitLight` takes a `DirectionalLight` (colour and intensity; up to 4 per scene), a `PointLight` or a `SpotLight` (position, colour, intensity and range, plus a direction and an inner/outer cone angle for the spot), and `SetAmbientLight(colour, intensity)` sets a coloured ambient. The types live in `Graphics/Light.h`. Point and spot lights fall off smoothly from their intensity at the light to exactly zero at `Range`, as `(1 - (d / Range)²)²`. Lighting is scene-scoped: whatever is submitted lights the next `EndScene`, which then clears it. A scene that submits no light and no ambient is lit by the `Renderer3DParams` default light, which reproduces the pre-v0.7 image pixel for pixel.
- **A capped forward multi-light path**: point and spot lights share one budget of 32 (`Renderer3DCapabilities::MaxLocalLights`, read back with `GetLocalLightBudget()`). `EndScene` culls the lights whose range sphere is outside the view frustum, ranks the rest by brightness as seen from the camera and keeps the top N; ties go to the light the camera is nearer to relative to its range, then to the earlier submission, so a still scene picks the same lights every frame, and an overflow warns once and counts in `Statistics::DroppedLights`. The lights ride at the end of the scene UBO at binding 0, behind a frozen 96-byte prefix (`ViewProjection`, `LightDirection`, `Ambient`), so every existing custom material compiles and renders unchanged. v0.4.2's per-material batching is intact: no deferred pass, no G-buffer.
- **Light components**: `PointLightComponent` and `SpotLightComponent` (position, and the spot's aim, from the entity's `Transform3DComponent`; `Enabled` snuffs one without losing its settings) and `AmbientLightComponent`. `DirectionalLightComponent` gains the colour and intensity it never had and stops being one-per-scene. The `SceneRenderer` submits all of them through the new `Scene::SubmitLights`, which is public for custom 3D passes. A scene with no light component at all still gets a default directional light, so a 3D scene never renders black by accident.
- **A shading pass worth lighting**: normalised Blinn-Phong specular per light, shaped by two new `MaterialParams`, `Roughness` and `Specular` (0 by default, so existing materials render as before). `Renderer3D::CreateLitMaterial` makes a material that uses the lit shader with its own emissive, roughness and specular and an albedo texture in slot 0. Per-object glow therefore no longer needs a custom shader: the v0.5 emissive channel, which only the one shared default material ever received, now works on any lit material, and pairs with a co-located point light ("this object *is* the light source"). `Material::SetTexture` / `SetSampler` now rebind when a slot changes, so a texture that arrives after the first draw (a `LoadAsync` result) shows up.
- **The lit shader is a file**: it moved out of the `Renderer3D.cpp` string literal into `src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl`. The build embeds it into `DingoEngine.lib` (`scripts/embed.lua`, run as a premake custom build rule), so a game ships no engine files; Debug builds load the source file instead and, with asset hot-reload enabled, reload it on the `AssetManager`'s poll, so light falloff and specular response become things you tune with the game running.
- **Light stats**: the F4 Renderer tab shows the directional lights in use out of 4, a bar of point and spot lights against the budget, how many were out of view and how many were dropped.
- **Gameplay queries** (added with the example game): `GetLightAttenuation(light, point)` (`Graphics/Light.h`) returns the weight the lit shader gives a `PointLight` or `SpotLight` at a world point, the falloff times the cone for a spot, both already squared. `PointLightComponent::ToLight(transform)` and `SpotLightComponent::ToLight(transform)` build the light the `SceneRenderer` submits for a component. They run on the renderer's own angle clamps and cone set-up (`Graphics/LightMath.h`), so a game tests exactly the cone it draws, instead of copying a formula that can drift from a hot-reloaded shader. The weight leaves out `N·L`, colour, intensity, occlusion and the frame's budget; see [docs/lighting.md](docs/lighting.md#gameplay-queries).
- **Vulkan wireframe fix**: `FillMode::Wireframe` has always been in `MaterialParams`, but the Vulkan device never requested `fillModeNonSolid`, so the first wireframe material (Candlewick's `--debug-cone`) logged a validation error. The feature is now requested whenever the GPU supports it; a wireframe material on a GPU without it was filed as K19, now [#78](https://github.com/KingoBoiii/DingoEngine/issues/78), and draws solid since v0.8.3.
- **Migration**: no API breaks, but two behaviours change. Several `DirectionalLightComponent`s now all light the scene (up to 4, each adding its own legacy `Ambient`), where before only the first counted; and 3D drawn on the shared renderer outside the `SceneRenderer`, without `Scene::SubmitLights`, is lit by the `Renderer3DParams` default light instead of the last scene's sun. The frozen scene-UBO prefix still carries only the first directional light, so a custom shader that wants the rest reads the full block. Two limits are filed as issues: lights can pop at the budget edge ([#75](https://github.com/KingoBoiii/DingoEngine/issues/75), formerly K16), and bright overlapping lights clip until v0.9's tone mapping ([#76](https://github.com/KingoBoiii/DingoEngine/issues/76), formerly K17).

**Example game**: [Candlewick](examples/Candlewick/) — a stealth crawl through a dark keep of four rooms
(the Gatehouse, the Great Hall, the Gallery and the Chapel), built so that every light in the scene is a
gameplay object rather than set dressing. The player carries one lantern whose radius *is* a burning
resource: it burns 1 oil a second, its range shrinks from 7 m to 2.5 m as the oil runs down, and flasks
and lit braziers refill it. Four wardens patrol with a lamp (a point light) and an eye (a spot light)
each, and see through that spot-light cone: the engine's `GetLightAttenuation` weighs the cone at three
points on the player, behind a line-of-sight ray (v0.5's ray casts), so the cone drawn on the floor is
the cone that catches you, and each eye's range is clamped to the wall it faces so a cone never reaches
through one. The lantern is the other half: a lit lantern (or standing in a sconce's or brazier's light)
makes a warden notice you from further away and walk over to investigate, but only the cone catches.
Snuffing the lantern with Q hides you from that and blinds you at once. Braziers with emissive cores
start cold, apart from the Gatehouse's: hold E (or A on a pad) for a second beside one with the lantern
lit and it kindles, becomes the room's main light, refills the lantern and saves your checkpoint. The
same hold at a lit brazier refills the lantern again (and relights it), which is the way out once the
oil and the flasks are gone.
Lighting the Chapel altar wins, and the End screen shows the time and how often you were caught. All
sound is synthesised by a script and positional (a crackling brazier, footsteps, a warden's alert), and
every control has a gamepad binding.

It stresses the light budget honestly — 58 local lights (49 static flames, the lantern, eight warden
lights) against the budget of 32 — and stays inside it on purpose: a game-side light LOD counts the
gameplay lights in view with the engine's own frustum test and fades decorative flames in and out to
fit, so the engine never has to drop a warden's cone while it still sees you. Launch
flags make each claim checkable: `--debug-cone` draws the tested cones and the sample points,
`--no-light-lod` shows the engine's own selection, `--light-budget=<16-32>` lowers the budget (16 is the 14 gameplay lights plus the LOD's headroom), and
`--room=1..4`, `--freeze`, `--overview`, `--oil=<0-100>` and `--spawn=<col>,<row>` make a frame
reproducible. It is *played* rather than looked at.

**Test**: the test app's new **Lighting Test** (`--test=light`), the first test of `Renderer3D`'s lighting. Its modes (`--lighting=default|lights|overbudget|materials`) cover the default light, orbiting point and spot lights, more lights than the budget, and lit materials — a roughness row, an emissive lamp holding a point light, a textured crate. `--entities` drives the same lights through ECS components and `--specular=off` gives a before/after on one frame. It also lists PASS/FAIL checks, among them the `GetLightAttenuation` weights. [DungeonCrawler3D](examples/DungeonCrawler3D/) gains an opt-in `--night` (a dim moon, a lantern above the hero, a point light per treasure) and `--seed=<n>`; its default look is unchanged.

## v0.7.1 — Transform Hierarchy
A point release that ships the first half of v0.8 early. Parent-child transforms pay off
immediately and need no skinning, as v0.8's own text said: they delete the per-part world maths
games write by hand and turn attach points into parenting. So they ship now, and v0.8 keeps
skinning, clips, blending and events.
- **Parenting** (3D and 2D): `Entity::SetParent(parent, keepWorldTransform = true)`, `RemoveParent`, `GetParent`, `GetChildCount`, `ForEachChild`, `GetChildren` and `FindChild(name, recursive)`. `Transform3DComponent` and `TransformComponent` become local to the parent. 3D reads and writes world values with `GetWorldTransform/Position/Rotation/Scale` and `SetWorldPosition/Rotation`. 2D has `GetWorldPosition2D/SetWorldPosition2D/GetWorldRotation2D/SetWorldRotation2D`: a 2D child's position turns with its parent's rotation, z and rotation add, and `Size` is not inherited. Cycles and parents in another scene are refused with an error. A root keeps its component's own values, so a scene without parents renders exactly as before.
- **Lifetime**: destroying an entity destroys its subtree, and duplicating one duplicates its subtree with the links.
- **Every reader on world values**: 3D meshes, 2D sprites, circles and text (at equal z a parent draws before its children), point and spot lights, the camera and audio sources and listeners all use world transforms.
- **Physics**:
  - Bodies are built from the world pose and world scale.
  - Dynamic and character-controller children are simulated in world space and write back the local transform that reproduces it.
  - Kinematic children follow their parent: they are moved before each step to where the parent will be after it.
  - Static children are placed once.
  - 2D does the same through the new `Physics2D::MoveKinematic` and `GetAngularVelocity`.
- **Cheap worlds**: every pass over many entities (rendering, lights, audio, the physics bake, write-back and kinematic follow) works out each world transform once, parents first, and keeps nothing past the pass. In a 10,110-entity stress scene, `RenderEntities3D` on a parented scene costs 1.04x the flat one in Release (1.57x before). A flat scene costs what it did on v0.7, within measurement noise.
- **DungeonCrawler3D's characters are parented rigs**: a root at the feet, a joint per hip and shoulder, and each part (and the sword) under its joint. The per-part world maths is gone, and the parts land where the old maths put them.
- **EchoVault's sentry eye** rides its sentry, and line of sight now starts at the eye. The old ray started inside the sentry's own box and was always blocked, so sentries never saw the player. They now detect the player.

**Test**: the test app's new **Hierarchy Test** (`--test=hierarchy`), 41 checks covering the API, reparenting, destroy and duplicate, world readers and the physics rules in 3D and 2D. Its modes are `--hierarchy=2d`, `probe2d` (a 2D draw-position probe), `stress` and `stressflat` (the 10k-entity timing pair).

**Known limit**: bodies in one hierarchy are not filtered against each other, so a parent's and a child's colliders can collide. Narrowed in v0.8: a kinematic child now ignores its ancestors' bodies; dynamic and static children still collide with them.

## v0.7.2 — Updating in the Background
A point release for a v0.7.0 regression, found while bumping the co-op game *Headstone* to v0.7.1.
v0.7.0 stopped rendering into a minimized window's (0,0) swap chain by skipping the whole frame,
`OnUpdate` included, with no way to opt out (known bug K20). A game that pumps its network or
simulation in `OnUpdate` froze while minimized, and exclusive fullscreen minimizes on every alt-tab.
In *Headstone*'s co-op over TCP, a minimized host dropped its client after the 5 s session timeout,
and a minimized client was dropped by the host.
- **`ApplicationParams::UpdateInBackground`** (and `Application::SetUpdateInBackground` at runtime) chooses what an app does while its window is minimized or unfocused:
  - **Off, the default: it pauses.** No `OnUpdate`, no rendering, and the paused time is left out of the deltas; audio keeps playing. An unfocused window shows its last frame. **Behaviour change**: v0.7.1 paused only while minimized and kept running while unfocused; a game that should keep running behind another window sets the flag.
  - **On: layers keep updating** with the real delta time, and asset loads carry on. An unfocused window renders as usual. A minimized one waits on window events between updates, aiming at 60 a second (Windows' default timer gives about 35, at under 1% of a core), and renders nothing: the new `Renderer::SkipFrame()` parks the render thread as `BeginFrame` does. Until the next `BeginFrame`, every `Renderer` upload, clear and draw, every `Renderer2D` and `Renderer3D` scene and `SceneRenderer::Render` is a no-op, so a game that renders from `OnUpdate` needs no guard. New queries: `Application::IsMinimized()` and `Renderer::IsFrameSkipped()`.
  - Either way every key and button edge reaches exactly one `OnUpdate`.
- **The test app no longer crashes when minimized** (K14): it skips the viewport resize while ImGui reports the panel with no area, which happens in the frame a minimize lands in. The test app opts in to `UpdateInBackground`.

**Test**: the test app's new **Background Test** (`--test=background`, `--update-in-background=off`, or the Properties checkbox). Minimize the window or click away, then come back, and it checks the stretch. With the flag on: updates kept coming (their rate and the longest delta), their delta times add up to the wall clock, and only minimized updates skipped rendering. With it off: no update ran, and neither of the first two deltas after the pause holds the paused time. In both modes, no key edge was lost or doubled. Run against an engine that pauses regardless of the flag, it fails.

**Known limit**: the first Vulkan frame after startup, or after a minimized stretch in which assets were uploaded, is not ordered after its swap-chain image acquire. It is harmless in practice and predates v0.7.2; filed as K21, now [#79](https://github.com/KingoBoiii/DingoEngine/issues/79), and fixed in v0.8.3.

## v0.8 — Animation & Character Fidelity
This one is a debt the roadmap has carried since v0.4.2. That milestone gave DungeonCrawler3D's hero
a body instead of a sphere, and admitted in the same breath that a real skeletal-animation system
"remains future engine work, slated to land with the character fidelity push of v0.5+" — a promise
v0.5, v0.5.1, v0.6 and v0.7 have all walked past. In the meantime the workaround hardened into the
house style: `Model` loads flat submeshes with baked node transforms and **no bone data at all**, so
every animated character in every project is a *pile of entities* — seven part-entities for the
DungeonCrawler3D hero, 13–24 per character in the external dungeon crawler — posed part by part in
game code every frame, against pivot offsets reverse-engineered out of the model exporter. v0.7.1
gave those parts parents; v0.8 replaces the pile with a skinned mesh that plays real animation.

- **Transform hierarchy**: shipped early as [v0.7.1](#v071--transform-hierarchy).
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

**Status**: released as v0.8.0 on 2026-10-05. The engine work (P4–P12 of
`.claude/plans/2026-10-01-v0.8-animation-plan.md`, reviewed in `.claude/reviews/2026-10-03-v0.8.0-review.md`)
and the example game, *Marionette* (P13, M0–M6, closed by a milestone review in
`.claude/reviews/2026-10-04-marionette-review.md`), shipped together. Built:
- **Skinned models**: `Model::LoadFromFile` reads skeletons, skin weights and clips from glTF and FBX, including clip libraries (clips without meshes). Static models load exactly as before.
- **GPU skinning** on Vulkan, D3D11 and D3D12: `SkinnedMeshRendererComponent`, or `Renderer3D::SubmitSkinnedMesh` outside a scene. A model's joint palette uploads once a frame, for up to 64 models by default (256 at most), at up to 128 joints a draw. A custom shader can skin through `DE_SKINNED` and a `SkinData` block.
- **The animator**: cross-fades, `Blend1D` on a float parameter with the clips kept in step, masked layers (an upper body over locomotion), one-shots that return to what they interrupted, and retargeting by joint name. It runs standalone or as an `AnimatorComponent`.
- **Timeline events**: instants and ranges, written in code or in a `.events` file beside the model, delivered to `ScriptableEntity::OnAnimationEvent` or polled. Only the clip a layer shows fires, so a blend never doubles a footstep.
- **Joint sockets**: `SetParent(character, "b_RightHand")` hangs a sword on a hand. A kinematic child now ignores its ancestors' bodies, which narrows v0.7.1's known limit.
- **Model hot-reload in place**: a saved model, or its `.events` file, reloads into the same objects, so the game's pointers stay valid.
- **Debugging**: the **F7** Animation tab (every animator's layers and states, the last 20 events), skinning stats in **F4**, and a skeleton overlay in the test app.
- **A retargeting fix found by Marionette**: KayKit's clips key a still translation on `root`, which made `root` the "root-most animated joint", so every retargeted clip lost its hips' motion. Retargeting now counts a translation track only when it leaves the source joint's rest offset (`a81727f`).

**Test**: the test app's **Animation Test** (`--test=anim`): 78 checks in its default `bind` mode across loading, skinning, the animator, blending, events, sockets and hot-reload, on all three backends. Its modes are `--anim=bind|bindstatic|pose|clip|blend|layers|events|crowd`, with `--anim-skeleton` and `--anim-reload`. Guide: [docs/animation.md](docs/animation.md).

**Not in v0.8**: root motion, IK, additive layers and state machines as assets, and skinned shadows (v0.9).

**Example game**: [Marionette](examples/Marionette/) — a one-arena melee duel against three opponents
in a row (the Recruit, the Veteran, the Champion), each faster and smarter than the last, built so that
no combat timing lives in game code at all. The `.events` files beside the clips hold the whole combat
design: `windup` (the telegraph the AI reads), `hitbox` (the swing's active frames), `combo` (where a
second press chains), `dash`, `iframes` and `parry`, plus `step_l` / `step_r` for positional
footsteps. Swords and shields hang on hand sockets, and the hit and hurt spheres are socketed
entities too, so the spheres `--debug-hitbox` draws are the ones tested. Locomotion is a `Blend1D` on
one `Move` parameter; a block is a layer masked from the spine up, so a fighter walks and blocks at
once; attacks, dodges and hit reactions are one-shots. One `Fighter` class serves the player and the
AI, and the AI sees the opponent through a reaction delay, so a tier-3 parry depends on reading the
wind-up in time. Four characters (the Knight, the Barbarian and two skeletons) play the same five
KayKit clip libraries, retargeted by joint name; the free packs share one body, so they differ by
weapon, pace and uniform scale rather than limb length. Edit a `hitbox` range in a saved `.events`
file while the game runs (`--hot-reload`, or `--live-edit-demo`, which does it after ten seconds)
and the next swing changes. If the animation is wrong the fight is wrong — which is precisely the
pressure this milestone needs to be tested under.

Its flags make runs checkable: `--check` (asset, movement, combat and AI checks, read from the log),
`--drive=duel|ramp|circle|strafe|wall` (scripted input), `--autoplay` and `--tournament=N` (seeded
AI-vs-AI bouts), `--freeze --pose=<clip>@<s>` with `--debug-hitbox` (a frozen frame), and
`--fixed-dt=<s>` for repeatable runs. Marionette also found six engine gaps, filed as issues
[#99](https://github.com/KingoBoiii/DingoEngine/issues/99)–[#104](https://github.com/KingoBoiii/DingoEngine/issues/104).

## v0.8.1 — Winding and Culling
A point release for two bugs found by *Headstone*, whose custom night material started culling back
faces. No custom `Material` could cull an engine mesh correctly: `CullMode::Back`, the default,
drew boxes and every loaded model inside out, and the one workaround, `CullMode::Front`, read like
the opposite of what it did.
- **`Material` passes `MaterialParams::FrontCounterClockwise` to its pipeline** ([#68](https://github.com/KingoBoiii/DingoEngine/issues/68)). It used to drop the flag, so every material treated clockwise triangles as front faces.
- **`Mesh::CreateSphere` is wound counter-clockwise seen from outside** ([#69](https://github.com/KingoBoiii/DingoEngine/issues/69)), like `CreateBox` and loaded models. A sphere used as a triangle-mesh collider is now solid from outside.
- **`MaterialParams::FrontCounterClockwise` defaults to `true`**, so a plain `MaterialParams()` culls back faces correctly on Vulkan, D3D11 and D3D12. **Behaviour change**: code that culled with `CullMode::Front` to work around #68 should switch to `CullMode::Back`. Lit materials still draw both faces.
- `PipelineParams::SetFrontCounterClockwise` joins the other pipeline setters.

**Test**: the Mesh 3D Test checks at start that `CreateBox` and `CreateSphere` (at three detail levels) wind every triangle outward, and takes `--mesh=box|sphere` and `--mesh-angle=<deg>`. The Model 3D Test's models, which used to draw inside out, now show their outside.

## v0.8.2 — Start-up Without a GPU
A point release for two start-up failures found on a Hyper-V VM with no GPU, whose only Direct3D
adapter is WARP (Windows' software rasterizer, "Microsoft Basic Render Driver") and which has no
Vulkan driver. DirectX 11 already ran there, because it takes the default adapter. Checking what
that machine drew turned up a third bug, in blending, which every machine has.
- **DirectX 12 falls back to WARP** when no hardware adapter supports feature level 12_0, logs a warning, and reports the adapter as `AdapterDeviceType::Software`. It used to skip every software adapter and assert `No suitable DirectX 12 GPU found.` Hardware selection is unchanged.
- **Vulkan reports a missing driver instead of crashing.** With required instance extensions or layers missing, start-up printed them to stdout and then crashed on a null instance. They now go to the engine log, and start-up stops with a `DE_CORE_VERIFY`, as a failed device selection already did.
- **Translucent draws keep a render target's alpha.** Every pipeline wrote the destination alpha as the source's, so text, a half-transparent quad or an anti-aliased edge left the pixel under it as transparent as itself. A window never shows its swap chain's alpha, but a framebuffer composited afterwards does: the test framework's viewport let the panel behind it through as a dark box around every glyph. The alpha now composites like the colour (`a = src + dst·(1 − src)`), and colours are unchanged.

WARP renders on the CPU: enough to run the test framework, not to measure anything.

**Test**: none new. On the VM, the test framework starts on DX12 and its Lighting, Renderer3D Batch, Mesh 3D and Animation checks pass; `--graphics=vulkan` shows the assert dialog naming the missing extension. The Text Test's glyphs lose their boxes and the Color Quad Test's 50% white quad is half white over the clear colour instead of grey, while the 3D tests and SpaceInvaders' menu are pixel-identical.

## v0.8.3 — Bug Sweep and Headstone Requests
A point release that closes all nine open GitHub issues labelled `bug`: six carried over from the
known-bug list (K18, K19 and K21 from the v0.7 reviews, K22–K24 from the v0.8 one), one found by
AddressSanitizer on Linux and two found by *Headstone*. It also takes five of *Headstone*'s
feature requests, listed after the fixes.
- **A texture created at a freed texture's address is noticed** ([#77](https://github.com/KingoBoiii/DingoEngine/issues/77), K18). Every texture now draws its generation from one shared counter, so a material's cached render pass, which compares generations at bind time, binds the new texture instead of drawing the old image. Clearing the slot first is no longer needed for textures; samplers are still told apart by pointer.
- **Wireframe falls back to solid on a GPU that can't draw lines** ([#78](https://github.com/KingoBoiii/DingoEngine/issues/78), K19). `GraphicsContext::SupportsWireframe()` reports whether Vulkan's `fillModeNonSolid` was enabled. Without it, a `FillMode::Wireframe` pipeline is built solid, with a one-time warning, instead of in a line mode the device never enabled.
- **A Vulkan frame waits for its own swap-chain image** ([#79](https://github.com/KingoBoiii/DingoEngine/issues/79), K21). The acquire's semaphore wait is queued just before the frame's command list executes. Before, it was queued at the acquire, so whatever was submitted in between took it: assets loaded in `OnAttach`, async loads, `DirectUpload` buffers, ImGui viewports. After a minimized stretch, `BeginFrame` applies the pending resize and acquires an image before anything records, and skips the frame if it still gets none. `SwapChain` gains `IsImageAcquired()` and `QueueImageWait()`, which do nothing on D3D11 and D3D12.
- **Binding an animator to another skeleton ends its open ranges** ([#80](https://github.com/KingoBoiii/DingoEngine/issues/80), K22). Each sends its `RangeEnd` at the next `Update`, as a seek would, with a null `Clip` because the old model may be gone. A hitbox shut on `RangeEnd` no longer stays open when an entity's model changes.
- **A disabled `AnimatorComponent` reports no events** ([#81](https://github.com/KingoBoiii/DingoEngine/issues/81), K23). Before, its animator reported the events of the frame it stopped on, every frame. The scene empties the list with the new `Animator::ClearEventsThisFrame()`.
- **A layer at weight 0 freezes the right pose** ([#82](https://github.com/KingoBoiii/DingoEngine/issues/82), K24). A fifth state on a hidden layer froze the last pose the layer had shown, or the rest pose, rather than the mix its four states had reached meanwhile. The layer's pose is now worked out when a freeze needs it.
- **The ImGui index upload stays inside its buffers** ([#86](https://github.com/KingoBoiii/DingoEngine/issues/86)). NVRHI rounds a small Vulkan buffer write up to a multiple of 4 bytes, so an odd count of 16-bit indices read 2 bytes past the host copy, which stopped every AddressSanitizer run. The copy and the GPU buffer are now padded to an even count.
- **Every small Vulkan buffer upload stays inside its buffers** (found reviewing #86). `GraphicsBuffer` uploads had the same hazard: an odd count of 16-bit indices, or any upload whose size isn't a multiple of 4, read past the caller's data and could write past the end of the buffer. On Vulkan, non-volatile buffers are now allocated rounded up to 4 bytes, and an upload of such a size is padded before NVRHI sees it. Volatile buffers, D3D11 and D3D12 are unchanged.
- **`AudioAttenuationModel::None` means no distance falloff** ([#96](https://github.com/KingoBoiii/DingoEngine/issues/96)). It mapped to miniaudio's own "none", which bypasses the spatializer, so the sound played centred, at full level whatever its volume. It is now an inverse model with no rolloff: full gain at any distance, still panned, with the volume applied.
- **`Window::GetAspectRatio` keeps the last aspect while minimized** ([#97](https://github.com/KingoBoiii/DingoEngine/issues/97)). It divided by the minimized window's zero height, which a game updating in the background read as NaN.

The requests:
- **Text turns** ([#93](https://github.com/KingoBoiii/DingoEngine/issues/93)). `TextParameters::Rotation` turns a string about its position (the middle of its first line's baseline when `Centered`), and a scene passes a text entity's world rotation through instead of dropping it.
- **Text keeps its z** ([#94](https://github.com/KingoBoiii/DingoEngine/issues/94)). A scene draws sprites, circles and text as one stream sorted by world z, so a panel can cover a label; before, every circle drew above every sprite and every text above both. At equal z it keeps the old order (sprites, then circles, then text), so a scene only looks different where text or a circle sat below what it overlaps. `Renderer2D::Flush()` draws what has been submitted so far, which the scene calls between kinds. Candlewick, Marionette and AngryBirds put their HUD text above their fades and flying bodies to keep their look; DungeonCrawler's gems now lie under the characters walking over them, as their z always said.
- **A scene renders into a texture** ([#89](https://github.com/KingoBoiii/DingoEngine/issues/89)). `SceneRenderer::Render(scene, target)` renders into a `Framebuffer` (RGBA8 with depth, like the window's), and the projections take the aspect of whatever is drawn into, the current render target included (they used to take the window's). `Framebuffer::GetWidth`/`GetHeight` read the size from creation on; they read 0 until the first resize. Renderer2D's camera buffer, like Renderer3D's scene buffer, now holds 32 scenes a frame on Vulkan (`k_MaxScenesPerFrame`), not 8.
- **Textures read back and save as PNGs** ([#90](https://github.com/KingoBoiii/DingoEngine/issues/90)). `FileSystem::WriteImage` writes PNG, BMP, TGA or JPEG through `stb_image_write` (now in `vendor/stb`). `Texture::ReadPixels` copies an RGBA8 texture back to the CPU: inside a frame the copy follows what the frame has recorded and the pixels arrive at the start of the next frame; from `OnAttach` or a minimized frame at once; between frames, at the next frame's start. `Texture::SaveToFile` writes one the right way up.
- **Gamepad rumble** ([#88](https://github.com/KingoBoiii/DingoEngine/issues/88)). `Input::SetGamepadRumble(low, high, seconds)`, `StopGamepadRumble` and `IsGamepadRumbleSupported`, on XInput pads under Windows; elsewhere it does nothing and reports so. GLFW's gamepads are input only, so the engine pairs each GLFW joystick with its XInput slot the way GLFW assigns them, and stops a rumble on time even while the app is paused.

**Test**: new checks in the Animation Test (a hidden layer's freeze, a disabled animator's events, and the range ends of a rebind, on a bare animator and in a scene; all four fail on v0.8.2), the Background Test (the aspect ratio while minimized), and the Asset Manager Test (no texture loaded after an `Unload` shares a generation with the freed one). The new **Render Target Test** (`--test=target`) crossfades two 3D scenes rendered into textures, and reads back a 2D probe scene rendered into a square framebuffer to check that its projection takes the framebuffer's aspect, that row 0 is its top, that text under a sprite with a higher z is hidden, that rotated text turns, and that the PNGs it saves hold the pixels read back; with v0.8.2's scene rendering the aspect, z-order and rotation checks fail. Rumble was not tried on a pad: the machine has none. The Vulkan changes (#78, #79, #86, the buffer padding) were reviewed but not run: the machine they were made on has no Vulkan driver. #78's fallback was checked on DX11 by forcing it, and the buffer padding by forcing every upload through it, which left the Mesh 3D and Model 3D frames pixel-identical.

## v0.9 — Shadows, Post-processing & VFX
**Status**: released as v0.9.0 on 2026-10-08 (PR #106). The engine work (P1–P12 of
`.claude/plans/2026-10-06-v0.9-shadows-post-vfx-plan.md`, whose "As built" notes override the design
below, reviewed in `.claude/reviews/2026-10-08-v0.9.0-review.md`), the *Candlewick* upgrade (P14,
`.claude/plans/2026-10-08-candlewick-v0.9-plan.md`) and the *Marionette* upgrade (P15,
`.claude/plans/2026-10-08-marionette-v0.9-plan.md`) shipped together, verified on Vulkan, DX11 and
DX12 (the plan's §9 "GPU verification"). What it adds:

- **Profiling**: Tracy (not Optick) behind premake's `--profile`, `DE_PROFILE_*` zones, always-on GPU
  pass timers and an F8 Profiler tab ([docs/profiling.md](docs/profiling.md)).
- **Shadows**: cascaded shadow maps for the first casting directional light, and shadows for up to 8
  (at most 16) casting point and spot lights in one atlas, skinned casters, `ShadowCasting` per mesh
  (`ShadowsOnly` for cut-away walls), and **shadow probes**: `Scene::GetLightVisibility` answers "is
  this point in that light's shadow" with the renderer's own lookup, a frame or two late
  ([docs/shadows.md](docs/shadows.md)).
- **The post chain**: an HDR scene target per camera (`PostProcessComponent`), tone mapping (`Soft`,
  the default, leaves everything below 0.8 untouched; ACES; Khronos Neutral), dual-filter bloom and
  SAO ambient occlusion ([docs/post-processing.md](docs/post-processing.md)). #76 is closed.
- **GPU particles**: compute-simulated emitters (`ParticleEffectParams`, `ParticleEmitterComponent`),
  soft particles through the post chain, particles fired from animation events
  (`ParticleEventComponent`), and a live effect editor in F4 that prints the effect's code
  ([docs/particles.md](docs/particles.md)); under them, compute shaders and storage buffers
  ([docs/compute.md](docs/compute.md)).
- **The light budget's fade band** (#75): opt-in, `Renderer3DCapabilities::LightBudgetFade`.

**Migration**: no API breaks, and every feature is off until a game asks for it, so an existing game
renders as before. Behaviour notes: a `Framebuffer` resize keeps its attachment `Texture`s (it used to
replace them, which left anything holding `GetAttachment(0)` dangling); `PostProcessStack::Begin`
needs the projection for AO (`Begin(settings, projection)`, which the `SceneRenderer` passes);
`ParticleEffect` is not copyable.

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
  target rather than as one-off effects. Tone mapping isn't cosmetic here — v0.7 lets N lights sum
  past 1.0, so the choice is mapping that range or clipping it, and today the engine clips
  ([#76](https://github.com/KingoBoiii/DingoEngine/issues/76), formerly K17).
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
  rather than by what exists. v0.7 already culls *lights* by frustum; meshes are still submitted
  whether or not they are visible, and games cull them by hand today, with `MeshRendererComponent::Visible`.
- **Material sharing**: a shared-material path so the first custom material in a scene doesn't
  fragment the single-batch fast path. It is also what lets per-mesh roughness and emissive stop
  costing a material each: v0.7's lit materials are per-material, not per-mesh.
- **DingoUI, a game UI framework** ([#98](https://github.com/KingoBoiii/DingoEngine/issues/98)):
  games have no UI layer of their own today (`Dingo::UI` is an ImGui facade for debug panels), so
  every example builds its HUD and menus from sprites and text by hand. `DingoUI` is an
  immediate-mode API (`DingoUI::Button("Play")`) for HUDs and menus: flexbox-style layout anchored to
  the screen, gamepad and keyboard focus navigation, world-space health bars and labels, 9-slice
  skins and themes, tweens and screen transitions, and clicks the UI takes kept away from the game.
  It draws a whole menu in about one draw call, after the post chain. ImGui stays the debug UI.
  Plan: `.claude/plans/2026-10-08-v1.0-game-ui-plan.md`.

Doing this last is deliberate: optimising a renderer is measurement work, and by v1.0 there is
finally a full frame to measure — lights, skinned characters, shadows and a post chain all present —
instead of a moving target.

**Full game release**: *Hearthvale* (working title), a new cozy builder/farm sim in `examples/`, which replaced *Dungeon Crawler* as the 1.0 release game on 2026-10-08. You build up a valley of fences, crops and houses, then tend it day by day. That makes it the capstone stress test for the engine: thousands of placed objects for culling, instancing and static batching; the heaviest saves and game UI of any example; the day/night sun and lanterns of v0.7 and v0.9; and animated villagers and animals from v0.8. **Released on Itch.io, with Steam as a stretch goal.** The plan is `.claude/plans/2026-10-08-v1.0-plan.md` (§6).

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
