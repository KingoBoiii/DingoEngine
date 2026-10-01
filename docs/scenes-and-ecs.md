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

### Entity API

| Call | Effect |
|---|---|
| `AddComponent<T>(value)` | Attach a built-in component `T`. Returns `T&`. |
| `GetComponent<T>()` | Reference to the entity's `T`. |
| `HasComponent<T>()` | `true` if the entity has a `T`. |
| `RemoveComponent<T>()` | Detach the `T`. |
| `AddScript<T>(args...)` | Attach a behaviour (see below). |
| `GetScript<T>()` / `HasScript<T>()` | The attached `T` behaviour, or null. |
| `GetUUID()` / `GetName()` | The entity's id / name. |
| `IsValid()` / `operator bool` | `false` for a null or destroyed entity. |
| `Destroy()` | Destroy this entity (and its behaviour). |

> The component methods support the **built-in component types** (below). To carry
> game-specific data, put it in a `ScriptableEntity` subclass rather than defining new
> component types — that's what keeps the ECS backend hidden.

## Built-in components

| Component | Fields |
|---|---|
| `TransformComponent` | `glm::vec3 Position` (center), `float Rotation` (degrees, +Z), `glm::vec2 Size`; `GetTransform()` → `mat4` |
| `SpriteRendererComponent` | `glm::vec4 Color`, `Texture* Texture` (null ⇒ solid colour) |
| `CircleRendererComponent` | `glm::vec4 Color`, `float Thickness`, `float Fade` |
| `TextComponent` | `std::string Text`, `Font* Font`, `glm::vec4 Color`, `float Size`, `bool Centered` |
| `TagComponent` / `IDComponent` | Name / `UUID` (added automatically) |
| `CameraComponent` | `ProjectionType Type` (`Orthographic`/`Perspective`), ortho `OrthographicSize`/`OrthoNear`/`OrthoFar`, perspective `FOV`/`PerspNear`/`PerspFar`, `bool Primary`; the camera the `SceneRenderer` views the scene through |
| `DirectionalLightComponent` | `glm::vec3 Direction` (the way the light travels), `glm::vec3 Color` and `float Intensity` (v0.7), `float Ambient` (the original single knob, see [Lights](#lights-v07)) — a sun |
| `AmbientLightComponent` (v0.7) | `glm::vec3 Color`, `float Intensity` — light that reaches every face equally; all of them add up |
| `PointLightComponent` (v0.7) | `glm::vec3 Color`, `float Intensity`, `float Range`, `bool Enabled` — light in every direction from the entity's `Transform3DComponent` position |
| `SpotLightComponent` (v0.7) | `glm::vec3 Color`, `float Intensity`, `float Range`, `float InnerConeAngle` / `OuterConeAngle` (degrees), `glm::vec3 Direction` (local space, default `{ 0, 0, -1 }`), `bool Enabled` — a cone of light from the entity's `Transform3DComponent` |

The four light components are read by the `SceneRenderer` for the 3D pass — see [Lights](#lights-v07).

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

    m_Scenes.SetActiveScene("Game");   // select the active scene...
    m_Game->OnStart();                 // ...and start it (see Lifecycle, under SceneManager)
}

void GameLayer::OnUpdate(float dt)
{
    m_Scenes.OnUpdate(dt);   // runs the active scene's behaviours + physics
    m_Scenes.OnRender();     // draws it through its camera (no renderer argument)
}
```

> The viewport aspect is applied automatically, so a camera tracks window resizes with no
> per-layer bookkeeping. For a **custom overlay** in the same view (e.g. a HUD over the
> entities), wrap your own `Renderer2D::BeginScene`/`EndScene` around `Scene::RenderEntities`,
> taking the matrix from `Scene::GetActiveCameraViewProjection(aspect)`.

## 3D entities (v0.4.1)

The same `Scene` also drives **3D** entities, mirroring the 2D side. A 3D entity carries a
`Transform3DComponent` (and, to be drawn, a `MeshRendererComponent`; to be simulated, a
`RigidBody3DComponent` plus one collider). The 3D physics world is the Jolt-backed
[`Physics3D`](physics-3d.md); meshes are drawn through the engine's `Renderer3D`.

| Component | Fields |
|---|---|
| `Transform3DComponent` | `glm::vec3 Position`, `glm::quat Rotation`, `glm::vec3 Scale`; `GetTransform()` → `mat4`; `SetRotationEuler(degrees)` |
| `MeshRendererComponent` | `Mesh* Mesh` (not owned), `glm::vec4 Color`, `Material* Material` (optional; null = the built-in lit material) |
| `RigidBody3DComponent` | `BodyType3D Type` (`Static`/`Dynamic`/`Kinematic`), `bool ContinuousCollision` (v0.6.2) |
| `BoxCollider3DComponent` | `glm::vec3 HalfExtents` (fraction of `Scale`), `Friction`, `Restitution` |
| `SphereCollider3DComponent` | `float Radius` (fraction of `Scale.x`), `Friction`, `Restitution` |
| `CapsuleCollider3DComponent` | `float Radius` (fraction of `Scale.x`), `float HalfHeight` (fraction of `Scale.y`), `Friction`, `Restitution` |
| `MeshCollider3DComponent` (v0.6.2) | `Mesh* Mesh` (not owned; null = the entity's `MeshRendererComponent::Mesh`), `bool Convex`, `Friction`, `Restitution` — scaled by the full `Scale`, see [Mesh colliders](physics-3d.md#mesh-colliders-v062) |

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
scene.OnPhysicsStart();                              // builds a 3D world only if 3D bodies exist
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
scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>(); // defaults match the built-in light

// Each frame — drive the camera transform (the view is inverse(translate(P) * rotate(R))):
auto& t = cam.GetComponent<Transform3DComponent>();
t.Position = eye;                                                  // e.g. focus + { 0, 15, 11 }
t.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(eye, focus, { 0, 1, 0 })));

scenes.OnUpdate(dt);
scenes.OnRender();   // SceneRenderer clears + draws the Transform3D+Mesh entities, lit by the scene's lights
```

### Lights (v0.7)

A 3D scene is lit by light **entities**. Each frame the `SceneRenderer` calls `Scene::SubmitLights`,
which hands every light component to `Renderer3D` before the meshes are drawn, so there is
nothing to wire up beyond adding the entities. What each light takes from the entity:

- **Directional and ambient lights** ignore the transform. A directional light's `Direction` is in
  world space.
- **Point and spot lights** take their position from `Transform3DComponent::Position`, and a spot
  light aims by rotating its local `Direction` with `Transform3DComponent::Rotation`. Scale is
  ignored. An entity without a `Transform3DComponent` is skipped.
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

There is no parent-child transform yet, so a lantern that follows a character is a separate
entity that you move by hand, a little outside the character's mesh. A light inside a closed mesh
lights none of it, because every face points away from the light.

Things to know:

- **The default light.** A scene with no light component at all is lit by a default
  `DirectionalLightComponent`, so a 3D scene is never black by accident. Any light component turns
  that default off, even a disabled one or one on an entity with no `Transform3DComponent`.
- **Ambient is always set.** `Scene::SubmitLights` sets the scene's ambient every frame, even to
  black. A scene whose lights are all switched off therefore goes dark, and a scene lit only by
  point lights has no sun and no ambient until you add an `AmbientLightComponent`.
- **Directional lights add up**, up to four, and so does each one's legacy `Ambient`. Set
  `Ambient` to 0 to light the scene with `AmbientLightComponent` instead.
- **Custom passes.** Lights are scene-scoped, so a custom 3D pass on the shared renderer calls
  `scene.SubmitLights(renderer)` between its `BeginScene` and `EndScene`, the same way it calls
  `RenderEntities3D`. Without it the pass gets `Renderer3DParams`' default light, not the scene's
  sun. Both changed in v0.7; see [Migrating from v0.6](lighting.md#migrating-from-v06).
- **Limits.** A scene is lit by at most four directional lights and 32 point and spot lights.
  Falloff, how lights are chosen past the limit, lit materials with specular and emissive, and
  hot-reloading the lit shader are covered in [Lighting](lighting.md).

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

```cpp
// Inside a ScriptableEntity: drive a kinematic platform along a path.
const PhysicsBodyId3D body = GetScene().GetRuntimeBody3D(GetEntity());
if (Physics3D* physics = GetScene().GetPhysics3D(); physics && body != k_InvalidBody3D)
    physics->MoveKinematic(body, target, rotation, deltaTime);
```

`examples/DungeonCrawler3D/` is a worked dungeon-crawler prototype built entirely on this path.

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
  `mat4 ViewProjection; vec4 LightDirection; vec4 Ambient;`, and the camera position and the full
  light block are appended after it (below). A shader may declare a block shorter than the
  buffer, so one that declares only the prefix — as every custom shader written before v0.7 does —
  compiles and renders as it did.
- **binding 1** — your material's own uniforms (whatever you pass to `Material::SetUniform`). Omit it
  if the material has no params.
- **binding 2+** — the material's textures/samplers (`Material::SetTexture` / `SetSampler`),
  interleaved: texture slot *i* at `2 + 2i`, sampler slot *i* at `3 + 2i`.

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
reference loop.

Vertices arrive already in **world space** (Renderer3D transforms them on the CPU while batching),
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

`Scene::OnUpdate(dt)` drives every behaviour's `OnUpdate`. It caps `dt` at 4/60 s, for scripts and
physics alike, so a stall (a cold shader compile, a breakpoint) runs the scene slow instead of letting
bodies tunnel through their colliders. Inside a script you have:

- `GetEntity()` — the entity you're attached to (and `GetEntity().GetComponent<T>()`, `Destroy()`, …).
- `GetScene()` — the owning scene (to spawn or find other entities).
- `GetComponent<T>()` / `HasComponent<T>()` — shorthand for your own entity's components.

The hooks fire in order: **`OnCreate`** when the script is attached, **`OnStart`** once when
the scene starts (`Scene::OnStart`, *before* physics — or on the first `OnUpdate` for scripts
spawned later), then **`OnUpdate`** every frame, and **`OnDestroy`** on removal.

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
  removal to the end of the update pass. `OnDestroy` runs then.
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
`OnStop` lifecycle: **`OnStart` brings the scene up (starts physics, `OnPhysicsStart`), `OnStop`
tears it down (`OnPhysicsStop`)**, and `IsRunning()` reports the state.

Lifecycle is **explicit**: you call `scene->OnStart()` (typically the last thing in `OnAttach`)
and `scene->OnStop()` (in `OnDetach`). `SetActiveScene(name)` only *selects* the active scene on
its **first** call — so your explicit `OnStart` is the real start. A later **switch** between
scenes does run `OnStop` on the outgoing scene and `OnStart` on the incoming one, so multi-scene
transitions stay one-liners. Re-activating the already-active scene is a no-op, and `CreateScene`
never activates.

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
        m_Scenes.SetActiveScene("Menu");                 // selects the active scene...
        m_Menu->OnStart();                               // ...you start it explicitly
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

---

Back to the [documentation index](README.md).
