# Performance

*(v1.0)*

Before v1.0 the 3D renderer paid for every mesh you submitted, seen or not: it transformed every
vertex on the CPU, every frame. From v1.0 the cost follows what is **visible** and what **changes**.
Most of it needs nothing from your game. A few switches, and a few habits, let a game benefit more.

| What the engine does | What your game does |
|---|---|
| [Culls](#culling) meshes, shadow casters, skinned meshes, sprites and circles that the view can't reach. | Nothing. Set `CullDistance` on small far props. |
| [Bakes](#static-geometry) meshes that never move into world space, once. | Set `MeshRendererComponent::Static`. |
| [Instances](#gpu-instancing) a mesh repeated many times: one draw, placed on the GPU. | Share one `Mesh` and one material between the copies. |
| Folds per-object emissive, roughness and specular into one batch ([surface overrides](#surface-overrides)). | Use `SetSurface` instead of a material per entity. |
| Skins crowds from one buffer and can stop [posing animators](#skinned-meshes-and-animation-lod) nobody sees. | Set `AnimatorComponent::CullingMode` on crowds. |

Measure before and after: [Measuring](#measuring) shows the debug window's tabs, and the
[checklist](#before-you-optimise) says what to try first.

The numbers on this page are CPU milliseconds of `Scene::RenderEntities3D` plus
`Renderer3D::EndScene`, on a Release build running on **llvmpipe** (a software rasteriser, which
shares its cores with the CPU work). Each row compares one feature against the build or setting
without it. Treat them as the size of the effect, and measure on your own target.

| Feature | Scene | Before | After |
|---|---|---|---|
| Culling | `mixed`, 10,000 meshes, about 90% out of view | 20.9 ms | 5.8 ms |
| Instancing | `repeat`, 10,000 copies of five props | 11.6 ms | 3.7 ms |
| Static geometry | `mixed`, 10,000 meshes | 4.2 ms | 1.7 ms |

## Culling

`Renderer3D` leaves out every mesh the camera can't see, by its bounds (`Mesh::GetBoundsMin` and
`GetBoundsMax`, placed by the entity's world transform). There is nothing to turn on, and a mesh
that reaches a pixel is never left out. The cull is made in `EndScene`, before any vertex is
transformed or uploaded, so a culled mesh costs its record and a bounds test, and nothing else.

What is culled:

- **Meshes.** `SubmitMesh`, and so every `MeshRendererComponent`. The camera tests the four sides of
  its view. The far plane culls only a mesh whose material depth-tests with `Less`, without a depth
  bias, into a target with depth. The near plane never culls.
- **Shadow casters.** A mesh outside the view still casts into any shadow view its bounds reach, so
  a wall behind you can shadow the room in front. A caster that reaches no shadow view of the scene
  is left out of the shadow atlas ([Shadows](shadows.md)). A mesh past its `CullDistance` casts
  nothing.
- **Skinned meshes.** `SubmitSkinnedMesh` and `SkinnedMeshRendererComponent`, by the rest pose's
  bounds padded on every side by half their longest side, to leave room for the animation. A
  character with every mesh culled from every pass takes nothing of the skinned budget.
- **Sprites and circles.** `Scene::RenderEntities` skips a `SpriteRendererComponent` or
  `CircleRendererComponent` whose bounding square lies outside the 2D camera's view
  ([2D Rendering](rendering-2d.md)).
- **Lights.** A point or spot light whose range can't reach anything in view is skipped before the
  light budget picks ([Lighting](lighting.md#the-light-budget)).

### Distance culling

`CullDistance` (on `MeshRendererComponent` and `SkinnedMeshRendererComponent`, and the last
argument of `SubmitMesh` and `SubmitSkinnedMesh`) drops a mesh that is farther from a perspective
camera than that distance, measured to its bounds. It is neither drawn nor does it cast. 0, the
default, never culls by distance. An orthographic camera ignores it.

```cpp
auto& crate = scene->CreateEntity("Crate").AddComponent<MeshRendererComponent>(
    MeshRendererComponent(crateMesh, glm::vec4(1.0f)));
crate.CullDistance = 60.0f;   // small prop: not worth its vertices past 60 m

renderer.SubmitMesh(rock, transform, color, nullptr, ShadowCasting::On, 1, 120.0f);
```

Use it for small props that a far view needn't spend vertices on. A mesh with a `CullDistance` is
not [`Static`](#static-geometry), and it pops out at that distance, so pick one the eye won't miss.

### Opting out

Culling trusts the mesh's bounds. A vertex shader that moves vertices off them (wind, waves,
outlines) would be culled while its pixels are still in view. Give its material
`FrustumCulled = false`, and the camera never culls its meshes:

```cpp
Material* sway = Material::Create(MaterialParams().SetShader(windShader).SetFrustumCulled(false));
```

This is also the way out for a skinned model whose pose reaches farther than its padded rest
bounds: give it a material with `FrustumCulled` off, at the price that the camera then always draws
it. Shadows come from the renderer's own depth
pass, which never runs the material's shader, so casters are culled either way.

### What is never culled

- A mesh whose transform isn't affine or isn't finite.
- A mesh of a material with `FrustumCulled = false`, by the camera. Its `CullDistance` still
  applies.
- Text, and any 2D quad you submit to `Renderer2D` yourself. Only the scene's sprites and circles
  are culled.
- Directional lights: the first four in submission order light the scene, seen or not.

### Your own culling

`Frustum` and `AABB` (`DingoEngine/Graphics/Frustum.h`) are the engine's own tests, public so a
game's own systems (an AI that only thinks about what the player can see, a light LOD) use the same
maths:

```cpp
Frustum view = Frustum::FromViewProjection(camera.GetViewProjectionMatrix());
AABB box = AABB(mesh->GetBoundsMin(), mesh->GetBoundsMax()).Transformed(transform);
if (view.Intersects(box))
    thinkAboutIt();
```

`Intersects` is false only when the box lies wholly behind one plane, so a box near a corner can
pass while outside. That suits a cull, which can afford a few extra. The other way round, the
transformed box holds the transformed mesh only up to float rounding, so a vertex can lie a few
ulps outside it. Pad the box a little (about `1e-5` of its distance from the origin) when a mesh that
touches the edge of the view must never be dropped; the renderer pads its own boxes that way.

## Static geometry

Walls, floors and props that never move can be **baked**. The renderer transforms their vertices
into world space once, keeps them in GPU buffers between frames, and a frame then costs a cull of
each cell's draws instead of a transform of every vertex.

In a scene, set one flag:

```cpp
auto& wall = entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(boxMesh));
wall.Material = stone;
wall.Static = true;
```

`Scene::RenderEntities3D` bakes every `Static` entity of the scene, and draws them with whatever
lights, shadows and fog the scene has.

### How it is split

- **Cells.** The baked meshes are split by a world grid of 32-unit cells. A mesh belongs to the
  cell its bounds' centre falls in. Each cell has its own vertex and index buffers.
- **Draws.** In a cell, meshes that share a material, a shadow setting and caster groups are one
  draw (more when they outgrow one draw's `MaxVertices` and `MaxIndices`). Each draw is culled
  against the camera and every shadow view by the box around its meshes, as a single mesh is.
- **The picture.** It is the one the same meshes draw without `Static`, pixel for pixel. Where they
  would have been instanced, it is the instanced picture within the [instancing](#gpu-instancing)
  tolerance, since baked meshes are never instanced.
- **The order differs.** A cell draws before the material's other meshes, and its static
  materials join the scene's draw order after the moving meshes'. So two meshes meeting at exactly
  equal depth can resolve the other way.

### Rebaking

A change costs a rebake of the cell the mesh is in, at the next frame. It never costs the rest of
the world.

| You do | What happens |
|---|---|
| Change its mesh, colour, material, surface override or shadow setting, or hot-reload its model | The cell rebakes quietly. |
| Add, destroy, hide (`Visible = false`) or clear `Static` on one | The cell rebakes quietly. |
| **Move** it, or move a parent it hangs from | The cell rebakes, and the engine **warns once**: a mesh that moves shouldn't be `Static`. |

The scene compares each `Static` entity's draw state with what it was baked at, every frame, so the
change is seen without any call from you. That comparison is most of what is left of the scene's
submit cost. A [`StaticBatch`](#drawing-without-a-scene) you hold yourself skips it, since only your
own calls change it.

### What a cell can't hold

These are drawn as if `Static` were false (through `SubmitMesh`, every frame), so they still draw
correctly:

- a mesh with a `CullDistance`;
- a translucent material (unless the mesh is `ShadowsOnly`);
- a skinned-only shader;
- a custom-shader material with `MaterialParams::Instancing`;
- a mesh with more vertices than one draw takes;
- a transform that isn't affine.

Also remember that baked meshes cost **GPU memory**: each one stores its vertices again in world
space, 52 bytes a vertex and 4 bytes an index, even when many of them are copies of one mesh. Copies
of a mesh that never moves are a choice between the two: bake them (cheapest to draw, most memory,
no GPU placement), or leave them for [instancing](#gpu-instancing) (one copy of the vertices, placed
on the GPU each frame).

### Drawing without a scene

Outside the ECS, `StaticBatch` is the same thing by hand. A batch belongs to the renderer that made
it; the meshes and materials it holds must outlive their entries, as they must for `SubmitMesh`.

```cpp
Renderer3D& renderer = Application::Get().GetRenderer3D();
std::shared_ptr<StaticBatch> level =
    renderer.CreateStaticBatch(StaticBatchParams().SetCellSize(32.0f));

std::vector<StaticMeshId> ids;
for (const Placement& p : placements)
    ids.push_back(level->Add(StaticMesh().SetMesh(p.Mesh).SetTransform(p.Transform)
        .SetColor(p.Color).SetMaterial(stone)));

// every frame, between BeginScene and EndScene:
renderer.BeginScene(camera);
renderer.SubmitStaticBatch(*level);
renderer.EndScene();

// later: only the cells these touch rebake, at the next SubmitStaticBatch
level->Update(ids[3], StaticMesh(*level->Get(ids[3])).SetColor(red));
level->Remove(ids[7]);
```

`SubmitStaticBatch` takes no transform: cells are baked in world space, so a batch that moves is
rebuilt with `Update`. `StaticBatchParams::CellSize` is the grid's side, 32 by default; a smaller
cell culls finer and makes more draws. Ids are reused after `Remove` and `Clear`. A batch may
outlive its renderer, drawing nothing. `GetMeshCount()` and `GetCellCount()` tell you what it holds.

## GPU instancing

A mesh drawn many times in one scene (trees, crates, rubble, coins) is drawn **instanced**: every
copy in view in one draw from the mesh's own buffers, each placed on the GPU from a 128-byte record
(its transform, normal matrix, colour and surface index). Nothing is transformed on the CPU.

It is automatic. A mesh is instanced when its batch holds at least
`Renderer3DCapabilities::InstanceThreshold` copies of it (8 by default), or fewer whose vertices
together outgrow one draw, drawn with one material and one shadow setting. The copies are **counted
before culling**, so the camera moving never moves a copy between the CPU and the GPU's transform.
Below the threshold the CPU batch stays cheaper.

```cpp
ApplicationParams params;
params.Renderer3D.Capabilities.Instancing = true;          // the default
params.Renderer3D.Capabilities.InstanceThreshold = 8;      // the default
renderer.SetInstancingEnabled(false);                      // at runtime, to compare
```

What decides whether your copies instance:

- **The same `Mesh`, the same material.** Load a model once and share it. Two `Mesh` objects with
  identical geometry are two meshes and never add up. Likewise a material per entity splits a
  batch: use [surface overrides](#surface-overrides).
- **Lit materials.** The default material and every `CreateLitMaterial` material. A custom material
  is batched on the CPU unless it [opts in](#custom-shaders).
- **Opaque, not skinned, not baked.** Translucent meshes are sorted mesh by mesh and so never
  instance. Skinned meshes and [`Static`](#static-geometry) ones don't either.
- **A budget of draws.** `Renderer3D::k_MaxInstancedDrawsPerFrame` (512) instanced draws a frame,
  lit and shadow together. Groups past it batch on the CPU instead.

Copies culled from the camera but still casting are instanced into the shadow atlas the same way,
every copy once per shadow view in one draw.

### Same picture

The picture matches the CPU batches within the rounding of the two transforms (the GPU and the CPU
round differently): no colour channel off by more than 2/255, on at most 0.1% of the pixels. A scene
where nothing repeats draws exactly as before. Meshes meeting at exactly equal depth can resolve the
other way, since a material's instanced meshes draw after its CPU batch.
`SetInstancingEnabled(false)` batches everything on the CPU again, which is how to check an artefact
is not the instancing.

### Custom shaders

A custom material opts in with `MaterialParams::SetInstancing(true)`. Its vertex stage then reads
the mesh's own vertices in model space and places each copy itself, from the engine include
`<DingoEngine/Instancing.glsl>`:

```cpp
Material* grass = Material::Create(MaterialParams().SetShader(grassShader).SetInstancing(true));
```

```glsl
#include <DingoEngine/Instancing.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

void main()
{
    InstanceRecord instance = GetInstance(uint(gl_InstanceIndex));
    vec3 world = InstanceWorldPosition(instance, a_Position);
    gl_Position = ViewProjection * vec4(world, 1.0);
    v_Normal = InstanceWorldNormal(instance, a_Normal);
    v_Color = instance.Color;
}
```

Such a material draws every mesh given it instanced, however few copies and even with instancing
turned off, since its shader can't read the CPU batches. Its blocks sit at bindings 9 and 10 by
default; a texture or sampler on those bindings makes it draw nothing, so move them with
`DE_INSTANCE_DATA_BINDING` and `DE_INSTANCE_DRAW_BINDING`. Read the details, and the rule about
`gl_InstanceIndex`, in [Scenes & ECS](scenes-and-ecs.md#instanced-custom-shaders-v10).

## Surface overrides

Meshes that differ only in their emissive glow, roughness or specular used to need a material each,
and each material is its own batch and its own draw. A `SurfaceOverride` gives one mesh its own
values while it keeps its lit material:

```cpp
auto& orb = entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(sphere, color));
orb.SetSurface(SurfaceOverride().SetEmissiveColor({ 1.0f, 0.6f, 0.2f }).SetEmissiveStrength(2.4f));

orb.Surface->EmissiveStrength = 2.4f * pulse;   // each frame, for a pulse: no cost
orb.ClearSurface();                             // back to the material's values

SurfaceOverride glow = SurfaceOverride()
    .SetEmissiveColor({ 1.0f, 0.6f, 0.2f }).SetEmissiveStrength(2.4f);
renderer.SubmitMesh(sphere, transform, color, nullptr, ShadowCasting::On, 1, 0.0f, &glow);
```

- **All four or none.** An override replaces `EmissiveColor`, `EmissiveStrength`, `Roughness` and
  `Specular` together. The ones you don't set take `SurfaceOverride`'s defaults.
- **Lit materials only.** A custom shader ignores it. A skinned mesh has none.
- **It composes.** An instanced copy keeps its override, and so does a
  [baked](#static-geometry) mesh (changing a baked mesh's surface rebakes its cell).
- **Cheap to change.** An override is a 32-byte record in a storage buffer, not a pipeline or a
  batch.

Measured in draw calls (not time) with F4, frozen: the *Candlewick* example's first room, 331
meshes, went from 10 draws to 2 when its per-entity materials became overrides.
[Lighting](lighting.md#surface-overrides-v10) has the details.

## Skinned meshes and animation LOD

A skinned character costs a draw per submesh, a joint palette, and the animator that poses it. v1.0
cuts the palette cost, and lets an animator stop posing a character nobody sees.

### The skin palette arena

Every skinned **instance** (a model's submeshes, which share a palette, transform and colour) puts
its transform, colour and joints into one storage buffer, uploaded once a scene. Each draw then
pushes 16 bytes (a push constant) saying where its instance starts, which writes no buffer, so the
instances of a pass share one render pass on Vulkan. A culled instance is neither uploaded nor counted.

What this changes for you is the size of the crowd. `Renderer3DCapabilities::MaxSkinnedInstances`
is the number of skinned instances a frame across every scene the renderer runs, 1024 by default,
at most `Renderer3D::k_MaxSkinnedInstancesLimit` (4096):

```cpp
ApplicationParams params;
params.Renderer3D.Capabilities.MaxSkinnedInstances = 2048;
```

An instance past the budget is dropped **whole**, with a warning
(`Statistics::DroppedSkinnedDraws`), so a character never loses parts. Raise it only when F4's
*Instances* bar is full or something is dropped. A custom skinned shader with the older `SkinData`
block keeps working, but through the old path: at most 256 instances a frame, an upload before each
draw, and on Vulkan every upload ends the render pass. Instanced static draws still upload between
draws, with the same cost. See
[Animation](animation.md#instances-and-the-per-frame-budget).

### Culled animators

A crowd mostly out of view need not be posed every frame. `AnimatorComponent::CullingMode` says what
an animator does while none of its entity's skinned meshes was drawn, into the view or into a
shadow, by the renderers that drew the scene this frame or the last:

```cpp
auto& animator = entity.AddComponent<AnimatorComponent>(AnimatorComponent("Walk"));
animator.CullingMode = AnimatorCullingMode::ReduceWhenCulled;
```

| `AnimatorCullingMode` | While culled |
|---|---|
| `AlwaysAnimate` (default) | Poses every update, seen or not. Nothing changes until you opt in. |
| `PauseWhenCulled` | Keeps the last pose until the entity is drawn again. |
| `ReduceWhenCulled` | Poses every 4th update, offset by entity so a crowd spreads its poses. |

Only **sampling and composing the pose** stop. The animator's time, its fades, one-shot returns,
**events** and **root motion** go on exactly as before, so a culled character's footsteps still
play, a culled walker still walks, and it shows the right pose on the frame after it is drawn
again. That makes the modes safe for gameplay driven by animation.

Know these before you rely on it:

- **One frame late.** Visibility is last frame's, so an entity coming into view shows its stale pose
  for a frame.
- **Children hold still.** Joint sockets, bodies on joints and `Animator::GetSkinningPalette` read
  the last pose evaluated, so a sword parented to a culled hand does not move with the animation.
- **Nothing drawn, nothing culled.** A scene no renderer has drawn lately (stepped headless, or
  hidden behind a full-screen menu) counts every animator as seen, and so does
  `Renderer3D::SetCullingEnabled(false)`.
- **A dropped instance still counts as seen**, since it is in view.

Measured on llvmpipe in a Release build: 128 foxes with 77% of them out of view, `Scene::OnUpdate`
went from 0.51 ms (`AlwaysAnimate`) to 0.22 ms (`PauseWhenCulled`) and 0.31 ms
(`ReduceWhenCulled`). [Animation](animation.md#culled-animators) has the rest, and F7 marks a culled
animator.

## Measuring

Do not guess: the engine shows where the frame goes. The debug window is on by default
(`ApplicationParams::EnableDebugOverlays`); **F3** to **F8** open its tabs.

### F4, the Renderer tab

The tab shows the most recent scene's `Renderer3D::Statistics`. The lines to read for this page:

| Line | What it tells you |
|---|---|
| Draw calls, Meshes | Draws and meshes submitted. Few draws for many meshes is the goal. |
| Culled | Static meshes, skinned meshes and casters left out. 0 in a big world means nothing is culling. |
| Uploaded | Vertices and indices transformed and uploaded on the CPU. Instanced and baked meshes aren't in it. |
| Instanced | Meshes drawn instanced, their draws and shadow draws. |
| Static | Baked meshes, their draws and the cells baked this scene. Cells baked should be 0 once the level has loaded; a count that keeps coming means a `Static` mesh moves or changes. |
| Surfaces | Meshes with a `SurfaceOverride`. |
| Instances (skinning) | Skinned instances against the budget, the joints, the arena's size and what was dropped. |
| Renderer2D: Culled | Sprites and circles left out. |

The tab also has switches, which are what you use to check the work:

- **Cull meshes** off draws everything (and ignores `CullDistance`). The picture must look the same.
  If it doesn't, a mesh's bounds don't hold its pixels: see [Opting out](#opting-out).
- **Freeze culling frustum** keeps culling against the camera as it was. Fly away, and you see what
  that view kept. A mesh popping where it shouldn't shows up here.
- **Instance repeated meshes** off batches everything on the CPU again.
- **Cull 2D entities** is the same for sprites and circles.

The same switches and numbers are in code: `Renderer3D::SetCullingEnabled`, `SetCullingFrozen`,
`SetInstancingEnabled`, `GetStatistics()` (complete after `EndScene`) and
`Renderer2D::SetCullingEnabled`:

```cpp
const Renderer3D::Statistics& stats = Application::Get().GetRenderer3D().GetStatistics();
DE_INFO("draws {} culled {} instanced {} static {}", stats.DrawCalls, stats.CulledMeshes,
    stats.InstancedMeshes, stats.StaticMeshes);
```

### F8, the Profiler tab

It splits the frame in two. The **CPU** section is the main thread's frame: the update (every
layer's `OnUpdate`, which is where the draws are recorded) and the wait for the render thread, then
the render thread's own time. The **GPU passes** section is every GPU timer, such as `Frame`,
`Renderer3D` and `Shadows`. Read it this way:

- A big **Update** is on the CPU side: your scripts, the scene's culling and baking, the transforms.
  This is what culling, `Static` and instancing reduce.
- A big **Render thread** is the time to execute the frame's command list and present it.
- A big **GPU pass** is the GPU's work: fill rate, overdraw, shadow views, shaders. Culling helps
  there too. Baking and instancing save CPU work, and may not move a GPU-bound frame.

Time your own passes with `Renderer::BeginGpuTimer`. [Profiling](profiling.md) has the rest.

### `--perf`

A repeatable number, without touching the debug window. Build the engine from source, and run from
the test app's directory (the path is the Linux one):

```
cd test
../build/bin/Release-linux-x86_64/Dingo-TestFramework/Dingo-TestFramework \
    --test=throughput --throughput=mixed --count=10000 --perf --no-vsync
```

The Throughput Test builds a seeded scene of `--count` entities (`static`, `repeat`, `mixed`,
`skinned` or `shadow`), flies a camera lap, and logs one `[PERF]` line with the means of 600 frames
after 120 warm-up frames:
frame, update, `RenderEntities3D` and `EndScene` milliseconds, draws, vertices and the culled
percentage. Flags that compare a feature on and off: `--instancing=off`, `--static` (sets
`Static` on every mesh) and `--animator-culling=pause|reduce` (with `skinned`). `--frame=N` holds
the camera where frame N puts it, and `--cull-freeze=N` freezes the culling frustum at frame N.

Two examples take `--perf` too, and log the mean frame, update and render times once after a
warm-up: *Candlewick* (`--perf`) and *Marionette* (`--autoplay --perf`).

### Tracy

For a timeline of every thread, build with Tracy: `premake5 --profile <action>` (see
[Profiling](profiling.md#tracy)). The zones to look at are `Scene::RenderEntities3D` and
`Renderer3D::EndScene`, with the culling, `GroupInstances`, `TransformStaticSubmissions`,
`BakeStaticCell` and shadow steps inside, and `DrawSkinnedSubmissions`. Your own code adds zones
with `DE_PROFILE_SCOPE`.

## Before you optimise

1. **Measure a Release build, with VSync off.** A Debug build is far slower, and VSync caps the
   frame time at the display's refresh ([VSync](application-and-layers.md#vsync)).
2. **Find the side.** F8: is the cost in Update (CPU), the render thread, or a GPU pass? The fixes
   differ.
3. **Look at F4's Culled line.** In a big world, a number that stays 0 means nothing is culled: the
   meshes are inside the view, or their material has `FrustumCulled` off.
4. **Mark what never moves `Static`.** Level geometry, props, scenery. Then watch F4's *Static* line
   for rebakes.
5. **Share meshes and materials.** One `Mesh` and one material for the copies of a prop lets them
   instance. Use `SetSurface` instead of a material per entity.
6. **Give small props a `CullDistance`.** Pick a distance the eye won't miss.
7. **Put crowds on `ReduceWhenCulled`** (or `PauseWhenCulled`, if nothing off screen reads their
   pose), and raise `MaxSkinnedInstances` only if F4 shows drops.
8. **Check the picture** with *Cull meshes* off and *Instance repeated meshes* off. It should not
   change.
9. **Measure again.** If a change did not move the number, undo it.

## Limits

- **No occlusion culling.** Only the view and the shadow views cull. A mesh behind a wall is
  submitted and drawn, if it is in view, and the GPU's depth test hides it. There is no depth
  pre-pass either.
- **No LOD meshes.** There is no switching to a simpler mesh with distance. `CullDistance` is on or
  off. (The light budget and animation LOD are separate.)
- **Every submitted mesh is tested every frame.** There is no spatial tree for moving meshes. The
  [static](#static-geometry) world grid is the only spatial structure, and culls its draws, not its
  meshes.
- **A mesh that is neither `Static` nor instanced is still transformed on the CPU** every frame it
  is drawn, at its vertex count.
- **Bounds are boxes.** A mesh's culling shape is its axis-aligned bounding box, placed by its
  transform. A box near a corner of the view can pass while outside.
- **Skinned bounds are an estimate.** The rest pose padded by half its longest side. A pose that
  reaches farther, or hips that travel away without root motion, can be culled while in view: give
  the model a material with `FrustumCulled = false`.
- **`CullDistance` needs a perspective camera**, and a mesh that has one is never `Static`.
- **Static geometry** is world-space copies on the GPU (52 bytes a vertex), is never instanced,
  cannot hold translucent, skinned-only or instancing-shader meshes, and rebakes its cell when a
  mesh moves. The scene keeps a bake for each `Renderer3D` that draws it, at most four, and warns
  when more take turns.
- **Instancing** is for opaque lit meshes (and shaders written for it). No skinned instancing: each
  skinned instance is its own draw, up to 4096 a frame.
- **Surface overrides** are for lit materials and unskinned meshes.
- **Animation LOD** is whole-animator and one frame late. It follows what the renderers drew, not a
  distance, and it cannot tell an animator a gameplay system reads from one nobody does. Sockets and
  bodies on a culled character hold still.
- **Text and hand-submitted 2D quads are never culled.**
- **The numbers here come from a software rasteriser.** They show what the CPU does. The whole
  instanced frame, GPU work included, was slower there than the CPU path, because llvmpipe runs the
  vertex stage on the CPU. Time your target GPU and backend (Vulkan, DirectX 11 and DirectX 12).
