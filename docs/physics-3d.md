# 3D Physics

*(v0.4)*

Alongside the 2D physics ([physics-2d.md](physics-2d.md)), DingoEngine provides a **3D
rigid-body world**, `PhysicsWorld3D`, backed by [Jolt Physics](https://github.com/jrouwe/JoltPhysics).
As with EnTT and Box2D, the backend is an internal detail: **no Jolt type appears in any public
header**, and your game never includes or links Jolt — bodies are referred to through opaque
`PhysicsBodyId3D` handles.

> **Standalone, not ECS-integrated (yet).** The 2D physics is wired into the `Scene`/ECS via
> components. 3D is different: the engine has no 3D scene yet (3D rendering is done directly, as in
> the Breakout3D example), so `PhysicsWorld3D` is a self-contained world you drive yourself —
> create bodies, `Step()` each frame, then read each body's transform to render it however you like.
> When an ECS-integrated 3D scene lands, this world will slot in behind it.

World units are metres (1 unit = 1 metre = 1 Jolt unit).

## Lifecycle

```cpp
#include <DingoEngine.h>
using namespace Dingo;

PhysicsWorld3DParams params;
params.Gravity = { 0.0f, -9.81f, 0.0f };
PhysicsWorld3D* world = PhysicsWorld3D::Create(params);

// ... create bodies, simulate ...

world->Destroy(); // tears down the world (and Jolt itself once the last world is gone)
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

## See also

- [2D Physics](physics-2d.md) — the Box2D-backed, ECS-integrated 2D system.
- The **Physics3D** example (`examples/Physics3D/`) — a tower of boxes you knock down by firing
  spheres, rendering each body at its simulated transform.
