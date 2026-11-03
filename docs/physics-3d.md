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

`world->DestroyBody(id)` removes a body; on a handle already destroyed it does nothing. Box uses
`HalfExtents`; Sphere uses `Radius`; Capsule uses `Radius` plus `HalfHeight`; Mesh and ConvexHull
use `Mesh` plus `MeshScale`.

A dynamic body weighs 1000 kg per cubic metre of its shape unless `RigidBodyParams3D::Mass` gives it
a mass in kg; its inertia is then scaled from the shape to match.

## Mesh colliders (v0.6.2)

Two shapes build a collider from a `Mesh`'s vertex positions and indices (a `Model` submesh's
`Mesh` works too):

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
  bodies (a `QueryFilter3D` can leave one out).
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

## Collision groups

Bodies in the same group pass through each other, without a pair per combination:

```cpp
const std::uint32_t crowd = world->AllocateCollisionGroup();   // an id nobody has used
world->SetCollisionGroup(a, crowd);                            // a and b now pass through each other
world->SetCollisionGroup(b, crowd);
world->SetCollisionGroup(c, crowd, true);                      // opts in to colliding within the group
world->SetCollisionGroup(a, k_NoCollisionGroup);               // out of the group: collides again

RigidBodyParams3D params;
params.CollisionGroup = crowd;                                 // or set it at creation
params.CollideWithinGroup = false;
```

- Two bodies in the same non-zero group pass through each other unless **both** set
  `collideWithinGroup`; a one-sided opt-in still passes through. Bodies in different groups, and
  bodies in group 0 (`k_NoCollisionGroup`, the default), collide as usual. Group 0 takes a body out of
  its group and clears its opt-in; it collides again from the next `Step`.
- Any `uint32_t` is a group. `AllocateCollisionGroup()` returns an id that no body of this world has
  used through `SetCollisionGroup` or `RigidBodyParams3D`, so it never repeats; it logs an error and
  returns 0 if the ids run out.
- A very high id set directly (for example `0xFFFFFFFF`) counts as used and exhausts
  `AllocateCollisionGroup` for the rest of the world's life. Pick small ids yourself, or use only
  `AllocateCollisionGroup`.
- A group stores nothing per pair: 15 bodies in one group have no `IgnoreCollision` entries
  (`IsCollisionIgnored` stays false).
- Only contacts are affected: `RayCast`, `ShapeCastSphere` and `OverlapSphere` still report grouped
  bodies, and character controllers are unaffected. It works with sensors and mesh colliders.
- It composes with `IgnoreCollision` and a constraint's `CollideConnected = false`: a pair is dropped
  if either mechanism drops it, and removing one leaves the other in force.
- `SetCollisionGroup` wakes the body; `GetCollisionGroup` and `GetCollideWithinGroup` return 0 and
  false for an invalid or stale handle, and the setters do nothing on one. Change groups between
  `Step`s.
- In a `Scene`, `RigidBody3DComponent::CollisionGroup` and `CollideWithinGroup` are read when the body
  is built. A duplicated entity copies them, so the duplicate stays in the same group (and passes
  through its original).

The test for this lives in the Constraint Test (`--test=constraint`, "Collision Group" checks).

## Constraints

A constraint joins two bodies, or pins one body to the world, so they move together under a rule.
`CreateConstraint` returns a `ConstraintId3D`, or `k_InvalidConstraint3D` (0xFFFFFFFF, not 0) with
a warning in the log when the params are rejected:

```cpp
ConstraintParams3D door(ConstraintType3D::Hinge, doorId, k_InvalidBody3D); // B invalid: pinned to the world
door.AnchorA = { -0.5f, 0.0f, 0.0f };    // in the door's local space
door.AnchorB = { 10.0f, 1.0f, 4.0f };    // a world point, because B is invalid
door.AxisA = door.AxisB = { 0.0f, 1.0f, 0.0f };
door.MinLimit = -1.5f;                   // radians, in [-pi, 0]
door.MaxLimit = 0.0f;                    // radians, in [0, pi]
ConstraintId3D hinge = world->CreateConstraint(door);

float angle = world->GetConstraintValue(hinge);  // Hinge angle, Slider displacement, Distance in metres
world->SetConstraintEnabled(hinge, false);       // keeps the handle; IsConstraintEnabled reads it back
world->DestroyConstraint(hinge);
```

