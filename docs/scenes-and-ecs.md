# Scenes & ECS

Added in **v0.3**, the scene system lets you model game objects as **entities** with
small built-in **components** for data and rendering, and attach game logic as
**behaviours** (`ScriptableEntity`). Internally it's backed by an entity-component
system, but that backend is a private engine detail — **client code never includes
or links the ECS library**, and no ECS types appear in the public API.

The pieces:

- **`Scene`** — owns a set of entities + their behaviours, and renders the renderable ones.
- **`Entity`** — a lightweight handle for adding/reading components and scripts.
- **Components** — built-in data structs (Transform, Sprite, Circle, Text).
- **`ScriptableEntity`** — base class for your per-entity game logic.
- **`SceneManager`** — owns multiple named scenes and switches the active one.

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

## Rendering a scene

Give the scene a camera and clear colour, then call `OnRender` from your layer. It
draws every entity that has a `TransformComponent` plus a `SpriteRenderer`,
`CircleRenderer`, or `Text` component, wrapping `Renderer2D::BeginScene`/`EndScene`:

```cpp
void GameLayer::OnAttach()
{
    m_Scene.SetViewProjection(BuildCamera());            // a glm::mat4, see 2D Rendering
    m_Scene.SetClearColor({ 0.02f, 0.02f, 0.06f, 1.0f });
}

void GameLayer::OnUpdate(float dt)
{
    m_Scene.OnUpdate(dt);                                  // runs all attached behaviours
    m_Scene.OnRender(Application::Get().GetRenderer2D());  // draws the entities
}
```

## 3D entities (v0.4.1)

The same `Scene` also drives **3D** entities, mirroring the 2D side. A 3D entity carries a
`Transform3DComponent` (and, to be drawn, a `MeshRendererComponent`; to be simulated, a
`RigidBody3DComponent` plus one collider). The 3D physics world is the Jolt-backed
[`Physics3D`](physics-3d.md); meshes are drawn through the engine's `Renderer3D`.

| Component | Fields |
|---|---|
| `Transform3DComponent` | `glm::vec3 Position`, `glm::quat Rotation`, `glm::vec3 Scale`; `GetTransform()` → `mat4`; `SetRotationEuler(degrees)` |
| `MeshRendererComponent` | `Mesh* Mesh` (not owned), `glm::vec4 Color` |
| `RigidBody3DComponent` | `BodyType3D Type` (`Static`/`Dynamic`/`Kinematic`), opaque `RuntimeBody` |
| `BoxCollider3DComponent` | `glm::vec3 HalfExtents` (fraction of `Scale`), `Friction`, `Restitution` |
| `SphereCollider3DComponent` | `float Radius` (fraction of `Scale.x`), `Friction`, `Restitution` |

> A 3D entity still receives the default 2D `TransformComponent` on creation; it simply goes
> unused. The collider shape is **baked into the body at creation** (so a rigid-body entity
> needs exactly one box/sphere collider), and collider sizes are fractions of `Transform3D.Scale`
> — a unit-scaled entity with the default collider exactly fills its mesh.

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
back (2D → `TransformComponent`, 3D → `Transform3DComponent`). Render the 3D entities with a
perspective camera:

```cpp
PerspectiveCamera camera(50.0f, aspect, 0.1f, 500.0f);
camera.SetPosition(focus + glm::vec3{ 0, 15, 11 });  // a follow camera, updated each frame
camera.SetTarget(focus);

scene.OnUpdate(dt);
scene.OnRender3D(Application::Get().GetRenderer3D(), camera);  // clears + draws Transform3D+Mesh entities
```

Per-entity 3D controls live on `Scene` as `glm::vec3` overloads: `SetLinearVelocity` /
`GetLinearVelocity3D`, `ApplyImpulse`, `ApplyForce`, and `GetPhysics3D()` for direct access.
`examples/DungeonCrawler3D/` is a worked dungeon-crawler prototype built entirely on this path.

## Behaviours: `ScriptableEntity`

Game logic lives in `ScriptableEntity` subclasses. Override the lifecycle hooks,
keep game-specific state as members, and attach an instance to an entity:

```cpp
class BulletScript : public ScriptableEntity
{
public:
    BulletScript(glm::vec2 velocity) : m_Velocity(velocity) {}

protected:
    void OnCreate() override {}                  // attached to an entity
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

`Scene::OnUpdate(dt)` drives every behaviour's `OnUpdate`. Inside a script you have:

- `GetEntity()` — the entity you're attached to (and `GetEntity().GetComponent<T>()`, `Destroy()`, …).
- `GetScene()` — the owning scene (to spawn or find other entities).
- `GetComponent<T>()` / `HasComponent<T>()` — shorthand for your own entity's components.

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

## SceneManager: multiple scenes

`SceneManager` owns named scenes and tracks the active one, updating and rendering
only that scene:

```cpp
class GameLayer : public Layer
{
    SceneManager m_Scenes;
    Scene* m_Menu = nullptr;
    Scene* m_Game = nullptr;
    Scene* m_GameOver = nullptr;

    void OnAttach() override
    {
        m_Menu     = m_Scenes.CreateScene("Menu");      // first scene becomes active
        m_Game     = m_Scenes.CreateScene("Game");
        m_GameOver = m_Scenes.CreateScene("GameOver");
        for (Scene* s : { m_Menu, m_Game, m_GameOver }) { s->SetViewProjection(cam); s->SetClearColor(bg); }
        BuildMenu();
    }

    void OnUpdate(float dt) override
    {
        const std::string active = m_Scenes.GetActiveSceneName();
        if (active == "Menu")
        {
            if (Input::IsKeyDown(Key::Space)) { StartGame(); m_Scenes.SetActiveScene("Game"); }
        }
        else if (active == "Game")
        {
            m_Scenes.OnUpdate(dt);                       // runs the active scene's scripts
            if (m_State.GameOver) m_Scenes.SetActiveScene("GameOver");
        }
        else if (active == "GameOver")
        {
            if (Input::IsKeyDown(Key::Space)) m_Scenes.SetActiveScene("Menu");
        }
        m_Scenes.OnRender(Application::Get().GetRenderer2D());
    }
};
```

API: `CreateScene`, `GetScene`, `HasScene`, `SetActiveScene`, `GetActiveScene`,
`GetActiveSceneName`, `OnUpdate`, `OnRender`, `Clear`. The manager owns the scenes.

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
