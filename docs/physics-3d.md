# 3D Physics

*(v0.4)*

Alongside the 2D physics ([physics-2d.md](physics-2d.md)), DingoEngine provides a **3D
rigid-body world** behind the backend-agnostic **`Physics3D`** interface, backed by [Jolt Physics](https://github.com/jrouwe/JoltPhysics).
As with EnTT and Box2D, the backend is an internal detail: **no Jolt type appears in any public
header**, and your game never includes or links Jolt — bodies are referred to through opaque
`PhysicsBodyId3D` handles.

> **Two ways to use it.** `Physics3D` is a self-contained world you can drive yourself —
> create bodies, `Step()` each frame, then read each body's transform to render it however you
> like (the lifecycle and API below). **As of v0.4.1 it is also wired into the `Scene`/ECS**,
> exactly like the 2D world: add a `RigidBody3DComponent` plus a `BoxCollider3DComponent` or
> `SphereCollider3DComponent` to a `Transform3DComponent` entity and the `Scene` builds the body,
> steps the world, and writes the simulated transform back for you — see
> [scenes-and-ecs.md](scenes-and-ecs.md). The standalone API documented here is unchanged and
> still the right tool when you want a physics world without the ECS.

World units are metres (1 unit = 1 metre = 1 Jolt unit).

## Lifecycle

```cpp
#include <DingoEngine.h>
using namespace Dingo;

Physics3DParams params;
params.Gravity = { 0.0f, -9.81f, 0.0f };
Physics3D* world = Physics3D::Create(); // selects the Jolt backend
world->Initialize(params);              // brings the world live

// ... create bodies, simulate ...

delete world; // virtual dtor → Shutdown(); tears down the world (and Jolt itself once the last world is gone)
```

## Creating bodies

A body is described by a `RigidBodyParams3D`: a body type, a collider shape (box or sphere),
a transform, and material properties.

```cpp
// Static floor (a thin, wide box; top surface at y = 0)
RigidBodyParams3D floor(BodyType3D::Static, ColliderShape3D::Box);
floor.Position    = { 0.0f, -0.5f, 0.0f };
floor.HalfExtents = { 20.0f, 0.5f, 20.0f };
PhysicsBodyId3D floorId = world->CreateBody(floor);

// Dynamic box
RigidBodyParams3D box(BodyType3D::Dynamic, ColliderShape3D::Box);
box.Position    = { 0.0f, 5.0f, 0.0f };
box.HalfExtents = { 0.5f, 0.5f, 0.5f };
box.Friction    = 0.6f;
box.Restitution = 0.05f;
PhysicsBodyId3D boxId = world->CreateBody(box);

// Dynamic sphere with an initial velocity
RigidBodyParams3D ball(BodyType3D::Dynamic, ColliderShape3D::Sphere);
ball.Position = { 0.0f, 2.0f, 12.0f };
ball.Radius   = 0.6f;
PhysicsBodyId3D ballId = world->CreateBody(ball);
world->SetLinearVelocity(ballId, { 0.0f, 2.0f, -25.0f });
```

| `BodyType3D` | Behaviour |
|---|---|
| `Static`    | Never moves (ground, walls). |
| `Dynamic`   | Fully simulated — gravity, forces, collisions. |
| `Kinematic` | Moves only by the velocity you set; ignores forces and gravity. |

`world->DestroyBody(id)` removes a body. Box uses `HalfExtents`; Sphere uses `Radius`.

## Stepping and rendering

```cpp
// Each frame:
world->Step(deltaTime);            // advance the simulation

for (PhysicsBodyId3D id : myBodies)
{
    glm::mat4 model = world->GetTransform(id); // translation * rotation
    // multiply by your render scale (e.g. 2 * half-extents for a unit box mesh) and draw
}
```

`GetTransform` returns translation × rotation (no scale — you know each body's size, since you
created it). `GetPosition` / `GetRotation` are available individually. For large frame times, pass
more `collisionSteps` to `Step` (Jolt recommends one step per 1/60 s, rounded up).

## Controlling bodies

```cpp
world->SetGravity({ 0.0f, -9.81f, 0.0f });
glm::vec3 g = world->GetGravity();

world->SetLinearVelocity(id, { 5.0f, 0.0f, 0.0f });
glm::vec3 v = world->GetLinearVelocity(id);

world->ApplyImpulse(id, { 0.0f, 10.0f, 0.0f }); // instant change in momentum, at the center of mass
world->ApplyForce(id, { 0.0f, 50.0f, 0.0f });   // continuous push (per step)
```

## Architecture

`Physics3D` (`include/DingoEngine/Physics/3D/Physics3D.h`) is a backend-agnostic interface —
the 3D counterpart to `Physics2D`. It owns the physics world plus its bodies, all addressed
through opaque `PhysicsBodyId3D` handles; Jolt is one implementation of it (`JoltPhysics3D`,
under `src/DingoEngine/Physics/3D/JoltPhysics/` — the only place Jolt is included).
`Physics3D::Create()` selects the backend, mirroring `Physics2D::Create` and `GraphicsContext::Create`.

Jolt's process-global setup (allocator / `Factory` / `RegisterTypes`) is ref-counted across
worlds, so it is initialised with the first world and torn down with the last.

## See also

- [2D Physics](physics-2d.md) — the Box2D-backed, ECS-integrated 2D system.
- The **DungeonCrawler3D** example (`examples/DungeonCrawler3D/`) — a 3D dungeon-crawler prototype
  driving this world through the ECS (the player, enemies, and walls are `RigidBody3D` entities).