| Type | Allows | Reads |
|---|---|---|
| `Fixed` | nothing: a weld | anchors, `AxisA/B` (Y) and `NormalAxisA/B` (X) |
| `Point` | rotation about the anchor (ball and socket) | anchors |
| `Hinge` | rotation about `AxisA/B` | anchors, axes, normals, `MinLimit`/`MaxLimit` |
| `SwingTwist` | a cone of swing plus a twist range about `AxisA/B` | anchors, axes, normals, `SwingLimitY/Z`, `TwistMin/Max` |
| `Distance` | any motion that keeps the anchors `MinLimit`..`MaxLimit` apart | anchors, `MinLimit`/`MaxLimit` |
| `Slider` | translation along `AxisA/B` | anchors, axes, normals, `MinLimit`/`MaxLimit` |

- Anchors and axes are in each body's local space; with `BodyB` invalid, `AnchorB`, `AxisB` and
  `NormalAxisB` are world values. Axes need not be unit length but may not be zero, and a normal is
  made perpendicular to its axis (it may not be parallel to it).
- **Pin the moving body as `BodyA`.** A world pin needs `BodyA` to be Dynamic or Kinematic; a Static
  `BodyA` pinned to the world fails. Two Static bodies fail too.
- A Kinematic body joined to another Kinematic body or to a world pin is accepted, but Jolt never
  solves it: neither side can be moved by the constraint.
- Hinge limits: `MinLimit >= MaxLimit` means unlimited (-pi..pi); otherwise `-pi <= Min <= 0 <= Max <= pi`.
  Slider limits: `MinLimit >= MaxLimit` is unlimited; otherwise `Min <= 0 <= Max`. The zero point is
  where the anchors and normals coincide at creation.
- `Distance` differs from the others: `MinLimit == MaxLimit` is a rigid rod, `Min < Max` a rope that
  may slack or stretch inside the range, and a negative value or `Min > Max` fails. Both zero is
  therefore a rod of length zero, not "unlimited".
- `SwingTwist` is a cone: `SwingLimitY` is the half angle of the swing about `NormalAxis` and
  `SwingLimitZ` the half angle about the third axis (both in [0, pi]); `TwistMin`..`TwistMax` is the
  twist about the axis in [-pi, pi], and Min > Max fails.
- `GetConstraintValue` is the quantity the limits constrain. For a `Slider` or `Hinge` it is the
  position or angle of `BodyB`'s anchor relative to `BodyA`'s, so with a world pin and a moving `BodyA`
  its sign is the opposite of `BodyA`'s own motion. `Fixed`, `Point` and `SwingTwist` return 0.
- `CollideConnected = false` (the default) makes the two bodies ignore each other's contacts. It
  shares the pair with `IgnoreCollision`: the pair is ignored while either the user or any constraint
  asks for it, and `IgnoreCollision(a, b, false)` does not withdraw a constraint's ignore. A disabled
  constraint keeps its ignore; destroying it releases it.
- Only that pair is affected, and queries still report both bodies.
- Destroying either body destroys its constraints. The handle never comes back, and every call
  with a stale or none handle is a no-op (the getters return false, none or 0).
- Creating or changing a constraint wakes both bodies. `Shutdown` removes constraints before bodies.

In a `Scene`, a `Constraint3DComponent` on the entity that is `BodyA` builds the constraint; see
[Scenes & ECS](scenes-and-ecs.md).

### Motors

`Hinge` and `SwingTwist` joints can drive themselves. `SetMotor(constraint, axis, mode, params)` picks
the axis (`MotorAxis3D::Hinge` for a hinge; `Swing` or `Twist` for a swing-twist), the mode and a
`MotorParams3D { Stiffness, Damping, MaxTorque }`.

- `MotorMode3D::Off` (the default) drives nothing.
- `Position` is a spring toward the target set with `SetMotorTargetRotation`: `Stiffness` pulls, `Damping`
  resists velocity, and the torque is clamped to `MaxTorque`.
- `Velocity` spins toward the target set with `SetMotorTargetVelocity` (rad/s, in world axes of the
  joint). With `Damping == 0` it is Jolt's pure velocity motor; with `Damping > 0` it is a damper
  toward the target velocity with `Stiffness` ignored.
