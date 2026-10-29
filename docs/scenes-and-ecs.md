# Scenes & ECS

Added in **v0.3**, the scene system lets you model game objects as **entities** with
small built-in **components** for data and rendering, and attach game logic as
**behaviours** (`ScriptableEntity`). Internally it's backed by an entity-component
system, but that backend is a private engine detail — **client code never includes
or links the ECS library**, and no ECS types appear in the public API.

The pieces:

- **`Scene`** — owns a set of entities + their behaviours, and an `OnStart`/`OnUpdate`/`OnStop` lifecycle.
- **`Entity`** — a lightweight handle for adding/reading components and scripts.
- **Components** — built-in data structs (Transform, Sprite, Circle, Text, Camera, light).
- **`ScriptableEntity`** — base class for your per-entity game logic.
- **`SceneManager`** — owns named scenes, drives their lifecycle, and updates/renders the active one.
- **`SceneRenderer`** — renders a scene from its ECS camera + light components (engine-owned; `SceneManager::OnRender()` drives it).

Everything is available through `<DingoEngine.h>`.

## Creating a scene and entities

```cpp
Scene scene("Game");

Entity player = scene.CreateEntity("Player");          // auto-gets a UUID + name + Transform
player.GetComponent<TransformComponent>().Position = { 0.0f, -4.0f, 0.0f };
player.GetComponent<TransformComponent>().Size     = { 1.5f, 0.6f };
player.AddComponent<SpriteRendererComponent>(SpriteRendererComponent{ glm::vec4{ 0.3f, 0.9f, 0.4f, 1.0f } });
```

Every entity created via `CreateEntity` automatically gets a stable `UUID`, a name
(`TagComponent`), and a `TransformComponent`.
`CreateEntityWithUUID(uuid, name)` does the same with a UUID you choose. If an entity in the
scene already has that UUID, it creates nothing, logs a warning and returns an invalid `Entity`.
An entity destroyed from a script keeps its UUID until the end of that update pass.

### Entity API

