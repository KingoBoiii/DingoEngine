# Scenes & ECS

Added in **v0.3**, the scene system lets you model game objects as **entities** made
of small **components**, instead of bespoke classes. It is built on
[EnTT](https://github.com/skypjack/entt). Game logic runs as **systems** — plain
functions that query a scene for the entities they care about and update them.

The pieces:

- **`Scene`** — owns a set of entities and knows how to render the renderable ones.
- **`Entity`** — a lightweight handle for adding/reading components.
- **Components** — plain data structs (the engine ships a handful; you add your own).
- **`SceneManager`** — owns multiple named scenes and switches the active one.

Everything is available through `<DingoEngine.h>`.

## Creating a scene and entities

```cpp
Scene scene("Game");

Entity player = scene.CreateEntity("Player");          // auto-gets ID + Tag + Transform
player.GetComponent<TransformComponent>().Position = { 0.0f, -4.0f, 0.0f };
player.GetComponent<TransformComponent>().Size     = { 1.5f, 0.6f };
player.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.3f, 0.9f, 0.4f, 1.0f });
```

Every entity created via `CreateEntity` automatically gets an `IDComponent`
(a stable `UUID`), a `TagComponent` (its name), and a `TransformComponent`.

### Entity API

| Call | Effect |
|---|---|
| `AddComponent<T>(args...)` | Construct and attach a `T`. Returns `T&` (or `void` for empty tag types). |
| `AddOrReplaceComponent<T>(args...)` | Attach or overwrite. |
| `GetComponent<T>()` | Reference to the entity's `T` (asserts it exists). |
| `HasComponent<T>()` | `true` if the entity has a `T`. |
| `RemoveComponent<T>()` | Detach the `T`. |
| `GetUUID()` / `GetName()` | The entity's id / tag. |
| `operator bool()` | `false` for a null/default entity. |

An `Entity` is just `{ id, Scene* }` — cheap to copy and store.

## Built-in components

| Component | Fields |
|---|---|
| `IDComponent` | `UUID ID` |
| `TagComponent` | `std::string Tag` |
| `TransformComponent` | `glm::vec3 Position` (center), `float Rotation` (degrees, +Z), `glm::vec2 Size`; `GetTransform()` → `mat4` |
| `SpriteRendererComponent` | `glm::vec4 Color`, `Texture* Texture` (null ⇒ solid colour) |
| `CircleRendererComponent` | `glm::vec4 Color`, `float Thickness`, `float Fade` |
| `TextComponent` | `std::string Text`, `Font* Font`, `glm::vec4 Color`, `float Size`, `bool Centered` |

`TransformComponent` mirrors the `Renderer2D` conventions: position is the entity's
center, size is full extent, rotation is in degrees.

## Rendering a scene

Give the scene a camera and a clear colour, then call `OnRender` inside your layer's
update. `Scene::OnRender` wraps `BeginScene`/`EndScene` for you and draws every entity
that has a `TransformComponent` plus a `SpriteRenderer`, `CircleRenderer`, or `Text`
component:

```cpp
void GameLayer::OnAttach()
{
    m_Scene.SetViewProjection(BuildCamera());        // a glm::mat4, see 2D Rendering
    m_Scene.SetClearColor({ 0.02f, 0.02f, 0.06f, 1.0f });
}

void GameLayer::OnUpdate(float dt)
{
    // ...run systems that mutate components...
    m_Scene.OnRender(Application::Get().GetRenderer2D());
}
```

That's the whole render path: add renderable components to entities, and the scene
draws them.

## Defining your own components

Components are just structs — no base class, no registration:

```cpp
struct VelocityComponent { glm::vec2 Velocity{ 0.0f }; };
struct HealthComponent   { int Current = 100; int Max = 100; };

// "Tag" components carry no data and mark a category of entity:
struct PlayerTag {};
struct EnemyTag  {};
```

Attach them like any other component:

```cpp
Entity bullet = scene.CreateEntity("Bullet");
bullet.AddComponent<VelocityComponent>().Velocity = { 0.0f, 20.0f };
bullet.AddComponent<PlayerTag>();
```

> **Empty/tag components.** EnTT stores nothing for an empty struct, so
> `AddComponent<PlayerTag>()` returns `void` (this is fine — the engine's
> `AddComponent` is declared `decltype(auto)`). Use tags as **query filters** and with
> `HasComponent<PlayerTag>()`; don't call `GetComponent<PlayerTag>()` on them.

## Systems: querying entities

A "system" is just code that asks the scene for entities with a given set of
components and acts on them. Use `Scene::GetAllEntitiesWith<...>()`, which returns an
EnTT view you can iterate:

```cpp
// Move every entity that has both a Transform and a Velocity.
void MovementSystem(Scene& scene, float dt)
{
    auto view = scene.GetAllEntitiesWith<TransformComponent, VelocityComponent>();
    for (auto entity : view)
    {
        auto [transform, velocity] = view.get<TransformComponent, VelocityComponent>(entity);
        transform.Position.x += velocity.Velocity.x * dt;
        transform.Position.y += velocity.Velocity.y * dt;
    }
}
```

For a single component, `view.get<T>(entity)` returns a `T&` directly. The view only
visits entities that have *all* the listed components, so filtering by a tag is just
adding it to the list: `GetAllEntitiesWith<EnemyTag, TransformComponent>()`.

### Two gotchas worth internalizing

These come straight from EnTT's storage model and will bite you if ignored:

**1. Don't hold a component reference across entity/component creation.** Creating an
entity (or adding a component of the same type) can reallocate that component's
storage and invalidate existing references. Read what you need into locals *before*
creating:

```cpp
auto& t = player.GetComponent<TransformComponent>();
const float spawnX = t.Position.x;          // copy out FIRST
const float spawnY = t.Position.y + 1.0f;

Entity bullet = scene.CreateEntity("Bullet"); // <-- may invalidate `t`
auto& bt = bullet.GetComponent<TransformComponent>();
bt.Position = { spawnX, spawnY, 0.0f };       // use the copies, not `t`
```

**2. Don't destroy entities while iterating a view.** Collect the entities to remove,
then destroy them after the loop, guarding with `registry.valid()`:

```cpp
std::vector<entt::entity> toDestroy;
auto& registry = scene.GetRegistry();

auto view = scene.GetAllEntitiesWith<BulletTag, TransformComponent>();
for (auto e : view)
{
    auto& t = view.get<TransformComponent>(e);
    if (t.Position.y > topEdge)
        toDestroy.push_back(e);               // defer, don't destroy here
}

for (auto e : toDestroy)
    if (registry.valid(e))
        scene.DestroyEntity(Entity{ e, &scene });
```

Other scene helpers: `DestroyEntity(entity)`, `Clear()` (remove all entities,
keeping the scene), `GetEntityByUUID(uuid)`, and `GetRegistry()` for direct EnTT
access when you need it.

## SceneManager: multiple scenes

Real games have more than one screen. `SceneManager` owns named scenes and tracks
which one is active, rendering only that one:

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

        for (Scene* s : { m_Menu, m_Game, m_GameOver })
        {
            s->SetViewProjection(camera);
            s->SetClearColor({ 0.02f, 0.02f, 0.06f, 1.0f });
        }
        BuildMenu();   // populate the menu scene with text entities
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
            // run gameplay systems on m_Game ...
            if (playerDied) m_Scenes.SetActiveScene("GameOver");
        }
        else if (active == "GameOver")
        {
            if (Input::IsKeyDown(Key::Space)) m_Scenes.SetActiveScene("Menu");
        }

        m_Scenes.OnRender(Application::Get().GetRenderer2D());   // draws the active scene
    }
};
```

`SceneManager` API: `CreateScene(name)`, `GetScene(name)`, `HasScene(name)`,
`SetActiveScene(name)`, `GetActiveScene()`, `GetActiveSceneName()`, `OnRender(renderer)`,
and `Clear()`. The manager owns the scenes and deletes them when destroyed.

## Worked example: Space Invaders

The `examples/SpaceInvaders/` project is a complete game built entirely on this
system and is the best reference:

- **Three scenes** (`Menu`, `Game`, `GameOver`) swapped via `SceneManager`.
- The player, every invader, every bullet/bomb, and every destructible shield block is
  an **entity** in the `Game` scene.
- Game-specific components (`PlayerTag`, `InvaderTag`, `BulletTag`, `ShieldTag`,
  `VelocityComponent`) live in the example, not the engine.
- Gameplay is a set of systems — player movement/firing, formation marching, invader
  fire, projectile motion, AABB collisions, and a live HUD — each querying the scene
  with `GetAllEntitiesWith<...>()`.

Read `examples/SpaceInvaders/src/GameLayer.cpp` alongside this guide to see the
patterns above (including both gotchas) applied in a real game.

---

Back to the [documentation index](README.md).