- `MaxTorque == 0` turns the motor off whatever the mode. Units map directly to Jolt's: `Stiffness`
  is torque per radian and `Damping` torque per rad/s, both applied to the joint, so the result
  depends on the bodies' inertia (it is not mass-independent).
- The target rotation is in body space: `R_B = R_A * q`. For a `Hinge` only the rotation about the
  axis counts; identity is the rest pose, and the target angle matches `GetConstraintValue` only when
  both bodies were created with equal rotations. With a world pin and a moving `BodyA` the sign
  is flipped, as for `GetConstraintValue`.
- `GetMotorParams`, `GetMotorMode` read the settings back; `GetMotorTorque(constraint, axis)` is the
  magnitude of the torque applied last step, and 0 when the motor is off, both bodies are asleep or the
  handle is stale. No solver torque is applied while both bodies sleep, so a motor holding a pose
  against gravity reads 0 once it sleeps.
- A change wakes both bodies; setting the same values again does nothing. A stale handle, a
  negative or non-finite value, or an axis the joint doesn't have is ignored (the last logs a warning).

In a `Scene` the same settings are fields on `Constraint3DComponent` and are written every step; see
[Scenes & ECS](scenes-and-ecs.md). The test app's Constraint Test (`--test=constraint`) has a motor arm
and a panel to drive it.

### Breaking

A joint can let go when it is hit hard enough. Set `ConstraintParams3D::BreakForce` (newtons) and
`BreakTorque` (newton-metres) when you create it, or later with `SetConstraintBreakLimits`; `0` (the
default) never breaks. At the end of each `Step`, a constraint whose reaction exceeds a non-zero limit is
destroyed exactly as `DestroyConstraint` would, so its handle goes stale and the bodies wake. Ids are
never reused, so a constraint created afterwards has a higher id.

`ConsumeBrokenConstraints(out)` appends what broke since the last call, in the order it broke (constraint
id order within one step), and clears the list. Each `ConstraintBreak3D` is a snapshot of the handle, both
bodies, the `UserData` and the force and torque at the break; the bodies may be gone by the time you read it.
The world keeps at most 4096 undrained entries and drops the oldest with one warning.

`GetConstraintReaction(constraint, force, torque)` reads the same numbers for a live joint, and
`GetConstraintBreakLimits` reads the limits back. What counts as force and torque, from the solver's
accumulated impulse:

| Type | Force | Torque |
|---|---|---|
| Fixed | position impulse | rotation impulse |
| Point | position impulse | none |
| Hinge | position impulse | rotation impulse plus rotation-limit impulse |
| SwingTwist | position impulse | largest of swing Y, swing Z and twist impulse |
| Distance | position impulse (limits included) | none |
| Slider | position impulse plus position-limit impulse | rotation impulse |

Motor impulses never count, so a strong motor does not break its own joint. `BreakTorque` on a Point or
Distance joint is stored and has no effect.

- The reaction is the last collision sub-step's impulse scaled to a rate (`|λ| · collisionSteps / dt`, as
  `GetMotorTorque` does). Jolt keeps no earlier sub-step, so with `collisionSteps` above 1 a spike in an
  earlier sub-step goes unseen.
- A constraint the last `Step` did not solve reads 0 and cannot break: both bodies were asleep when the step
  began, or it is disabled. Nothing loads a sleeping joint. A pair woken during a step (a hit, a destroyed
  neighbour) or between steps (an impulse from a script) reads 0 until the next step solves it, which can
  delay a break by one step.
  `SetConstraintBreakLimits` does not wake bodies, so lowering a limit below a sleeping joint's load acts
  once something wakes it. A hung body at rest sleeps after about half a second and then reads 0.
- A negative, NaN or infinite limit is ignored by `SetConstraintBreakLimits` and treated as 0 with a
  warning by `CreateConstraint`.

## Sensors, query filters and body lookup

A **sensor** is a trigger volume: `RigidBodyParams3D::IsSensor` (or `RigidBody3DComponent::IsSensor`).
Nothing collides with it and character controllers walk through it. Ask what is inside:

```cpp
std::vector<PhysicsBodyId3D> bodies;
world->GetSensorOverlaps(sensorId, bodies);     // every non-sensor body touching it, once each
controller->IsOverlapping(sensorId);            // a character is not a body, so ask it directly

std::vector<Entity> inside;
scene->GetSensorOverlaps(triggerEntity, inside); // both: entities with a body, then with a controller
```