| Call | Effect |
|---|---|
| `AddComponent<T>(value)` | Attach a built-in component `T`. Returns `T&`. An entity that already has a `T` gets it replaced, with a warning. |
| `GetComponent<T>()` | Reference to the entity's `T`. |
| `HasComponent<T>()` | `true` if the entity has a `T`. |
| `RemoveComponent<T>()` | Detach the `T`. |
| `AddScript<T>(args...)` | Attach a behaviour (see below). An entity holds one script: a second `AddScript` destroys the first, with a warning. |
| `RemoveScript()` (v1.0) | Destroy the entity's script (its `OnDestroy` runs now, the object is freed after the scene's update pass, or its next `OnStop` or `Clear`). |
| `GetScript<T>()` / `HasScript<T>()` | The attached `T` behaviour, or null. |
| `GetUUID()` / `GetName()` / `SetName(name)` (v1.0) | The entity's id / name; a null entity reads `UUID()` and `""`, and an empty name becomes `"Entity"`. |
| `IsValid()` / `operator bool` | `false` for a null or destroyed entity. |
| `Destroy()` | Destroy this entity, its behaviour and its children. |
| `SetActive(bool)` / `IsActive()` / `IsActiveInHierarchy()` (v1.0) | Switch the entity and its subtree off and on (see [Active and inactive entities](#active-and-inactive-entities-v10)). |
| `SetParent(parent, keepWorldTransform = true)` / `RemoveParent(keepWorldTransform = true)` (v0.7.1) | Attach to / detach from a parent (see [Parenting](#parenting-v071)). |
| `SetParent(parent, "joint", keepWorldTransform = true)` / `GetParentJoint()` (v0.8) | Attach to a joint of the parent's skinned model, so the entity follows it as the model animates (see [Joints as parents](#joints-as-parents-v08)). |
| `GetParent()` / `GetChildCount()` / `GetChildren()` / `ForEachChild(fn)` / `FindChild(name, recursive = true)` (v0.7.1) | Walk the hierarchy. |
| `GetWorldTransform()` / `GetWorldPosition()` / `GetWorldRotation()` / `GetWorldScale()` (v0.7.1) | The 3D transform in world space, through every parent. |
| `SetWorldPosition(p)` / `SetWorldRotation(q)` (v0.7.1) | Write the 3D local value that gives this world value. |
| `GetWorldPosition2D()` / `SetWorldPosition2D(p)` / `GetWorldRotation2D()` / `SetWorldRotation2D(degrees)` (v0.7.1) | The same for the 2D `TransformComponent`: position with z, rotation in degrees. |

> The component methods support the **built-in component types** (below). To carry
> game-specific data, put it in a `ScriptableEntity` subclass rather than defining new
> component types — that's what keeps the ECS backend hidden.

## Built-in components

| Component | Fields |
|---|---|
| `TransformComponent` | `glm::vec3 Position` (center), `float Rotation` (degrees, +Z), `glm::vec2 Size`; `GetTransform()` → `mat4` |
| `SpriteRendererComponent` | `glm::vec4 Color`, `Texture* Texture` (null ⇒ solid colour) |
| `CircleRendererComponent` | `glm::vec4 Color`, `float Thickness`, `float Fade` |
| `TextComponent` | `std::string Text`, `Font* Font`, `glm::vec4 Color`, `float Size`, `bool Centered`; turns with the transform's `Rotation` (v0.8.3) |
| `TagComponent` / `IDComponent` | Name / `UUID` (added automatically) |
| `CameraComponent` | `ProjectionType Type` (`Orthographic`/`Perspective`), ortho `OrthographicSize`/`OrthoNear`/`OrthoFar`, perspective `FOV`/`PerspNear`/`PerspFar`, `bool Primary`; the camera the `SceneRenderer` views the scene through |
| `PostProcessComponent` (v0.9) | `PostProcessSettings Settings` (`Enabled`, false by default; `Tone`: operator, exposure, knee, white point; `Bloom`; `AmbientOcclusion`); on the primary perspective camera, the `SceneRenderer` runs the 3D pass through the post chain. See [Post-processing](post-processing.md) |
| `DirectionalLightComponent` | `glm::vec3 Direction` (the way the light travels), `glm::vec3 Color` and `float Intensity` (v0.7), `float Ambient` (deprecated, 0 by default, removed in 2.0: use an `AmbientLightComponent`), `bool CastShadows`, `float ShadowStrength` and `uint32_t ShadowCasterGroups` (v0.9, see [Shadows](shadows.md)) — a sun |
| `ParticleEmitterComponent` (v0.9) | `ParticleEffect* Effect`, `bool Playing`, `float RateScale`, `bool WorldSpace` — GPU particles from the entity's world transform, stepped by `Scene::OnUpdate` and drawn by the `SceneRenderer`; `Scene::EmitParticles` / `EmitParticlesAt` fire bursts. See [Particles](particles.md) |
| `ParticleEventComponent` (v0.9) | `Bindings` (`Bind(event, emitterUUID, count)`, `BindRange(event, emitterUUID)`) — the entity's animation events burst emitter entities or play them while a range is open. See [Particles](particles.md#from-animation-events) |
| `AmbientLightComponent` (v0.7) | `glm::vec3 Color`, `float Intensity` — light that reaches every face equally; all of them add up |
| `PointLightComponent` (v0.7) | `glm::vec3 Color`, `float Intensity`, `float Range`, `bool Enabled`, `bool CastShadows`, `float ShadowStrength` and `uint32_t ShadowCasterGroups` (v0.9, see [Shadows](shadows.md)) — light in every direction from the entity's `Transform3DComponent` position; `ToLight(transform)` → the `PointLight` it draws as |
| `SpotLightComponent` (v0.7) | `glm::vec3 Color`, `float Intensity`, `float Range`, `float InnerConeAngle` / `OuterConeAngle` (degrees), `glm::vec3 Direction` (local space, default `{ 0, 0, -1 }`), `bool Enabled`, `bool CastShadows`, `float ShadowStrength` and `uint32_t ShadowCasterGroups` (v0.9) — a cone of light from the entity's `Transform3DComponent`; `ToLight(transform)` → the world-space `SpotLight` it draws as |
| `FogComponent` | `Fog Settings` (the struct `Renderer3D::SetFog` takes: `Mode`, `Color`, `Start` / `End`, `Density`, `MaxOpacity`), `bool UseClearColor` (default true: the scene's clear colour in place of `Settings.Color`), `bool Enabled` — distance fog over the 3D pass; one on the rendered camera's entity fogs that view, else the first enabled one on an entity without a camera counts (another camera's only in a pass that names no camera), and it is no light. See [Lighting](lighting.md#fog) |

The four light components and `FogComponent` are read by the `SceneRenderer` for the 3D pass — see [Lights](#lights-v07).

## Rendering a scene

Rendering goes through a **camera entity** and the **`SceneManager`**. Give the scene a clear
colour and a camera — an entity with a `CameraComponent` — then call `SceneManager::OnRender()`.
It finds the primary camera, builds its view-projection (projection from the component; **view
from the camera entity's transform**, so moving that entity pans/zooms the view) and draws every
entity with a `TransformComponent` plus a `SpriteRenderer`, `CircleRenderer`, or `Text`
component — all through the engine's `SceneRenderer`:

```cpp
void GameLayer::OnAttach()
{
    m_Game = m_Scenes.CreateScene("Game");
    m_Game->SetClearColor({ 0.02f, 0.02f, 0.06f, 1.0f });

    // An orthographic camera at the origin → a centered view, full height 20 world units.
    Entity camera = m_Game->CreateEntity("Camera");
    auto& cam = camera.AddComponent<CameraComponent>();
    cam.Type = CameraComponent::ProjectionType::Orthographic;
    cam.OrthographicSize = 20.0f;

    m_Scenes.SetActiveScene("Game");   // activates and starts it (see Lifecycle, under SceneManager)
}

void GameLayer::OnUpdate(float dt)
{
    m_Scenes.OnUpdate(dt);   // runs the active scene's behaviours + physics
    m_Scenes.OnRender();     // draws it through its camera (no renderer argument)
}
```

> In the background the app pauses, or with `ApplicationParams::UpdateInBackground` keeps calling
> `OnUpdate()`; a minimized window's `OnRender()` then draws nothing (see
> [In the background](application-and-layers.md#in-the-background)).
>
> The viewport aspect is applied automatically, so a camera tracks window resizes with no
> per-layer bookkeeping. For a **custom overlay** in the same view (e.g. a HUD over the
> entities), wrap your own `Renderer2D::BeginScene`/`EndScene` around `Scene::RenderEntities`,
> taking the matrix from `Scene::GetActiveCameraViewProjection(aspect)`.

### Drawing order

Sprites, circles and text draw in one stream sorted by **world z**: a higher z draws on top,
whatever its kind (v0.8.3; before, every circle drew above every sprite and every text above
both, so a HUD text at z 0 stayed over a fade at z 0.5: raise such text above it). At equal z,
sprites come first, then circles, then text, and within a kind a parent draws before its
children; other ties follow the registry, not creation order, so give overlapping UI elements
distinct z values. A panel at z 0.5 covers a label at z 0:

```cpp
label.GetComponent<TransformComponent>().Position.z = 0.0f;
panel.GetComponent<TransformComponent>().Position.z = 0.5f;   // drawn over the label
```

The z must stay inside the camera's `OrthoNear`/`OrthoFar` range (-1 to 1 by default).

### Rendering into a texture

`SceneRenderer::Render(scene, target)` (v0.8.3) renders a scene into a `Framebuffer` instead of the
window, and its projections take the framebuffer's aspect. Draw the result as a texture,
`target->GetAttachment(0)`: crossfade from one map to the next, show a minimap, or render an icon
and save it ([Reading a texture back](rendering-2d.md#reading-a-texture-back-v083)).

```cpp
Framebuffer* oldMap = Framebuffer::Create(FramebufferParams()
    .SetWidth(1280).SetHeight(720)
    .SetEnableDepth(true)                                   // RGBA8 and depth, like the window's
    .AddAttachment({ TextureFormat::RGBA8_UNORM }));

SceneRenderer& scenes = Application::Get().GetSceneRenderer();
scenes.Render(*previousMap, oldMap);                        // into the texture
scenes.Render(*nextMap);                                    // into the window, as usual
// The first row of a render target is its top: a negative height shows it upright.
hud.DrawQuad({ 0.0f, 0.0f }, { width, -height }, oldMap->GetAttachment(0), { 1, 1, 1, fade });
```

Without a target, `Render` draws into the current render target (`Renderer::SetRenderTarget`, the
window by default), and takes that one's aspect too. The test app's **Render Target Test**
(`--test=target`) crossfades two 3D scenes this way, and reads a probe scene back to check it.

## 3D entities (v0.4.1)

The same `Scene` also drives **3D** entities, mirroring the 2D side. A 3D entity carries a
`Transform3DComponent` (and, to be drawn, a `MeshRendererComponent`; to be simulated, a
`RigidBody3DComponent` plus one collider). The 3D physics world is the Jolt-backed
[`Physics3D`](physics-3d.md); meshes are drawn through the engine's `Renderer3D`.

| Component | Fields |
|---|---|
| `Transform3DComponent` | `glm::vec3 Position`, `glm::quat Rotation`, `glm::vec3 Scale`; `GetTransform()` → `mat4`; `SetRotationEuler(degrees)` |
| `MeshRendererComponent` | `Mesh* Mesh` (not owned), `glm::vec4 Color`, `Material* Material` (optional; null = the built-in lit material), `bool Visible`, `ShadowCasting Shadows` (v0.9: `On`, `Off` or `ShadowsOnly`, see [Shadows](shadows.md)), `uint32_t ShadowGroups` (caster groups, default 1), `float CullDistance` (v1.0, see [Culling](#culling-v10)), `bool Static` (v1.0, see [Static geometry](#static-geometry-v10)), `std::optional<SurfaceOverride> Surface` (v1.0: its own emissive, roughness and specular on a lit material, set with `SetSurface`, see [Surface overrides](lighting.md#surface-overrides-v10)) |
| `SkinnedMeshRendererComponent` (v0.8) | `Model* Model` (not owned), `glm::vec4 Color`, `Material* Material`, `bool Visible`, `ShadowCasting Shadows` and `uint32_t ShadowGroups` (v0.9), `float CullDistance` (v1.0). Draws every submesh, skinning those with a skin on the GPU; see [Skinned models](#skinned-models-v08) |
| `AnimatorComponent` (v0.8) | `std::string DefaultClip`, `bool PlayOnStart` (true), `float Speed` (1), `bool Enabled` (true). Poses the entity's skinned model; `Scene::GetAnimator(entity)` plays clips. See [Animating a model](#animating-a-model-v08) |
| `RigidBody3DComponent` | `BodyType3D Type` (`Static`/`Dynamic`/`Kinematic`), `bool ContinuousCollision` (v0.6.2), `uint32_t CollisionGroup` (0 = none) and `bool CollideWithinGroup` ([collision groups](physics-3d.md#collision-groups); read when the body is built, and a duplicate stays in the same group) |
| `Constraint3DComponent` | `ConstraintType3D Type`, `UUID Other` (null pins to the world), `AnchorA/B`, `AxisA/B`, `NormalAxisA/B`, `MinLimit`/`MaxLimit`, `SwingLimitY/Z`, `TwistMin/Max`, `bool CollideConnected`, and the live motor fields `MotorMode`, `Motor`, `TwistMotor`, `TargetRotation`, `TargetAngularVelocity` — joins this entity's body (`BodyA`) to `Other`'s, see [Constraints](physics-3d.md#constraints) |
| `RagdollComponent` | `float Blend` (1), `float Strength` (1), `float DriveFrequency` (3), `bool DriveToAnimation` (true), `float TotalMass` (70), `bool CollideWithinRagdoll` (false) — an active ragdoll for the entity's skinned model, driven toward its animated pose; `Scene::GetRagdollBody(entity, joint)` finds a bone's body. See [Ragdolls](animation.md#ragdolls) |
| `BoxCollider3DComponent` | `glm::vec3 HalfExtents` (fraction of `Scale`), `Friction`, `Restitution` |
| `SphereCollider3DComponent` | `float Radius` (fraction of `Scale.x`), `Friction`, `Restitution` |
| `CapsuleCollider3DComponent` | `float Radius` (fraction of `Scale.x`), `float HalfHeight` (fraction of `Scale.y`), `Friction`, `Restitution` |
| `MeshCollider3DComponent` (v0.6.2) | `Mesh* Mesh` (not owned; null = the entity's `MeshRendererComponent::Mesh`), `bool Convex`, `Friction`, `Restitution` — scaled by the full `Scale`, see [Mesh colliders](physics-3d.md#mesh-colliders-v062) |

> **Constraints in a scene.** The scene builds a `Constraint3DComponent` at `OnStart`, or on the next
> physics step for one added later, once both entities have bodies. `AnchorA` is multiplied by the
> entity's world scale; `AnchorB`, the axes and the normals are used as given (world values when
> `Other` is null). `Other` is a `UUID` so a copy can follow it: `DuplicateEntity` remaps an `Other`
> inside the copied subtree to the copy and leaves one outside it alone. The constraint goes away with
> either entity, with the component, or while an entity is inactive (`SetActive(false)`), and is
> rebuilt when both are active again. A missing or bodiless `Other` logs once and retries; params the
> physics world rejects log once and are not retried until the component is re-added.
>
> The motor fields are live: they are written to the joint every physics step (and right after it is built), so editing them on a built constraint takes effect at once. `Motor` drives the hinge or swing axis and `TwistMotor` the twist; see [Motors](physics-3d.md#motors).

> A 3D entity still receives the default 2D `TransformComponent` on creation; it simply goes
> unused. The collider shape is **baked into the body at creation** (so a rigid-body entity
> needs exactly one collider), and primitive collider sizes are fractions of `Transform3D.Scale`
> — a unit-scaled entity with the default collider exactly fills its mesh. A mesh collider
> instead takes the mesh at the entity's full `Scale`, exactly as `MeshRendererComponent` draws it.

```cpp
// A dynamic sphere on a static floor. Mesh* come from Renderer3D's built-in primitives.
Renderer3D& r3d = Application::Get().GetRenderer3D();

Entity ball = scene.CreateEntity("Ball");
auto& t = ball.AddComponent<Transform3DComponent>();
t.Position = { 0.0f, 5.0f, 0.0f };
ball.AddComponent<MeshRendererComponent>(MeshRendererComponent(r3d.GetSphereMesh(), { 0.3f, 0.85f, 0.95f, 1.0f }));
ball.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
ball.AddComponent<SphereCollider3DComponent>();

scene.SetGravity(glm::vec3{ 0.0f, -9.81f, 0.0f });   // vec3 overload → the 3D world
scene.OnStart();                                     // builds a 3D world only if 3D bodies exist
```

`Scene::OnUpdate(dt)` steps whichever physics world(s) are live and writes simulated transforms
back (2D → `TransformComponent`, 3D → `Transform3DComponent`). A 3D scene renders through a
**perspective camera entity** (plus the light entities described [below](#lights-v07)); the
`SceneRenderer` picks the 3D pass from the camera's projection type. The camera's *view* comes
from its `Transform3DComponent`, so a follow camera writes its position + a look-at orientation
each frame (here `scene` is a `SceneManager`-owned `Scene*` and `scenes` the manager — see below):

```cpp
Entity cam = scene->CreateEntity("Camera");
auto& camera = cam.AddComponent<CameraComponent>();
camera.Type = CameraComponent::ProjectionType::Perspective;
camera.FOV = 50.0f;                       // PerspNear/PerspFar default to 0.1 / 1000
cam.AddComponent<Transform3DComponent>();
scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
scene->CreateEntity("Sky").AddComponent<AmbientLightComponent>(AmbientLightComponent(glm::vec3(1.0f), 0.35f));

// Each frame — drive the camera transform (the view is inverse(translate(P) * rotate(R))):
auto& t = cam.GetComponent<Transform3DComponent>();
t.Position = eye;                                                  // e.g. focus + { 0, 15, 11 }
t.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(eye, focus, { 0, 1, 0 })));

scenes.OnUpdate(dt);
scenes.OnRender();   // SceneRenderer clears + draws the Transform3D+Mesh entities, lit by the scene's lights
```

### Culling (v1.0)

`Renderer3D` leaves out every mesh the camera can't see, by its bounds (`Mesh::GetBoundsMin/Max`
placed by the entity's world transform), so a scene can hold far more meshes than are in view at
once. Nothing to turn on, and a static mesh that reaches a pixel is never left out: the cull tests
the sides of the view, and the far plane only for materials that depth-test with `Less`, without a
depth bias, into a target with depth. A culled mesh still casts into any shadow view its bounds
reach. A skinned mesh is tested by its rest pose's bounds padded on every side by half their longest
side, and its animator keeps running; a pose that reaches farther than that, or hips that travel
away without root motion, can be culled while in view.

Culling trusts the mesh's bounds. A material whose vertex shader moves vertices (wind, waves,
outlines) takes `MaterialParams::SetFrustumCulled(false)`, and the camera then never culls its
meshes.

`CullDistance` (on both renderer components, or `SubmitMesh`/`SubmitSkinnedMesh`'s last argument)
adds a distance cull for perspective cameras: past that distance from the camera to its bounds a
mesh is neither drawn nor casts (its bounds still set the cascades' depth range, so other shadows
don't shift when it crosses). Use it for small props a far view needn't spend vertices on; 0, the
default, never culls by distance.

F4's Renderer tab counts what was culled (`Statistics::CulledMeshes`, `CulledSkinned`,
`CulledCasters`) and has two switches: **Cull meshes** off draws everything, which must look the
same, and **Freeze culling frustum** keeps culling against the camera as it was, so flying away
shows what that view kept. `Renderer3D::SetCullingEnabled/SetCullingFrozen` do the same from code.

A scene culls 2D too: `RenderEntities` skips sprites and circles whose bounding square lies outside
the camera's view (`Renderer2D::Statistics::CulledQuads`; `Renderer2D::SetCullingEnabled`). Text is
never culled.

### Instancing (v1.0)

A mesh drawn many times in one scene, with one material and one shadow setting, is drawn
**instanced**: every copy in view in one draw from the mesh's own vertex and index buffers, each
placed on the GPU from a 128-byte record (its transform, normal matrix, colour and the index of its
surface override). Nothing to turn on: it applies to the built-in lit material and every
`CreateLitMaterial` material once a mesh has `Renderer3DCapabilities::InstanceThreshold` copies
submitted with one material and shadow setting (8 by default), or fewer whose vertices together
outgrow one batch (`MaxVertices`). Below that the CPU batch stays cheaper. The count is taken before
culling, so the camera never moves a copy between the two transforms. Copies culled from the camera
but still casting are instanced into the shadow atlas the same way, every copy once per shadow view
in one draw.

The picture matches the CPU batches within rounding (the GPU and the CPU round the transform
differently: no channel off by more than 2/255 on at most 0.1% of the pixels), so a scene where
nothing repeats draws exactly as before. A material's instanced meshes draw after its CPU batch, so
two of its meshes meeting at exactly equal depth can resolve the other way. `Renderer3DCapabilities::Instancing = false`, F4's
**Instance repeated meshes** or `Renderer3D::SetInstancingEnabled(false)` batches everything on the
CPU again. F4 counts instanced meshes and draws (`Statistics::InstancedMeshes`, `InstancedDraws`,
`InstancedShadowDraws`); `VertexCount` leaves instanced meshes out, since nothing transforms them on
the CPU.

Translucent meshes are never instanced (the translucent pass sorts mesh by mesh), nor are skinned
ones. A custom material is instanced only when its shader is written for it; see
[Instanced custom shaders](#instanced-custom-shaders-v10).

### Static geometry (v1.0)

Walls, floors and props that never move can be **baked**: set `MeshRendererComponent::Static`, and
`RenderEntities3D` transforms the mesh into world space once, with the scene's other static meshes,
into vertex and index buffers kept on the GPU between frames. A frame then costs a cull of each
cell of them instead of transforming every vertex.

```cpp
auto& wall = entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(boxMesh, stone));
wall.Static = true;
```

The baked meshes are split by a 32-metre world grid into cells, each with its own buffers and one
draw per material, shadow setting and caster groups (more when they outgrow one draw). Each draw is
culled against the camera and every shadow view by the box around its meshes, as a single mesh is,
and drawn into the shadows as its meshes would be. The picture is the one the same meshes draw
without `Static` on the CPU, pixel for pixel; where they would have been instanced, within the
instancing tolerance, since baked meshes are never instanced. The order differs: a cell draws
before the material's other meshes, its own meshes by cell, then by material and shadow setting,
then by an internal id that reuses freed ones (so not necessarily the order the meshes were made
`Static`), and the static materials join the scene's draw order after the moving meshes'. So two
meshes meeting at exactly equal depth can resolve the other way.

Change is allowed but costs a rebake of the cell the mesh is in: changing its mesh, colour,
material, surface override or shadow setting (or hot-reloading its model) rebakes it quietly;
**moving** it rebakes it and warns once, since a mesh that moves shouldn't be `Static`. Adding,
destroying or hiding one (`Visible = false`) rebakes its cell too. A mesh with a `CullDistance`, a
translucent or skinned-only material, a custom-shader `MaterialParams::Instancing` material, more
vertices than one draw takes, or a non-affine transform is drawn as if `Static` were false. F4 counts
baked meshes, their draws and the cells baked each scene (`Statistics::StaticMeshes`, `StaticDraws`,
`StaticCellsBaked`).

Outside a scene the same is `Renderer3D::CreateStaticBatch(StaticBatchParams)` and
`SubmitStaticBatch(batch)` between `BeginScene` and `EndScene`; `StaticBatch::Add`, `Update` and
`Remove` take a `StaticMesh` (mesh, transform, colour, material, shadows, caster groups, surface).
A batch belongs to the renderer that made it and draws nothing on another. The meshes and materials
it holds must outlive their entries, as they must for `SubmitMesh`. Ids are reused after `Remove`.

### Parenting (v0.7.1)

`child.SetParent(parent)` makes the child's transforms **local to its parent**. In 3D its world
transform is the parent's world `Transform3DComponent` × its own. In 2D its `TransformComponent`
position turns with the parent's world rotation and is added to the parent's world position; z
and `Rotation` add; `Size` is **not** inherited, because it is a dimension rather than a scale. A
root's local transform *is* its world transform, so a scene without parents behaves exactly as
before.

```cpp
Entity turret = scene->CreateEntity("Turret");
turret.AddComponent<Transform3DComponent>().Position = { 0.0f, 0.7f, 0.0f };
turret.AddComponent<MeshRendererComponent>(MeshRendererComponent(turretMesh));
turret.SetParent(hull, false);    // keep the local values: 0.7 above the hull, wherever it drives

glm::vec3 muzzle = barrel.GetWorldPosition();   // barrel -> turret -> hull

// 2D: a turret sprite that turns with its hull sprite.
Entity hull2D = scene->CreateEntity("Hull");
hull2D.GetComponent<TransformComponent>().Size = { 4.0f, 1.6f };
hull2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.3f, 0.5f, 0.3f, 1.0f }));

Entity turret2D = scene->CreateEntity("Turret");
turret2D.GetComponent<TransformComponent>().Position = { 0.0f, 0.9f, 0.1f };   // z 0.1 above the hull
turret2D.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.4f, 0.6f, 0.4f, 1.0f }));
turret2D.SetParent(hull2D, false);
float aim = turret2D.GetWorldRotation2D();      // the hull's rotation + the turret's
```

- **`keepWorldTransform`.** By default `SetParent` leaves the entity where it is in the world and
  rewrites its local transform: the `Transform3DComponent` of a 3D entity, the `TransformComponent`
  of any other. `false` keeps the local values, so the entity jumps to its place under the new
  parent. An entity without a `Transform3DComponent` has no 3D local transform to rewrite, so its
  3D descendants do move, and the engine warns. A null `Entity` detaches it, like
  `RemoveParent`; setting the parent it already has changes nothing. A parent in another scene,
  or one that would make a cycle, logs an error and changes nothing. Children keep the order they
  were added in.
- **World values are computed on every call** from the parent chain and never cached, so they are
  never stale. `SetWorldPosition`/`SetWorldRotation` write the local value that produces them.
  The unsuffixed `GetWorld*`/`SetWorld*` calls are 3D: an entity without a
  `Transform3DComponent` (a grouping node) reports its parent's 3D world and ignores the setters.
  2D has its own calls, `GetWorldPosition2D`/`SetWorldPosition2D` (with z) and
  `GetWorldRotation2D`/`SetWorldRotation2D` (degrees).
- **Shear.** A rotated child under a non-uniformly scaled parent is sheared, which position,
  rotation and scale can't express: `GetWorldRotation`/`GetWorldScale` and `keepWorldTransform`
  approximate it, and a `keepWorldTransform` reparent that can only approximate it logs a warning.
  Where that matters, keep a parent's scale uniform and put its scaled mesh on a child of its own. Under a parent with a zero scale on some axis there is no local position that
  gives a chosen world position, so `keepWorldTransform`, `SetWorldPosition` and the physics
  write-back leave the local transform as it was.
- **Mixed trees.** 2D and 3D transforms don't convert into each other: a parent counts as identity
  in a dimension it has no transform for. A 3D entity under a 2D sprite ignores the sprite's
  `TransformComponent`, and a sprite under a 3D entity ignores its `Transform3DComponent` (the 3D
  entity's own `TransformComponent` is still composed, but a 3D entity leaves it at the origin).
- **Subtrees.** `DestroyEntity` destroys the whole subtree, children first, so a child's `OnDestroy`
  still sees its parent; called from a script, the subtree waits for the end of the pass like any
  other destroy. `DuplicateEntity` copies the subtree and gives the copy the source's parent.
- **Readers.** Mesh rendering, sprites, circles and text, point and spot lights (position and
  aim), both camera types, `ScreenPointToRay` and audio (sources and the listener) all use world
  values. Sprites, circles and text sort by world z, and a child at the same world z as its parent
  draws on top of it. Text turns with its world rotation (since v0.8.3).
  `MeshRendererComponent::Visible` is not inherited.

**Physics under a parent.** A body is built from the entity's world transform, with 3D collider
sizes taken from its world scale. The rules below are the 3D ones; 2D bodies follow the same rules
([Bodies under a parent](physics-2d.md#bodies-under-a-parent-v071)).

| Body | Rule |
|---|---|
| Dynamic | Simulated in world space. The engine writes back the local transform that puts it where physics did, so moving the parent doesn't drag it. Nested dynamic bodies are written back parents first. |
| Kinematic with a parent | Driven (`MoveKinematic`) every step to where its world transform will be at the end of that step, so it follows its parent. When an ancestor has a moving body of its own, the engine predicts that ancestor's end-of-step pose from its velocity: exact for a kinematic ancestor, close for a dynamic one or a character controller (this step's contacts and gravity aren't known yet). A kinematic root is still yours to move. |
| Static | Its collider is placed once and stays put; its mesh still follows the parent, so don't parent static bodies to anything that moves. |
| Character controller | Placed from its world transform and written back like a dynamic body. Keep controllers on roots. |

**A kinematic child ignores its ancestors** (v0.8). Its body doesn't collide with any ancestor's
body, and an ancestor's character controller passes through it, so a hitbox on a character's hand
never shoves the character carrying it. Nothing else is filtered: dynamic and static children,
siblings and unrelated bodies collide as before (a dynamic crate still rides its carrier), and 2D
physics has no such rule. The pairs are worked out at the start of every physics step, so a
`SetParent` or `RemoveParent` takes effect on the next one; `Physics3D::IsCollisionIgnored` and
`CharacterController3D::IsBodyIgnored` report them.

Parent an entity before its body is built (before `OnStart`, or before `CreateRigidBody` for a
runtime spawn). `SetParent(parent, false)` on an entity that already has a body doesn't teleport
the body: a dynamic one stays where it is, a kinematic one sweeps to its new place in one step and
a static one stays put.

### Lights (v0.7)

A 3D scene is lit by light **entities**. Each frame the `SceneRenderer` calls `Scene::SubmitLights`,
which hands every light component to `Renderer3D` before the meshes are drawn, so there is
nothing to wire up beyond adding the entities. What each light takes from the entity:

- **Directional and ambient lights** ignore the transform. A directional light's `Direction` is in
  world space.
- **Point and spot lights** take their position from the entity's world position (its
  `Transform3DComponent`, through any [parents](#parenting-v071)), and a spot light aims by rotating
  its local `Direction` with the world rotation. Scale is ignored. An entity without a
  `Transform3DComponent` is skipped, with a one-time warning.
- **`Enabled = false`** on a point or spot light switches it off without losing its settings, so
  game code never has to stash an old `Intensity`.

```cpp
// A lantern. Move its Transform3DComponent each frame and the light follows.
Entity lantern = scene->CreateEntity("Lantern");
lantern.AddComponent<Transform3DComponent>().Position = { 2.0f, 1.5f, -3.0f };
lantern.AddComponent<PointLightComponent>(PointLightComponent({ 1.0f, 0.7f, 0.3f }, 2.0f, 8.0f)); // colour, intensity, range

// With a light in the scene there is no default sun, so a faint ambient keeps the rest from going black.
scene->CreateEntity("Ambient").AddComponent<AmbientLightComponent>(AmbientLightComponent({ 0.4f, 0.5f, 0.8f }, 0.05f));
```

A light on a child entity follows its parent, so a lantern can hang off a character. Keep it a
little outside the character's mesh: a light inside a closed mesh lights none of it, because every
face points away from the light.

Things to know:

- **The default light.** A scene with no light component at all is lit by a default
  `DirectionalLightComponent`, so a 3D scene is never black by accident. Any light component turns
  that default off, even a disabled one, except a point or spot light with no
  `Transform3DComponent`.
- **Ambient is always set.** `Scene::SubmitLights` sets the scene's ambient every frame, even to
  black. A scene whose lights are all switched off therefore goes dark, and a scene lit only by
  point lights has no sun and no ambient until you add an `AmbientLightComponent`.
- **Directional lights add up**, up to four (the first created). The deprecated `Ambient` field (0 by
  default since v1.0, removed in 2.0) still adds white ambient and scales its light by `1 - Ambient`;
  light the scene with an `AmbientLightComponent` instead.
- **Custom passes.** Lights are scene-scoped, so a custom 3D pass on the shared renderer calls
  `scene.SubmitLights(renderer)` between its `BeginScene` and `EndScene`, the same way it calls
  `RenderEntities3D`. Without it the pass gets `Renderer3DParams`' default light, not the scene's
  sun. Both changed in v0.7; see [Migrating from v0.6](lighting.md#migrating-from-v06).
- **Limits.** A scene is lit by at most four directional lights and 32 point and spot lights.
  Falloff, how lights are chosen past the limit, lit materials with specular and emissive, and
  hot-reloading the lit shader are covered in [Lighting](lighting.md).

### Skinned models (v0.8)

A model with bones (see [the asset pipeline](asset-pipeline.md)) draws through a
`SkinnedMeshRendererComponent` on an entity that also has a `Transform3DComponent`. Its skinned
submeshes are skinned on the GPU, one draw each, posed by the entity's [animator](#animating-a-model-v08)
or, without one, by the skeleton's rest pose. Submeshes without a skin draw like a
`MeshRendererComponent`, and the component's `Material` applies to every submesh.

```cpp
Model* fox = Model::LoadFromFile("models/Fox/Fox.gltf");
Material* fur = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("Fox"));
fur->SetTexture(0, fox->GetSubMeshes()[0].DiffuseTexture);

Entity entity = scene.CreateEntity("Fox");
entity.AddComponent<Transform3DComponent>(Transform3DComponent({ 0, 0, 0 }, glm::vec3(0.02f)));
entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(fox)).Material = fur;
```

Loading skinned models, the per-frame instance budget and the 128-joint cap, draw order, custom
skinned shaders and drawing without a scene are in [Animation](animation.md#loading-skinned-models).

### Animating a model (v0.8)

An `AnimatorComponent` next to the `SkinnedMeshRendererComponent` gives the entity an `Animator`,
which plays the model's clips and poses it. `Scene::OnUpdate` advances it by `dt × Speed`, after the
scripts and before physics.

```cpp
entity.AddComponent<AnimatorComponent>(AnimatorComponent("Survey"));   // plays, looping, from the start

// In a script:
Animator* animator = GetScene().GetAnimator(GetEntity());
animator->Play(fox->FindAnimation("Run"), 0.25f);                      // cross-fade over 0.25 s
```

- **`Scene::GetAnimator(entity)`** creates the animator on first use and returns null without an
  `AnimatorComponent` or a model with a skeleton. It survives `OnStop`/`OnStart`, is freed with the
  entity or the component, and starts again from `DefaultClip` if the model changes. `Enabled = false`
  holds the pose. `DuplicateEntity` copies the component; the copy's animator starts from the beginning.
- **Playing, blending and the rest** are the animator's own API, which works without a scene too:
  `Play`/`Stop` and their fades, `Blend1D`, layers, one-shots, retargeting clips from another model.
  See [Animation](animation.md#the-animator) and [Blending](animation.md#blending).

### Animation events (v0.8)

A clip carries named marks on its timeline (an instant, or a range such as a sword's hitbox), added in
code or read from a `.events` file beside the model. A script on the entity receives them as the
animator crosses each one:

```cpp
class Fighter : public ScriptableEntity
{
    void OnAnimationEvent(const AnimationEvent& event) override
    {
        if (event.Name == "hitbox" && event.Type == AnimationEventType::RangeBegin)
            m_Swinging = true;            // or poll GetScene().GetAnimator(GetEntity())->IsEventActive("hitbox")
    }
};
```

`OnAnimationEvent` runs once per event, after every script's `OnUpdate` and before physics. A
`DestroyEntity` there waits for the end of the pass, and a script spawned this frame hears its
entity's first events next frame, once it has started. Authoring, the `.events` format, which clip
fires in a blend and how ranges close are in [Animation](animation.md#events).

### Joints as parents (v0.8)

`child.SetParent(character, "b_RightHand", keepWorldTransform)` attaches the child to a joint of the
character's skinned model. Its world transform is then the character's world × the joint's frame ×
its own local transform, from this frame's pose, so a sword follows the hand through every swing.
Everything that reads world values (rendering, lights, audio, physics) follows the joint, and
`GetParentJoint()` names it. A kinematic body on a joint is driven to this frame's pose before
physics steps, so a hitbox follows the animation, and it ignores its ancestors' bodies (see the rule
above). The scale strip, unknown joint names and the rest are in [Animation](animation.md#sockets).

### 2D UI over a 3D scene

To draw a 2D HUD/UI over a 3D scene, give the scene a **second camera** with an orthographic
`CameraComponent` alongside your `Text`/`Sprite`/`Circle` UI entities. The `SceneRenderer` draws
the 3D world first, then the 2D entities as an overlay on top — no extra calls, and a scene with
only one camera type renders just that pass:

```cpp
Entity ui = scene->CreateEntity("UICamera");
auto& uiCam = ui.AddComponent<CameraComponent>();
uiCam.Type = CameraComponent::ProjectionType::Orthographic;
uiCam.OrthographicSize = 20.0f;             // screen-space UI height, in world units
// ...add Text / Sprite entities for the HUD; they draw on top of the 3D world.
```

(Alternatively, keep the HUD out of the scene and draw it in your own `Renderer2D` pass — what
`examples/DungeonCrawler3D/` does today via `RenderHud()`.)

Per-entity 3D controls live on `Scene` as `glm::vec3` overloads: `SetLinearVelocity` /
`GetLinearVelocity3D`, `ApplyImpulse`, `ApplyForce`, and `GetPhysics3D()` for direct access.
Direct `Physics3D` calls (`MoveKinematic`, `IsBodyValid`, matching a ray-cast hit's `Body`) take
the entity's body handle from `Scene::GetRuntimeBody3D(entity)` — `k_InvalidBody3D` while it has
no live body. `GetRuntimeBody2D` and `GetRuntimeSound` (an `AudioSourceComponent`'s playing
sound) work the same way.

The components hold settings only. The engine keeps each entity's live body, character
controller and sound itself, so copying or assigning a component between entities
(`b.GetComponent<RigidBody3DComponent>() = a.GetComponent<RigidBody3DComponent>()`,
`Scene::DuplicateEntity`) never makes two entities share one.

A settings component is read when the object it describes is built (`OnStart`, `CreateRigidBody`,
a sound's start): editing `RigidBody3DComponent::Type` or a collider's size on a live body changes
nothing until the body is rebuilt, and adding a `RigidBody2D/3DComponent` to a running scene needs
`Scene::CreateRigidBody(entity)`. Removing one (or an `AudioSourceComponent`, or a
`CharacterController3DComponent`) does release the live object at once.

```cpp
// Inside a ScriptableEntity: drive a kinematic platform along a path.
const PhysicsBodyId3D body = GetScene().GetRuntimeBody3D(GetEntity());
if (Physics3D* physics = GetScene().GetPhysics3D(); physics && body != k_InvalidBody3D)
    physics->MoveKinematic(body, target, rotation, deltaTime);
```

`examples/DungeonCrawler3D/` is a worked dungeon-crawler prototype built entirely on this path.

### Screen points and world points (v1.0)

`Scene::ScreenPointToRay(screenPos, viewportSize)` turns a pixel (origin top-left, +Y down, as
`Input::GetMousePosition()` reports it) into a world-space ray through the primary perspective
camera, for picking. `Scene::WorldToScreenPoint(worldPos, viewportSize)` goes the other way, for a
health bar or a name over a 3D entity: it returns the pixel the point projects to through the
primary camera, or `std::nullopt` when the point is on or behind the camera plane, there is no
camera, or the viewport is empty. A point in front of the camera but out of view projects outside
`0..viewportSize`, so check the bounds if that matters. `PerspectiveCamera` has the same pair, and
`Dingo::WorldToScreenPoint(worldPos, viewportSize, view, projection)` in `Core/Ray.h` takes the
matrices directly.

```cpp
const glm::vec2 viewport = { (float)window.GetWidth(), (float)window.GetHeight() };
if (auto pixel = scene->WorldToScreenPoint(enemy.GetWorldPosition() + glm::vec3(0, 2, 0), viewport))
    DrawHealthBar(*pixel);
```

### Custom materials (per-mesh shaders)

By default meshes draw with Renderer3D's built-in lit material, which responds to the scene's
[lights](#lights-v07). Assign a `Material*` to a `MeshRendererComponent` to give that mesh its own
shader, uniforms, and textures. Renderer3D groups meshes by material and draws each material as
one batch, or several once it outgrows `Renderer3DCapabilities` (v0.6.3 — before that the
overflow was dropped).

If all you want is emissive, specular or an albedo texture on top of the engine's lighting, make
the material with `Renderer3D::CreateLitMaterial` instead of writing a shader — see
[Lighting](lighting.md).

The binding convention a custom mesh shader follows:

- **binding 0** — the engine **scene UBO**, bound on every material each frame. Declare it to
  position your vertices (and light, if you want it). It begins with a frozen 96-byte prefix,
  `mat4 ViewProjection; vec4 LightDirection; vec4 Ambient;`, and the camera position, the full
  light block and the fog are appended after it (below). A shader may declare a block shorter than the
  buffer, so one that declares only the prefix — as every custom shader written before v0.7 does —
  compiles and renders as it did.
- **binding 1** — your material's own uniforms (whatever you pass to `Material::SetUniform`). Omit it
  if the material has no params.
- **binding 2+** — the material's textures/samplers (`Material::SetTexture` / `SetSampler`),
  interleaved: texture slot *i* at `2 + 2i`, sampler slot *i* at `3 + 2i`.
- **push constants** — a custom shader may declare one `layout(push_constant)` block, at most 128
  bytes, and fill it with `Material::SetPushConstants(data, size)` (the rest is zeroed). A draw pushes
  the bytes as they are when it is recorded: `Renderer::Draw` records at the call, `Renderer3D` at
  `EndScene`, so a scene gives every mesh of a material one value. A skinned shader's only block is
  `DE_SkinDraw`, which the renderer owns, and `SetPushConstants` refuses it
  ([custom skinned shaders](animation.md#custom-skinned-shaders)), as it refuses an instanced shader's
  push-constant `DE_InstanceDraw` ([below](#instanced-custom-shaders-v10)).

The whole of binding 0, as the engine's own lit shader declares it:

```glsl
struct DirectionalLight
{
    vec4 Direction;       // xyz = the way the light travels
    vec4 Color;           // rgb = colour × intensity
};

struct LocalLight         // a point or spot light; a point light has cone scale 0 and offset 1
{
    vec4 PositionRange;   // xyz = world position, w = range
    vec4 Color;           // rgb = colour × intensity, w = cone scale
    vec4 SpotDirection;   // xyz = the way the cone points, w = cone offset
};

layout(std140, binding = 0) uniform CameraData
{
    mat4  ViewProjection;     // these three are the frozen prefix
    vec4  LightDirection;
    vec4  Ambient;
    vec4  CameraPosition;     // w = 1: world position; w = 0: orthographic, xyz = towards the camera
    vec4  AmbientColor;       // rgb = colour × intensity
    ivec4 LightCounts;        // x = directional lights in use, y = point and spot lights in use
    DirectionalLight DirectionalLights[4];
    LocalLight       LocalLights[32];
    vec4  FogColor;           // rgb = colour, a = max opacity
    vec4  FogParams;          // x = start, y = end, z = density, w = mode: 0 none, 1 linear, 2 exp, 3 exp squared
};
```

The prefix is frozen: later versions only append to the block. It carries one directional light and
no colour or intensity. `LightDirection` is the first directional light's direction, or the default
light's when the scene has none, and `Ambient.x` is the scene's ambient as a single value (the
strongest channel of the ambient colour). A shader that lights itself with the old
`Ambient.x + (1 - Ambient.x) * max(dot(N, -normalize(LightDirection.xyz)), 0)`
therefore still draws a sun in a scene that has none, which the engine's own lit shader does not.
Declare the full block and loop over `LightCounts` to light a material the way the engine does;
[Lighting](lighting.md) has the
falloff and cone formulas, and `src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl` is the
reference loop. The fog members are zero while the scene has no fog (`Renderer3D::SetFog`, or a
`FogComponent`); [Lighting](lighting.md#fog) has the fog factor the lit shader mixes by.

Vertices arrive already in **world space** (Renderer3D transforms them on the CPU while batching;
an [instanced shader](#instanced-custom-shaders-v10) places them itself),
as `a_Position` (location 0), `a_Normal` (1), `a_Color` (2 — the component's `Color`) and
`a_TexCoord` (3 — the mesh's UVs, since v0.6.1). Use exactly these names: D3D11/D3D12 match vertex
inputs by name, Vulkan by location. To texture a mesh, sample a bound texture with `a_TexCoord`:

```glsl
layout(location = 3) in vec2 a_TexCoord;                      // vertex stage; pass it on
layout(set = 0, binding = 2) uniform texture2D u_Albedo;      // Material::SetTexture(0, ...)
layout(set = 0, binding = 3) uniform sampler   u_Sampler;     // Material::SetSampler(0, ...)
// fragment: vec4 albedo = texture(sampler2D(u_Albedo, u_Sampler), v_TexCoord) * v_Color;
```

Every binding the shader declares must be bound before the first draw (the pipeline bakes them):
call `SetTexture`/`SetSampler` when creating the material — `Renderer::GetWhiteTexture()` and
`Renderer::GetClampSampler()` are good defaults for an untextured variant.

```cpp
Shader* shader = Shader::CreateFromSource("Glow", glowSource);   // GLSL with the bindings above
Material* glow = Material::Create(MaterialParams().SetShader(shader).SetCullMode(CullMode::None));
glow->SetUniform(GlowParams{ ... });        // creates the binding-1 UBO

entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color)).Material = glow;
// ...whenever the params change (the renderer re-uploads them every frame itself):
glow->SetUniform(GlowParams{ pulsedIntensity });
```

`examples/DungeonCrawler3D/` shows this: the treasures use a custom unlit/glow material whose
intensity is pulsed each frame (see `GameScripts.cpp`).

#### Instanced custom shaders (v1.0)

A custom material is batched on the CPU unless it opts in with `MaterialParams::SetInstancing(true)`.
Its vertex stage then reads the mesh's own vertices, in model space, as `a_Position` (location 0),
`a_Normal` (1) and `a_TexCoord` (2), and places each copy itself from its instance record. The
engine include `<DingoEngine/Instancing.glsl>` declares the record buffer (`DE_InstanceData`, a
readonly storage block at binding 9) and the draw's first record (`DE_InstanceDraw`, a uniform
block at binding 10), both found by name: define `DE_INSTANCE_DATA_BINDING` /
`DE_INSTANCE_DRAW_BINDING` before the include to move them off a texture's binding.

Define `DE_INSTANCE_DRAW_PUSH` before the include to make `DE_InstanceDraw` a push-constant block
instead, as the engine's own shaders do. Each draw then sets its first record without writing a
buffer, so on Vulkan the instanced draws of a pass no longer end and restart the render pass. The
renderer owns that block: the shader declares no other push-constant block, `Material::SetPushConstants`
refuses it, and `DE_INSTANCE_DRAW_BINDING` no longer applies.

```glsl
#include <DingoEngine/Instancing.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

void main()
{
    InstanceRecord instance = GetInstance(uint(gl_InstanceIndex)); // never gl_BaseInstance: D3D leaves it out
    vec3 world = InstanceWorldPosition(instance, a_Position);
    gl_Position = ViewProjection * vec4(world, 1.0);
    v_Normal = InstanceWorldNormal(instance, a_Normal);
    v_Color = instance.Color;                                     // the component's Color
}
```

Such a material draws every mesh given it instanced, however few copies and even with instancing
turned off, since its shader can't read the CPU batches (its copies culled from the camera may still
cast through the CPU batches, which the shadow pass draws with its own shader). Past
`Renderer3D::k_MaxInstancedDrawsPerFrame` instanced draws in a frame its meshes are skipped
(`Statistics::DroppedInstancedMeshes`, warned once). The draws are booked before culling, so culled
copies count too, and the camera never decides which meshes are skipped. A shader missing either
block, or putting one on a binding the material's textures or samplers use (texture slot *i* binds at `2 + 2i`, sampler
slot *i* at `3 + 2i`, so slot 4's texture or slot 3's sampler meet the default 10 and 9), draws
nothing and casts no shadow (warned once), and so does a translucent material with `Instancing`,
since the translucent pass draws no instances.

#### Winding and culling

`Mesh::CreateBox` and `Mesh::CreateSphere` are wound counter-clockwise seen from outside, and so
are well-formed OBJ, glTF and FBX files: `Model::LoadFromFile` keeps a file's winding. `MaterialParams` matches that by default (`CullMode::Back` with
`FrontCounterClockwise = true`), so a custom material hides back faces with no extra settings, on
Vulkan, D3D11 and D3D12 alike. Use `CullMode::None` for an open mesh you see from both sides.
`CreateSphere` raises `rings` to at least 2 and `segments` to at least 3. Its UVs wrap a loaded texture
upright and unmirrored seen from outside: `v = 1` at the +Y pole, and the image's left and right
edges meet at +X. Before 1.0 the texture showed turned half a turn.

Renderer3D keeps the vertices' order, whether it transforms them on the CPU or instances them on
the GPU, so an entity mirrored by a negative scale on one or three axes is drawn inside out by a
culling material. Give mirrored entities a `CullMode::None` material.

Before v0.8.1, `Material` ignored `FrontCounterClockwise` and treated clockwise triangles as front
faces, so `CullMode::Back` drew these meshes inside out (`CreateSphere`, wound the other way, came
out right). Code that culled with `CullMode::Front` to work around it should switch to
`CullMode::Back`.

## Behaviours: `ScriptableEntity`

Game logic lives in `ScriptableEntity` subclasses. Override the lifecycle hooks,
keep game-specific state as members, and attach an instance to an entity:

```cpp
class BulletScript : public ScriptableEntity
{
public:
    BulletScript(glm::vec2 velocity) : m_Velocity(velocity) {}

protected:
    void OnCreate() override {}                  // attached to an entity (AddScript)
    void OnStart() override {}                   // scene started, before physics
    void OnUpdate(float dt) override
    {
        auto& t = GetComponent<TransformComponent>();   // this entity's component
        t.Position += glm::vec3(m_Velocity * dt, 0.0f);
        if (t.Position.y > 12.0f)
            GetEntity().Destroy();
    }
    void OnDestroy() override {}                  // entity destroyed / scene cleared

private:
    glm::vec2 m_Velocity;
};
```

Attach and (optionally) keep the returned reference:

```cpp
Entity bullet = scene.CreateEntity("Bullet");
bullet.GetComponent<TransformComponent>().Position = spawn;
bullet.AddComponent<SpriteRendererComponent>(SpriteRendererComponent{ yellow });
bullet.AddScript<BulletScript>(glm::vec2{ 0.0f, 20.0f });
```

`Scene::OnUpdate(dt)` drives every behaviour's `OnFixedUpdate` and `OnUpdate`, then the animators
(and their events' `OnAnimationEvent`), then physics. It caps
`dt` at 4/60 s for all of them, so a stall (a cold shader compile, a breakpoint) runs the scene slow
instead of letting bodies tunnel through their colliders. Inside a script you have:

- `GetEntity()` — the entity you're attached to (and `GetEntity().GetComponent<T>()`, `Destroy()`, …).
- `GetScene()` — the owning scene (to spawn or find other entities).
- `GetComponent<T>()` / `HasComponent<T>()` — shorthand for your own entity's components.

The hooks fire in order: **`OnCreate`** when the script is attached, **`OnStart`** once when
the scene starts (`Scene::OnStart`, *before* physics — or on the first `OnUpdate` for scripts
spawned later), then **`OnUpdate`** every frame, and **`OnDestroy`** on removal.

**`OnFixedUpdate(step)`** (v1.0) runs before `OnUpdate`, as many times as the frame's delta covers
the scene's fixed step (`Scene::SetFixedTimeStep`, 1/60 s by default): none in a short frame, two
in a 1/30 s one. Time left over carries into the next frame, and `Scene::GetFixedTimeAlpha()` says
how far it reaches into the next step, for drawing between the last two. At most
`Scene::k_MaxFixedStepsPerUpdate` (8) run in one `OnUpdate`; the rest is dropped. Use it for logic
whose result must not depend on the frame rate (a counter, a cooldown in steps, a replay). By
default physics still steps once a frame with the frame's delta, so forces set in `OnFixedUpdate`
act over that.

### Fixed simulation tick (v1.1)

A scene made with `SceneParams::FixedTick` steps physics on the fixed step too, so the whole
simulation advances in ticks that don't depend on the frame rate: the same inputs give bitwise the
same bodies at 30, 60 or 144 frames a second, as long as the game moves them from `OnFixedUpdate`.
Each tick runs every `OnFixedUpdate(step)`, then the entities destroyed during it are removed, then
one physics step of exactly `step` (3D in `ceil(step * 60 - 0.1)` collision steps however long the
step is, 2D with 4 substeps). `OnUpdate`, the animators and audio then run once with the frame's
delta, so velocities set in `OnUpdate`, root motion and bodies on sockets still depend on the frame
timing, and a socketed kinematic child follows the previous frame's pose.

```cpp
Dingo::SceneParams params;
params.FixedTick = true;
Dingo::Scene* scene = sceneManager.CreateScene("Match", params);
scene->SetFixedTimeStep(1.0f / 30.0f);
```

`Scene::GetTick()` numbers the ticks: 0 after `OnStart`, one more after each tick, so a tick's
`OnFixedUpdate` sees the number of the ticks before it. `SetTick(n)` renumbers the clock (to line
up with a server, or after loading a save) and runs nothing. `GetFixedStepsLastUpdate()` says how
many ticks the last `OnUpdate` ran and `GetDroppedFixedSteps()` how many have been dropped by the
cap since `OnStart`; the F9 panel shows both.

Physics bodies move only on ticks, so drawn as they are they would stutter whenever the frame rate
isn't a multiple of the tick rate. With `SceneParams::InterpolatePhysics` (on by default) the scene
draws each dynamic body, kinematic root body, character controller and their children between
their last two ticks, by `GetFixedTimeAlpha()`. Only drawing sees that: the scene's render calls, lights,
`GetCameraViewProjection`, `ScreenPointToRay`, `WorldToScreenPoint` and the audio listener and
sources. Scripts, queries and `GetComponent` always read the latest tick. A body snaps instead of
sliding when its position was moved since the tick (`SetPosition`, a teleport, a transform the
game wrote); a rotation the game sets every frame (a player turning to face its input) is drawn as
set while the position stays smooth. `RigidBody3DComponent::Interpolate` /
`RigidBody2DComponent::Interpolate = false` draws one body at its latest tick; a character
controller always interpolates.

Game code that places something from a body's position (a follow camera, a health bar) reads the
latest tick, one tick ahead of what is drawn, and would shake against it. Place it from
`scene.GetDrawnWorldTransform(entity)` instead, or make it the body's child.

### Active and inactive entities (v1.0)

`entity.SetActive(false)` switches an entity and its whole subtree off without destroying them:
they keep their components and scripts but don't render, light, cast, simulate, animate, emit
particles or play or hear audio, and their scripts get no `OnUpdate`, `OnFixedUpdate` or
`OnAnimationEvent`. It overrides each component's own switch (`Visible`, `Enabled`, `Playing`).
`IsActive()` is the entity's own flag; `IsActiveInHierarchy()` is false when it or any ancestor is
off, and reparenting under an inactive entity switches a subtree off the same way.

- **Physics**: the bodies and character controllers are destroyed, so `GetRuntimeBody3D` and
  `GetCharacterController` return none and a cached `CharacterController3D*` dies with them. On
  `SetActive(true)` they are built again at the current transform, at rest.
- **Audio**: a playing source stops; a `PlayOnStart` source starts again when the entity comes back
  while the scene runs. `PlayAudioSource` on an inactive entity warns and does nothing.
- **Scripts**: a script whose entity is inactive when the scene starts gets its `OnStart` when the
  entity is first active.
- **Lights**: an inactive light still counts as the scene choosing its lights, like a disabled one,
  so no default light appears.
- **Queries** still see it: `ForEachEntity`, `GetScriptsOfType`, `GetEntityByUUID` and `FindChild`
  include inactive entities.

Because a script has full `GetScene()` access, this is enough to keep game *layers* tiny: a
single **controller** script can build the whole world in `OnStart` (spawn the level, the
player, enemies, the camera + light entities, the HUD) and run the game in `OnUpdate`, while
per-entity scripts own their own behaviour. `OnStart` runs before physics, so rigid bodies the
controller spawns are baked when the scene starts. `examples/DungeonCrawler3D/` is built this
way — its layer only creates the scene, attaches one `DungeonControllerScript`, and drives
update/render; everything else lives in scripts.

### Finding other entities

To act across entities (collisions, "all invaders", …), ask the scene for behaviours
of a given type — this is the EnTT-free replacement for raw component views:

```cpp
for (InvaderScript* invader : GetScene().GetScriptsOfType<InvaderScript>())
{
    auto& it = invader->GetEntity().GetComponent<TransformComponent>();
    if (Overlaps(myBox, it)) { /* hit! */ }
}
```

`Scene::ForEachEntity(callback)` visits every entity if you need a raw sweep.

### Lifetime: what the engine handles for you

- **Safe destruction during updates.** Calling `entity.Destroy()` — on yourself or
  another entity — from within `OnUpdate` is safe; the engine **defers** the actual
  removal to the end of the update pass. `OnDestroy` runs then. Destroying a parent destroys
  its children too, children first.
- **Spawning during updates.** Entities/scripts you create mid-update start running on
  the **next** frame.
- **One caveat — don't hold a component reference across a spawn.** `GetComponent<T>()`
  returns a reference into internal storage; creating an entity or adding a component
  can move that storage. Read the values you need into locals *before* spawning:

  ```cpp
  auto& t = GetComponent<TransformComponent>();
  const glm::vec3 muzzle = { t.Position.x, t.Position.y + 1.0f, 0.0f }; // copy FIRST
  Entity b = GetScene().CreateEntity("Bullet");                        // may move `t`
  b.GetComponent<TransformComponent>().Position = muzzle;              // use the copy
  ```

## SceneManager: multiple scenes & lifecycle

`SceneManager` is the default way to work with scenes — it owns named scenes, drives their
lifecycle, and updates + renders the active one. Each scene has an `OnStart` / `OnUpdate` /
`OnStop` lifecycle: **`OnStart` brings the scene up (scripts' `OnStart`, then physics and
`PlayOnStart` audio), `OnStop` tears it down**, and `IsRunning()` reports the state.

`SetActiveScene(name)` stops the outgoing scene, if any, and starts the new one (v1.0: before, the
first activation only selected the scene and the host had to call `OnStart` itself). So build a
scene's entities before activating it. `OnStart` is a no-op on a running scene, re-activating the
already-active scene is a no-op, and `CreateScene` never activates. Stop the active scene yourself
(`scene->OnStop()`) in `OnDetach`.

```cpp
class GameLayer : public Layer
{
    SceneManager m_Scenes;
    Scene* m_Menu = nullptr;
    Scene* m_Game = nullptr;
    Scene* m_GameOver = nullptr;

    void OnAttach() override
    {
        m_Menu     = m_Scenes.CreateScene("Menu");
        m_Game     = m_Scenes.CreateScene("Game");
        m_GameOver = m_Scenes.CreateScene("GameOver");
        for (Scene* s : { m_Menu, m_Game, m_GameOver }) { s->SetClearColor(bg); SetupCamera(s); }
        BuildMenu();
        m_Scenes.SetActiveScene("Menu");                 // activates and starts it
    }

    void OnDetach() override
    {
        if (Scene* active = m_Scenes.GetActiveScene())
            active->OnStop();                            // explicit teardown
    }

    void OnUpdate(float dt) override
    {
        m_Scenes.OnUpdate(dt);                           // always drive the active scene's scripts + physics

        const std::string active = m_Scenes.GetActiveSceneName();
        // Each switch auto-stops the outgoing scene and starts the incoming one.
        if (active == "Menu" && Input::IsKeyPressed(Key::Space)) { BuildGame(); m_Scenes.SetActiveScene("Game"); }
        else if (active == "Game" && m_State.GameOver)        m_Scenes.SetActiveScene("GameOver");
        else if (active == "GameOver" && Input::IsKeyPressed(Key::Space)) m_Scenes.SetActiveScene("Menu");

        m_Scenes.OnRender();                             // SceneRenderer draws the active scene
    }
};
```

> `Scene::Clear()` destroys **every** entity — including the camera (and light) entity — so
> recreate them wherever you rebuild a scene (a small `SetupCamera(scene)` helper called after each
> `Clear()`). To **restart the active scene** (its own `SetActiveScene` is a no-op), drive the
> lifecycle by hand: `scene->OnStop(); scene->Clear(); /* rebuild + SetupCamera */; scene->OnStart();`.

API: `CreateScene`, `GetScene`, `HasScene`, `SetActiveScene`, `GetActiveScene`,
`GetActiveSceneName`, `OnUpdate`, `OnRender` (no argument — uses the engine `SceneRenderer`),
`Clear`. Scene lifecycle: `OnStart` / `OnStop` / `IsRunning`. The manager owns the scenes.

## Debugging: the F9 Scene tab

**F9** opens the Debug window on its **Scene** tab. It is read-only and costs nothing while it isn't
shown.

- **Scenes**: every live scene that has updated at least once, in the order each first updated, whether it is running and has physics, its entity
  count, its enabled point and spot lights next to the renderer's local-light budget
  (`Renderer3D::GetLocalLightBudget`), and its live bodies and controllers. The budget applies to the
  lights in view, so a level with more lights than that is fine; F4 shows how many a frame drew.
  Below the list, the selected scene's primary camera, light counts (point and spot lights without a
  `Transform3DComponent`, which the renderer ignores, are called out, and so is the default light
  when it is in use), fog components, clear colour, and its 2D and 3D worlds: bodies, sensors,
  controllers and gravity.
- **Entities**: the selected scene's entities as a parent/child tree, roots in entity order (creation
  order, except that an entity reusing a destroyed one's slot sorts in its place) and children in
  child order, with a joint socket shown as `@joint`. A name filter (case-insensitive)
  and a "has component" filter turn the tree into a flat list of matches.
- **Inspector**: the selected entity's UUID, parent, socket and child count, then one section per
  built-in component with its fields. Transforms show the local values and the world values, read
  through the hierarchy. The runtime state behind the settings components (the live 2D or 3D body
  with its position and velocity, the character controller and whether it is grounded, the playing sound, the
  bound animator and each layer's clip, the particle emitters, and the attached script's type and
  whether it has started, and a constraint's id, type, other entity, enabled flag and value) follows in its own sections, marked *runtime, read-only*. A constraint's motor panel is the exception: mode, parameters and targets are editable and show the live angle and torque. So is a ragdoll's section: `Blend`, `Strength`, `DriveFrequency` and `DriveToAnimation` are editable, above each body's joint, mass, speed and motor torque.

The selection is kept by entity UUID and a scene serial that is never reused: destroying the
selected entity empties the inspector, and destroying the selected scene falls back to the first
one, never to an entity or scene that took the old one's place. Editing fields from the tab isn't
supported yet.

## Worked example: Space Invaders

`examples/SpaceInvaders/` is a complete game built on this system and is the best
reference. Its structure mirrors the design above:

- **Three scenes** (`Menu`, `Game`, `GameOver`) swapped via `SceneManager`.
- The player, every invader, every bullet/bomb, and every shield block is an
  **entity** with built-in components (Transform + Sprite for rendering).
- All logic is in **behaviours**: `PlayerScript` (input + firing), `ProjectileScript`
  (movement + collision + scoring), `InvaderScript` (identity/points),
  `ShieldScript` (destructible marker), and `FormationControllerScript` (marching,
  bombing, waves). They find each other with `GetScriptsOfType<...>()`.
- Shared game state (score, lives, wave) lives in a small `GameContext` the layer
  owns and passes to the scripts; the `GameLayer` is a thin orchestrator that builds
  the scenes, spawns the entities, updates the HUD, and switches scenes.

Read `examples/SpaceInvaders/src/GameScripts.cpp` and `GameLayer.cpp` alongside this
guide to see the patterns (including the spawn caveat and `GetScriptsOfType`) applied
in a real game.

## Limits

- No scene serialization (see [Saves & Settings](saves-and-settings.md)), and an entity holds one
  script.
- A scene is lit by at most four directional lights and 32 point and spot lights
  ([Lighting](lighting.md)).
- Culling trusts a mesh's bounds, so a vertex shader that moves vertices needs
  `MaterialParams::SetFrustumCulled(false)`; a skinned mesh is culled by its padded rest-pose bounds.
- Instancing never applies to translucent or skinned meshes, and a custom material instances only
  with a shader written for it. Past `Renderer3D::k_MaxInstancedDrawsPerFrame` instanced draws a
  frame the rest are skipped or batched on the CPU.
- A `Static` mesh is not drawn static when it has a `CullDistance`, a translucent or skinned-only
  material, too many vertices for one draw or a non-affine transform; moving one rebakes its cell.
- `OnFixedUpdate` runs at most `Scene::k_MaxFixedStepsPerUpdate` (8) times an update, and the
  frame's delta is capped at 4/60 s, so a very small fixed step falls behind real time. Physics
  stays on the variable step unless the scene has `SceneParams::FixedTick`.
- Interpolation lerps a body's local position and slerps its rotation; it does not interpolate
  animation, particles or entities that only scripts move. A teleported body or controller that
  has a parent slides over one tick instead of snapping, unless its transform was written too.
- A settings component is read when the object it describes is built; editing it on a live body
  changes nothing until the body is rebuilt.
- 2D and 3D transforms don't convert into each other in a mixed hierarchy.
