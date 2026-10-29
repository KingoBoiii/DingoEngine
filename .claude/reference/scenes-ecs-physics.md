# Scenes, ECS & physics

Read on demand from CLAUDE.md. Scene lifecycle, scripts, components, lights, hierarchy, runtime handles and physics.

Index of `##` headings (Grep the heading, then Read just that range):

- Scene and physics worlds, Lifecycle, Scene rendering, Scripts.
- Fixed tick, Components, Lights, Fog.
- Hierarchy, Sockets, Active, Ragdolls.
- Character controllers, Runtime handles, Physics stepping, Physics shapes and queries.

## Scene and physics worlds

- `Scene` owns entities (EnTT, PIMPL'd) and lazily-created per-dimension physics worlds:
- `Physics2D` (Box2D) / `Physics3D` (Jolt), reached via `Scene::GetPhysics2D()/GetPhysics3D()`.
- A scene only pays for the dimension it uses.

## Lifecycle

- `OnStart` → `OnUpdate` → `OnStop`, `IsRunning()`; `Clear()` also stops.
- `SceneManager` is the default driver: every `SetActiveScene` starts the scene (`OnStart` is idempotent; since v1.0 P9, A301, the first one too), and a switch runs `OnStop`(out)+`OnStart`(in); `CreateScene` never activates.
- `OnPhysicsStart/Stop` are deprecated (A310).
- Scripts can request a switch via `RequestSceneTransition(name)` (drained by `SceneManager::OnUpdate` after the active scene updates).

## Scene rendering

- `SceneManager::OnRender()` → engine-owned `SceneRenderer`, which reads the primary `CameraComponent` (ortho view from 2D `TransformComponent`, perspective from `Transform3DComponent`), submits the scene's lights through `Scene::SubmitLights`, and draws the 3D pass then 2D as overlay.
- `Scene::RenderEntities/RenderEntities3D` stay public for custom passes.
- `Render(scene, target)` (v0.8.3, #89) renders into a `Framebuffer` (`SetRenderTarget` for the call, the previous target restored); the projections take the aspect of whatever is drawn into: the target, else the current render target, else the swap chain.
- `RenderEntities` (v0.8.3, #94) sorts sprites, circles and text into one stream by world z, then kind (sprite, circle, text: the old pass order), then hierarchy depth, and calls `Renderer2D::Flush()` at every change of kind, since each kind batches separately; text takes its world rotation (#93, `TextParameters::Rotation`).

## Scripts

- `ScriptableEntity` — `OnCreate` → `OnStart` (before physics bake) → `OnFixedUpdate(step)` (v1.0 P9: zero or more times an update from a scene accumulator, `Scene::SetFixedTimeStep` 1/60 s by default, at most 8 steps an update, `GetFixedTimeAlpha`; physics still steps once per update unless `SceneParams::FixedTick`, docs/versioning.md) → `OnUpdate` → `OnDestroy`.
- One script per entity: a second `AddScript` replaces the first after its `OnDestroy`, `Entity::RemoveScript` detaches it, and a detached script is freed at the end of the update (A302).
- A controller script can build the whole world in `OnStart`; keeps game layers tiny (DungeonCrawler3D is the showcase).
- Calling `DestroyEntity` from inside a script's `OnDestroy` is safe:
- `DetachScript` unregisters the script *before* running `OnDestroy`, and `Clear`/`StartScripts`/`OnUpdate`/`ForEachScript` all walk handle snapshots rather than the live script map.

## Fixed tick

- **Fixed tick** (#299, v1.1):
- `Scene(name, SceneParams{ FixedTick, InterpolatePhysics })`.
- Per tick in `Scene::OnUpdate`:
- `Scripts.FixedUpdate` → `FlushPendingDestroys` → `Interpolation.BeforeTick` → `Physics.Step(step)` → `Interpolation.AfterTick` → `++Tick`; the per-frame physics step is skipped.
- `PhysicsInterpolation` (`Scene/Systems/`) keeps `InterpolatedPose3D/2D` (runtime components) with the local pose physics wrote on the last two ticks; the RAII `Scene::InterpolatedView` (nests by depth) writes the blended pose into the transforms for drawing and restores the latest tick on exit.
- Every scene call that draws or reads a transform for the camera (render, lights, `GetCameraViewProjection`, ray/screen helpers, `AudioSync`) opens one, and a new such call must too.
- A pose differing from the stored one (game write, teleport via body/controller) drops the record so the next tick snaps.
- `StopPhysics` clears the records.

## Components

- 3D mirrors 2D — `Transform3DComponent` (pos, quat, scale), `MeshRendererComponent` (`Mesh*` + color + optional `Material*`; `Static` bakes it, v1.0 P5), `RigidBody3DComponent`, `Box/Sphere/CapsuleCollider3DComponent` (collider size = fraction of transform scale), `MeshCollider3DComponent` (v0.6.2: the mesh at the FULL transform scale, like the renderer; null `Mesh` = the entity's `MeshRendererComponent::Mesh`).
- New built-in components must be registered via `DE_INSTANTIATE_COMPONENT` in `src/.../Entity.cpp` or clients can't use them.

## Lights

- **Lights** (v0.7):
- `PointLightComponent`, `SpotLightComponent` (position from `Transform3DComponent`, world position under a parent since v0.7.1; spot aim = world `Rotation * Direction`; skipped without its own transform; `Enabled`), `AmbientLightComponent`, and `DirectionalLightComponent` + `Color`/`Intensity` (its legacy `Ambient` (deprecated and 0 by default since v1.0 P9, A305: an `AmbientLightComponent` gives the old look) adds white ambient AND scales the light by `1 - Ambient`, keeping the old `a + (1-a)·N·L` look; up to 4 count, each adding its `Ambient`; components go in entity order, roughly creation order).
- `SceneRenderer` calls `Scene::SubmitLights` (→ `Internal::LightSystem`); a custom 3D pass calls it itself.
- **Default-light rule**: only a registry with NO light component gets a default `DirectionalLightComponent` (a disabled one still counts; a point/spot without a `Transform3DComponent` doesn't, and warns once), and ambient is always set, so an all-lights-off scene goes dark.
- Registered in `DE_INSTANTIATE_COMPONENT`; no runtime handles.
- Gameplay queries:
- `GetLightAttenuation(light, point)` (`Light.h`) is the lit shader's per-light weight (falloff² · cone², no N·L, occlusion or budget) and `Point/SpotLightComponent::ToLight(transform)` builds the light `SubmitLights` submits from a world-space transform (`SubmitLights` passes the world pose; a game passes `GetWorldPosition/GetWorldRotation` under a parent), both on the renderer's own `Graphics/LightMath.h`.

## Fog

- **Fog** (#92):
- `Renderer3D::SetFog(Fog)` / `ClearFog` (`FogMode` and `Fog` in `Light.h`), scene-scoped like the lights and never a light (a fogged scene keeps the default light); `CameraData` appends `FogColor` (a = `MaxOpacity`) and `FogParams` (start, end, density, mode) at 1808 (1840 bytes); the lit shader mixes after the cascade tint, only for a perspective camera (`CameraPosition.w`), toward the target `LitMaterialData::Surface.w` picks from the material's blend mode (0 `FogColor`, 1 black for `Additive`, 2 white for `Multiply`, #152); `Statistics::Fogged`.
- `FogComponent { Fog Settings, UseClearColor, Enabled }` (instantiated, duplicated; `Settings` passes on whole, so a field added to `Fog` reaches the ECS, #163; `UseClearColor` takes `Scene::GetClearColor`):
- `LightSystem::SubmitLights` gets the clear colour and the camera (`Scene::SubmitLights(renderer, camera, shadowProbes)`, which `SceneRenderer` calls with its perspective camera; #168) and passes the camera entity's own enabled one, else the first enabled one in entity order on an entity without a `CameraComponent`, else, only when no camera is passed, the first on one with (a named camera never takes another camera's fog).
- Lighting Test `--fog`.
- See docs/lighting.md.

## Hierarchy

- **Hierarchy** (v0.7.1):
- `Entity::SetParent/RemoveParent/GetParent/GetChildren/FindChild`, `GetWorldTransform/Position/Rotation/Scale`, `SetWorldPosition/Rotation` (3D only: a node without a `Transform3DComponent` reports its parent's 3D world), and the 2D `GetWorldPosition2D/SetWorldPosition2D/GetWorldRotation2D/SetWorldRotation2D`.
- The links live in the engine-internal `Internal::HierarchyComponent` (`Scene/Systems/HierarchySystem.h`), emplaced on the first `SetParent` and never in `DE_INSTANTIATE_COMPONENT`.
- `Transform3DComponent` and `TransformComponent` are local to the parent (2D: position turns with the parent's rotation, z and rotation add, `Size` is never inherited; a parent counts as identity in a dimension it has no transform for); world values are computed on demand and never cached past one pass (the one exception, `SceneData::MeshMemo` in `RenderEntities3D`, reuses a root mesh entity's world matrix, and any mesh entity's world bounds, only while what they were computed from comes back bit-identical, #231), and a root returns its component's own values untouched, which is what keeps flat scenes bit-identical.
- Every world reader (`RenderEntities3D`, `RenderEntities`, `LightSystem`, `CameraUtils`, `AudioSync`, `PhysicsSync`) goes through `HierarchySystem`; a new reader that treats either transform as world space must too.
- A per-entity loop uses a `HierarchySystem::WorldMemo`:
- `Begin` per pass, then each linked entity's world is composed once, parents first, bit-identical to the on-demand functions (without a single link it is the plain component read).
- Sprites tie-break equal world z on depth (parent first).
- Destroy and duplicate take the subtree.
- Physics: bodies bake from the world pose and world scale; dynamic and controller children write back `local = inverse(parentWorld) × simulated`, shallowest first; kinematic children are `MoveKinematic`'d before each step to their end-of-step world pose, extrapolating the nearest ancestor that has a moving body or controller by its velocity (exact for a kinematic ancestor); static children don't follow.
- 2D does the same through `Physics2D::MoveKinematic`/`GetAngularVelocity` (Box2D's approximate trig leaves ~0.1° of angle error).
- Since v0.8 P6 a kinematic child's body ignores every ancestor's body (`Physics3D::IgnoreCollision`, a Jolt `GroupFilter` keyed by body-id pairs; a change wakes both) and every ancestor's character controller ignores it (`CharacterController3D::IgnoreBody`, a `BodyFilter` passed to `ExtendedUpdate`).
- `PhysicsSync::SyncAncestorFilters` works the pairs out at the start of every 3D step and diffs them against the last, so reparenting and runtime bodies take effect on the next step.
- Nothing else is filtered: dynamic and static children, siblings and 2D bodies still collide.

## Sockets

- **Sockets** (v0.8 P6):
- `SetParent(parent, "joint", keep)` (a `const char*` overload keeps a string literal off the bool one) stores an `Internal::SocketComponent` beside the link; world = parentWorld × joint frame × local, the joint frame being `AnimationSystem::JointFrame` (the animator's pose, or the rest pose; position and rotation only, so an FBX root's 0.01 scale isn't inherited).
- `HierarchySystem::LinkTransform` / `WorldMemo::LinkOf` are where a socket enters, so every reader and the physics prediction follow it; unsocketed links compose exactly as before.

## Active

- **Active** (v1.0 P9):
- `Entity::SetActive/IsActive/IsActiveInHierarchy`.
- An engine-internal `InactiveSelfComponent` holds the entity's own flag and a derived `InactiveComponent` marks it or any ancestor off, re-derived over the subtree on `SetActive`, `SetParent`, `RemoveParent` and duplicate.
- Every system skips an inactive entity (2D, 3D, skinned and static drawing, lights and fog, camera and listener picks, animation as a disabled animator, particles, scripts); its bodies and controllers are destroyed and rebuilt at the current transform on activation, a sound stops and `PlayOnStart` starts again, and a script on an inactive entity starts when the entity is first active.
- Hierarchy Test `HierarchyTestActive.cpp`.

## Ragdolls

- **Ragdolls** (#298, physics-coop P1):
- `RagdollComponent { Blend, Strength, DriveFrequency, DriveToAnimation, TotalMass, CollideWithinRagdoll, Recipe }` beside a `SkinnedMeshRendererComponent`.
- `Internal::RagdollSystem` (`Scene/Systems/RagdollSystem.h`, owned by `PhysicsSync::m_Ragdolls`) keeps `RagdollRuntime` (`Parts` parents first with body, joint to the parent part, the body's offset in its joint's rigid world frame, mass and subtree inertia; `PartOfJoint`; its own rest-pose `Animator` when there is no `AnimatorComponent`, which `AnimationSystem::BoundAnimator` falls back to for the palette; `SkeletonId/Revision`; one collision group; `FollowsRoot`).
- `Sync` runs in `PhysicsSync::Start` (dt 0, build only; a ragdoll alone starts the 3D world) and every `Step` after `SyncConstraints`: drops a ragdoll whose skeleton id/revision changed or lost a body, builds pending ones (excluding `InactiveComponent`) from the current animated pose, then sets motors (`k = I·(2π·DriveFrequency)²·Strength`, `I` the subtree's inertia about the joint, capped at `BodyInertia·(2π·0.25/dt)²` so a light body under a heavy subtree never rings above a quarter of the step rate, which is what swung the Fox's tail at 4 and 6 Hz before #307; damping `2√(k·I)`) and the root drive, which adds the root's correction to every part as one rigid motion, measured against the mass-weighted mean velocity; the target's own velocity (`RagdollRuntime::TargetPosition/Rotation`, skipped past a 0.5 m or 1 rad jump) is fed forward so a moving pose isn't trailed by v/ω.
- Under FixedTick the animator holds the interpolated bones, so joint reads and sockets on ragdoll bones lag up to a tick (noted in PhysicsInterpolation.h).
- `RagdollSystem::AfterStep` (end of `PhysicsSync::Step`) records each part's previous and current pose and moves a following entity by `HierarchySystem::SetWorldPosition`; `Scene::OnUpdate` calls `PhysicsSync::PoseRagdolls(alpha)` inside the `InterpolatedView`, which blends the recorded poses (`PhysicsInterpolation` tracks a `FollowsRoot` entity) into `Animator::BlendGlobalTransforms`.
- F9:
- `SceneDebug::RagdollView`/`WriteRagdoll` (Blend, Strength, DriveFrequency, Drive editable).
- A failed build destroys what it made and is rejected once per (skeleton id, revision).
- Dropped by `on_destroy<RagdollComponent>`, `DestroyBodiesForEntity` (destroy, `SetActive(false)`) and `Stop` (`Clear`).
- `EntityOfBody3D` accepts ragdoll bodies (`UserData` = entity); `Scene::GetRagdollBody(entity, joint)`.
- While `FollowsRoot`, `AnimationSystem::Update` skips root motion.
- A ray down from the root at the moment it goes limp stores the entity's height above ground (`GroundHeight`); `Drive` re-seats a `Carried` entity at that height above the ground under the hips when the ragdoll is driven again, eased at `k_SeatSpeed` 2 m/s (`Seating/SeatHeight`); `RagdollRuntime::DrivenFor` runs the first second after limp at `k_SoftStrength` 0.25 (a full-strength restart leaves the tail ringing).
- `RigidBodyParams3D::Mass` (dynamic only, `CalculateInertia` override).
- Recipes (P2):
- `<stem>.ragdoll` beside a model with a skeleton (`Internal::ParseRagdollRecipes`, `Graphics/RagdollRecipeFile.h`, unit-tested; `[name]` sections, the lines before the first one the default; `exclude`, per joint `shape/radius/mass/hinge/swing/twist`), read through `Model::FindRagdollRecipe`; `Model::GetRagdollRecipeRevision()` and `RagdollComponent::Recipe` join the rebuild key (`RagdollRuntime::Recipe/RecipeRevision`, `Rejection`).
- Radii are measured (`MeasureRadii`: 85th-percentile distance of the rest vertices each part moves most, from its bone, clamped to 0.08–0.6 × length, ≥ 8 samples), but mass splits by the automatic (P1) capsule volume, since measured fur made the Fox's tail tip outweigh the hips and the driven hold sagged.
- The AssetManager watches every `CompanionFile` in a policy's `Companions` (model: `.events` → `ReloadEvents`, `.ragdoll` → `ReloadRagdollRecipes`).
- Ragdoll Test `--test=ragdoll` (`RunRecipeChecks` edits a copy in the temp dir; P3 push, stairs and an 8-ragdoll crowd budget).
- A mesh collider added after `OnStart` needs `Scene::CreateRigidBody(entity)`.
- Plan:
- `.claude/plans/2026-10-27-physics-coop-plan.md`.

## Character controllers

- `CharacterController3DComponent` — `Scene::OnUpdate` calls `controller->Update(dt)` itself, so a script only *sets* velocity/direction; never step it by hand.
- Controllers are rebuilt on `OnStart`, so scripts must not cache a `CharacterController3D*` across an `OnStop`/`OnStart` cycle (use-after-free).

## Runtime handles

- **Runtime handles** (v0.6.3): public components hold settings only.
- An entity's live physics body (+ 2D shape ids), character-controller slot and playing sound are engine-internal EnTT components in `src/DingoEngine/Scene/Systems/RuntimeComponents.h`, present exactly while the thing they name is alive (emplaced on create, removed on destroy, `registry.clear<T>()` when the world stops).
- So copying, assigning or duplicating a component can never alias a body — never add a handle field back onto a public component, and never add the runtime structs to `DE_INSTANTIATE_COMPONENT`.
- Clients read handles via `Scene::GetRuntimeBody2D/3D`, `GetRuntimeSound`, `GetCharacterController`; 3D's "none" sentinel is `k_InvalidBody3D` = 0xFFFFFFFF, **not 0**.

## Physics stepping

- `Scene::OnUpdate` runs scripts, steps live worlds, writes simulated transforms back (2D → `TransformComponent`, 3D → `Transform3DComponent`).
- The 3D step takes `ceil(dt * 60 - 0.1)` Jolt collision steps, clamped to [1, 4] (v0.6.2; it was always 1, which let bodies tunnel through mesh colliders at 30 fps).
- `Scene::OnUpdate` caps its delta at `PhysicsSync::k_MaxStepTime` = 4/60 s and drops the rest.
- The cap covers scripts and both worlds alike, so `MoveKinematic` targets and controller velocities match what physics simulates, and a scene runs slow through a stall instead of tunnelling.
- Before it (K15), a cold shader cache's 1.3 s first frame dropped DungeonCrawler3D's player through the floor.
- Layers outside a scene still get the real delta.

## Physics shapes and queries

- Shape is baked into the body at creation (no separate 3D shape handles).
- Mesh/ConvexHull shapes are cached per world by `Mesh::GetId()` — never by `Mesh*`, whose address a freed mesh can hand to a new one — and each body wraps the shared shape in its own `ScaledShape`.
- Triangle meshes are Static/Kinematic only (Dynamic falls back to the hull) and one-sided; fast bodies against them need `ContinuousCollision` (Jolt `LinearCast`).
- Camera picking:
- `Scene::ScreenPointToRay(screenPos, viewportSize)` + `Ray::IntersectGroundPlane` (perspective cameras only).
- Sensors (#102):
- `IsSensor` bodies never collide, queries skip them by default, `Physics3D::GetSensorOverlaps` / `CharacterController3D::IsOverlapping` / `Scene::GetSensorOverlaps` ask what is inside; `QueryFilter3D` (ignored bodies, `QueryLayers` mask, sensors) on every query; Jolt user data = `QueryLayers << 32 | UserData`, and a Scene keeps the entity in `UserData` (`Scene::GetEntityFromBody3D`).
- Controllers block each other unless `CollideWithCharacters` is off (#101, one `CharacterVsCharacterCollisionSimple` per world).