It is a query, not an event: call it when you need to know (each frame, for a door that opens while
someone stands in it). A Mesh-shaped sensor doesn't find Mesh colliders, and finds only what crosses its surface: a body
wholly inside a closed Mesh sensor isn't listed, so give a volume a box, sphere, capsule or convex
hull shape.

**Query filters.** `RayCast`, `ShapeCastSphere` and `OverlapSphere` take a `QueryFilter3D`:

```cpp
QueryFilter3D filter = QueryFilter3D().Ignore(shooterBody).SetLayers(LAYER_WORLD | LAYER_ENEMY);
world->RayCast(ray, 50.0f, hit, filter);
```

- Every query skips sensors unless the filter `SetIncludeSensors(true)`; the overloads without a
  filter use the defaults. No body could be a sensor before, so existing calls behave as they did.
- `RigidBodyParams3D::QueryLayers` (and the component's) is a bit mask of the layers a body is in,
  `1` by default. A query hits a body when `filter.Layers & body.QueryLayers` isn't zero. Layers
  filter queries only; contacts don't read them.

**Body to entity.** `Scene::GetEntityFromBody3D(hit.Body)` gives back the entity of a ray hit, or a
null `Entity` for a body the scene didn't make or one already destroyed. The scene keeps the entity
in the body's `UserData` (`RigidBodyParams3D::UserData`, `Physics3D::GetUserData/SetUserData`), so
leave a scene body's user data alone.

## Character controllers and each other

Two character controllers block each other: neither walks through the other, and neither shoves the
other aside, each stops at the other's capsule. `CharacterControllerParams3D::CollideWithCharacters`
(and `CharacterController3DComponent::CollideWithCharacters`) is on by default; a controller that
turns it off neither blocks nor is blocked by the others. A controller is still not a body, so ray,
shape and overlap queries don't find it.

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
- The test app's **Constraint Test** (`--test=constraint`) — every joint type, limits,
  collide-connected, destruction and rejected params, then the same through a `Scene`; also the
  collision group checks.
- [Ragdolls](animation.md#ragdolls) — `RagdollComponent` builds a skinned character's bodies and
  joints from its skeleton; the test app's **Ragdoll Test** (`--test=ragdoll`) checks it.
- The test app's **Hierarchy Test** (`--test=hierarchy`) — bodies under parents, including the
  checks for the ancestor filter above.

## Limits

- The step is the frame's delta, cut to 4/60 s, in `ceil(dt * 60 - 0.1)` collision steps (at most 4)
  in a `Scene`, unless the scene has `SceneParams::FixedTick`, which steps it once per fixed tick and
  draws bodies between ticks ([Scenes & ECS](scenes-and-ecs.md#fixed-simulation-tick-v11)).
- A `Mesh` collider is one-sided, can't be Dynamic (a Dynamic body gets the mesh's convex hull, with
  a warning), and a hull is capped at 256 vertices.
- A thin triangle needs `ContinuousCollision` for fast bodies.
- Collision groups filter contacts only, are per world, and cannot be changed while a `Step` runs.
- A Mesh-shaped sensor finds only what crosses its surface and doesn't find Mesh colliders.
- A character controller is not a body: ray, shape and overlap queries don't find it. Controllers
  are rebuilt on `OnStart`, so a script never keeps a `CharacterController3D*` across
  `OnStop`/`OnStart`.
- A collider is baked into its body when the body is built (`OnStart`, `CreateRigidBody`, or
  `SetActive(true)`); editing a component on a live body changes nothing until then.
- A constraint is limited to what Jolt solves: a very stiff chain of light bodies against a heavy
  one stretches, and a joint between bodies far from its anchors is violated until the solver
  catches up. `Distance` with `Min == Max == 0` is a rod of length zero, not unlimited.
- A `SwingTwist` velocity motor maps the world target velocity into the joint's frame, which is exact
  only near the rest pose. A `Position` target outside a limit pushes against the limit. A `Swing`
  `GetMotorTorque` is the larger of the two swing axes.
- A Scene's kinematic-ancestor filter is recomputed every step, so a hand-made `IgnoreCollision` on
  such a pair is undone.
