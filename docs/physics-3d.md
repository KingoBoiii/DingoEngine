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
> exactly like the 2D world: add a `RigidBody3DComponent` plus a collider component (box, sphere,
> capsule or — v0.6.2 — mesh) to a `Transform3DComponent` entity and the `Scene` builds the body,
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

A body is described by a `RigidBodyParams3D`: a body type, a collider shape (box, sphere,
capsule, or one built from a mesh — see [Mesh colliders](#mesh-colliders-v062)), a transform,
and material properties.

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

`world->DestroyBody(id)` removes a body. Box uses `HalfExtents`; Sphere uses `Radius`; Capsule
uses `Radius` plus `HalfHeight`; Mesh and ConvexHull use `Mesh` plus `MeshScale`.

## Mesh colliders (v0.6.2)

Two shapes build a collider from a `Mesh`'s vertex positions and indices (a `Model` submesh's
`MeshData` works too):

| `ColliderShape3D` | Collides with | Body types |
|---|---|---|
| `Mesh` | the triangles themselves — concave level geometry, terrain, ramps, stairs | `Static`, `Kinematic` |
| `ConvexHull` | the smallest convex shape around the vertices — props, rocks, debris | any |

```cpp
RigidBodyParams3D ground(BodyType3D::Static, ColliderShape3D::Mesh);
ground.Mesh      = terrainMesh;
ground.MeshScale = { 2.0f, 1.0f, 2.0f }; // per axis; negative (mirrored) is fine, zero is not
PhysicsBodyId3D groundId = world->CreateBody(ground);

RigidBodyParams3D rock(BodyType3D::Dynamic, ColliderShape3D::ConvexHull);
rock.Mesh     = rockMesh;
rock.Position = { 0.0f, 5.0f, 0.0f };
PhysicsBodyId3D rockId = world->CreateBody(rock);
```

In the ECS, add a `MeshCollider3DComponent` next to the `RigidBody3DComponent`. Without a `Mesh`
of its own it collides as whatever the entity's `MeshRendererComponent` draws, at the transform's
full `Scale` — so the collider matches what is on screen:

```cpp
level.AddComponent<MeshRendererComponent>(MeshRendererComponent(levelMesh));
level.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
level.AddComponent<MeshCollider3DComponent>();                                   // levelMesh's triangles

crate.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
crate.AddComponent<MeshCollider3DComponent>(MeshCollider3DComponent(crateMesh, true)); // convex hull
```

- **Built once, shared, copied.** The first body built from a mesh bakes its shape (a triangle
  BVH or a hull); every later body from the same mesh reuses it, at any scale. The data is copied
  out, so freeing the `Mesh` afterwards is safe — its bodies keep colliding. A baked shape no body
  uses any more is dropped the next time a new one is built, and all of them with the world.
- **A Dynamic `Mesh` becomes a hull.** Jolt cannot derive mass from a triangle soup and does not
  collide mesh against mesh, so a Dynamic body asking for `Mesh` gets the mesh's convex hull, with
  a warning. A Dynamic hull needs volume: a flat mesh (a single quad) is rejected.
- **Triangles are one-sided.** A triangle collides only from the side its counter-clockwise winding
  faces (the side its normal points to). `Renderer3D` draws both sides, so a mesh with inverted
  winding looks right but lets bodies fall through from outside. The engine's own meshes face
  outward; before v0.8.1 `Mesh::CreateSphere` faced inward, so a sphere mesh collider was hollow
  from outside.
- **Thin geometry needs `ContinuousCollision` for fast bodies.** A triangle has no thickness, so a
  body that moves farther than its own radius in one step passes straight through it — a fast
  projectile, even at 60 fps. (A `Scene` already takes one collision step per 1/60 s, so ordinary
  falls stay safe at low frame rates; a standalone world must pass `collisionSteps` itself.) Set
  `RigidBodyParams3D::ContinuousCollision` (ECS: `RigidBody3DComponent::ContinuousCollision`) on such
  bodies and they are swept along their motion each step; the cast only runs on steps where the body
  is actually that fast.
- **Hulls are capped at 256 vertices**; a larger hull is simplified to fit.
- **Failures** — a null mesh, a non-triangle index count, an index out of range, a zero scale axis,
  degenerate geometry — log an error and `CreateBody` returns `k_InvalidBody3D`. In the ECS the
  entity simply gets no body.

Ray casts, shape casts, `OverlapSphere` and character controllers work against mesh colliders
exactly as against primitives.

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
more `collisionSteps` to `Step` (Jolt recommends one step per 1/60 s, rounded up). A `Scene` does
this for you since v0.6.2, up to 4 steps a frame. It also caps a frame at 4/60 s, for its scripts
and physics alike: a longer frame (a stall, a breakpoint) runs the scene slow instead of pushing
bodies through their colliders.

## Controlling bodies

```cpp
world->SetGravity({ 0.0f, -9.81f, 0.0f });
glm::vec3 g = world->GetGravity();

world->SetLinearVelocity(id, { 5.0f, 0.0f, 0.0f });
glm::vec3 v = world->GetLinearVelocity(id);

world->ApplyImpulse(id, { 0.0f, 10.0f, 0.0f }); // instant change in momentum, at the center of mass
world->ApplyForce(id, { 0.0f, 50.0f, 0.0f });   // continuous push (per step)
```

## Ignoring collisions (v0.8)

Two bodies can be told to pass through each other, and a character controller to pass through a
body, without touching any other pair:

```cpp
world->IgnoreCollision(swordId, ownerId);        // symmetric: (ownerId, swordId) is the same pair
world->IsCollisionIgnored(ownerId, swordId);     // true
world->IgnoreCollision(swordId, ownerId, false); // collide again

controller->IgnoreBody(shieldId);                // the capsule neither stops on nor is pushed out of it
controller->IsBodyIgnored(shieldId);             // true
```

- Only contacts are affected: `RayCast`, `ShapeCastSphere` and `OverlapSphere` still report both
  bodies.
- A pair is dropped when either body is destroyed. Changing one wakes both bodies, so it holds from
  the next `Step` even if they were asleep.
- Both are no-ops on an invalid or stale handle, like the other per-body calls.

**In a `Scene` this is automatic for parented kinematic bodies.** A kinematic body whose entity has
a parent — a hitbox socketed to a hand, a shield on a character — ignores the bodies of all its
ancestors (not just its parent), and the character controller of any ancestor ignores it, so it
can't shove the body it hangs off or push its own controller out of its capsule. Nothing else
changes: dynamic and static children, siblings, and unrelated bodies keep colliding, so a dynamic
crate on a parented carrier still rests on it, and 2D physics has no such filter. The scene works
the pairs out at the start of every physics step, so reparenting, `RemoveParent`, a body created or
destroyed at runtime all take effect on the next step; until the first step after `OnStart`
nothing is ignored yet. The scene owns these pairs: calling `IgnoreCollision` on one by hand is
undone on the next step.

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
- The test app's **Mesh Collider Test** (`test/`, run with `--test=collider`) — a triangle-mesh
  terrain bowl, a kinematic mesh lift, and convex-hull pebbles, with pass/fail checks.
- The test app's **Hierarchy Test** (`--test=hierarchy`) — bodies under parents, including the
  checks for the ancestor filter above.
