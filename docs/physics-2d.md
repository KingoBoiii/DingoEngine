# 2D Physics

*(v0.4)*

DingoEngine integrates a 2D rigid-body simulation into the scene system. You add
physics **components** to entities, start the simulation, and the engine steps a
physics world each frame and writes the simulated positions/rotations back onto
your `TransformComponent`s.

The simulation backend is [Box2D](https://box2d.org/) (v3), but — exactly like the
EnTT-based ECS — it is an internal detail: **no Box2D type appears in any public
header**, and your game never includes or links Box2D. Bodies and shapes are
referred to through opaque handles stored on the components.

> World units are metres. Because the physics world shares the coordinate space of
> your orthographic camera, 1 world unit = 1 metre = 1 Box2D unit. Tune sizes,
> gravity, and speeds in that space.

## Components

Add these alongside a `TransformComponent` (every entity already has one).

### `RigidBody2DComponent`

```cpp
struct RigidBody2DComponent
{
    enum class BodyType { Static, Dynamic, Kinematic };
    BodyType Type = BodyType::Static;
    bool FixedRotation = false; // lock rotation about Z (e.g. a player character)
};
```

| `BodyType` | Behaviour |
|---|---|
| `Static`    | Never moves (ground, walls). Zero mass. |
| `Dynamic`   | Fully simulated — gravity, forces, collisions. |
| `Kinematic` | Moves only by the velocity you set; ignores forces and gravity. |

### `BoxCollider2DComponent` / `CircleCollider2DComponent`

```cpp
struct BoxCollider2DComponent
{
    glm::vec2 Offset{ 0.0f };
    glm::vec2 Size{ 0.5f };   // half-extents, as a fraction of TransformComponent::Size
    float Density = 1.0f;
    float Friction = 0.5f;
    float Restitution = 0.0f; // bounciness, [0,1]
};

struct CircleCollider2DComponent
{
    glm::vec2 Offset{ 0.0f };
    float Radius = 0.5f;      // fraction of TransformComponent::Size.x
    float Density = 1.0f;
    float Friction = 0.5f;
    float Restitution = 0.0f;
};
```

Collider extents are expressed as a **fraction of the entity's `Transform.Size`**,
so the defaults (`Size = {0.5, 0.5}`, `Radius = 0.5`) make the collider exactly fill
the entity's quad. A `Density` of 0 on a dynamic body falls back to a small mass so
the body still simulates. `Offset` is in the same fractional units, relative to the
entity center.

## Lifecycle

```cpp
Scene scene;

// 1. Build your physics entities.
Entity ground = scene.CreateEntity("Ground");
ground.GetComponent<TransformComponent>().Size = { 40.0f, 2.0f };
ground.AddComponent<RigidBody2DComponent>(RigidBody2DComponent::BodyType::Static);
ground.AddComponent<BoxCollider2DComponent>();

Entity crate = scene.CreateEntity("Crate");
crate.GetComponent<TransformComponent>().Position = { 0.0f, 5.0f, 0.0f };
crate.AddComponent<SpriteRendererComponent>(glm::vec4(0.6f, 0.4f, 0.2f, 1.0f));
crate.AddComponent<RigidBody2DComponent>(RigidBody2DComponent::BodyType::Dynamic);
crate.AddComponent<BoxCollider2DComponent>();

// 2. Configure gravity and start the simulation. OnPhysicsStart creates a body
//    (and collider shapes) for every entity that currently has a RigidBody2DComponent.
scene.SetGravity({ 0.0f, -9.81f });
scene.OnPhysicsStart();

// 3. Each frame: Scene::OnUpdate steps the world and syncs transforms back.
scene.OnUpdate(deltaTime);

// 4. Tear it down (also done automatically by Scene::Clear and the destructor).
scene.OnPhysicsStop();
```

`Scene::OnUpdate` runs your `ScriptableEntity` behaviours first (so a script can
apply forces this frame), then steps the world and copies each body's position and
rotation onto its `TransformComponent`. Rendering afterwards just draws the synced
transforms — no extra work needed.

Destroying an entity (`Entity::Destroy()` / `Scene::DestroyEntity`) automatically
releases its physics body and shapes.

### Spawning bodies at runtime

`OnPhysicsStart` only creates bodies for entities that exist when it is called. For
something spawned later (a projectile, a new enemy), add the components and then ask
the scene to instantiate the body:

```cpp
Entity ball = scene.CreateEntity("Ball");
ball.GetComponent<TransformComponent>().Position = { x, y, 0.0f };
ball.AddComponent<RigidBody2DComponent>(RigidBody2DComponent::BodyType::Dynamic);
ball.AddComponent<CircleCollider2DComponent>();
scene.CreateRigidBody(ball);                 // builds the body from the current transform
scene.SetLinearVelocity(ball, { 12.0f, 8.0f });
```

This is exactly how the Angry Birds example keeps a bird resting on the slingshot
(a body-less sprite) until launch, then gives it a dynamic body and flings it.

## Controlling bodies

All controls take an `Entity` and are no-ops if it has no live body. They keep
Box2D out of your code:

```cpp
scene.SetGravity({ 0.0f, -9.81f });
glm::vec2 g = scene.GetGravity();
bool running = scene.IsPhysicsRunning();

scene.SetLinearVelocity(entity, { 5.0f, 0.0f });
glm::vec2 v = scene.GetLinearVelocity(entity);

scene.ApplyLinearImpulseToCenter(entity, { 0.0f, 10.0f }); // instant change in momentum
scene.ApplyLinearImpulse(entity, impulse, worldPoint);     // off-center => adds spin
scene.ApplyForceToCenter(entity, { 0.0f, 50.0f });         // continuous push (per step)
```

## Collisions

The simulation resolves collisions for you: dynamic bodies bounce, stack, topple,
and rest on static geometry automatically. There is no contact-callback API yet, so
gameplay reactions are driven by observing the simulation — e.g. reading
`GetLinearVelocity` to detect a hard impact, or checking when a body has fallen out
of the play area. The Angry Birds `PigScript` uses exactly this approach to decide
when a pig has been knocked out.

## See also

- [Scenes & ECS](scenes-and-ecs.md) — entities, components, and `ScriptableEntity`.
- The **AngryBirds** example (`examples/AngryBirds/`) — slingshot launching,
  destructible block structures, and pig targets built entirely on this system.
