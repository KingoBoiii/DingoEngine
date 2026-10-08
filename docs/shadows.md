# Shadows

*(v0.9)*

`Renderer3D` draws **cascaded shadow maps** for the sun and **shadow maps for point and spot
lights**: the first directional light with `CastShadows`, and up to 8 casting point and spot lights,
darken what stands behind something from them, skinned meshes included. Shadows are opt-in per
light, so a scene whose lights cast nothing renders exactly as before v0.9.

## Turning them on

**In the ECS**, mark the sun:

```cpp
DirectionalLightComponent& sun = scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
sun.Direction = { -0.5f, -1.0f, -0.3f };
sun.CastShadows = true;
sun.ShadowStrength = 0.85f; // 0 = no shadow, 1 = none of this light gets through
```

Every `MeshRendererComponent` and `SkinnedMeshRendererComponent` casts by default.

**Without the ECS**, set the same fields on the `DirectionalLight` you submit:

```cpp
DirectionalLight sun;
sun.Direction = { -0.5f, -1.0f, -0.3f };
sun.CastShadows = true;
renderer.SubmitLight(sun);
```

Only the first casting directional light of a scene gets shadows; another one lights unshadowed and
warns once.

## Point and spot lights

Point and spot lights take the same two fields, on `PointLight`, `SpotLight`,
`PointLightComponent` and `SpotLightComponent`:

```cpp
PointLightComponent& lamp = entity.AddComponent<PointLightComponent>(glm::vec3(1.0f, 0.8f, 0.5f), 1.5f, 8.0f);
lamp.CastShadows = true;
```

- **A budget of shadow slots.** `Renderer3DCapabilities::MaxShadowedLocalLights` (8 by default, at
  most 16) casting lights get a shadow each scene. Of the point and spot lights drawn this scene
  (the light budget's choice), the casting ones take the slots in the budget's order, so the
  lights that matter most to the camera keep theirs. A casting light past the slots still lights,
  unshadowed: the renderer warns once and counts it in `Statistics::UnshadowedLights`. A casting
  light whose range reaches no caster takes no slot.
- **Tiles.** A spot light renders one view of its cone into the atlas; a point light renders six,
  the faces of a cube around it (each a little wider than 90 degrees, so filtering at a seam reads
  the right face). A spot light wider than 75 degrees from its axis casts like a point light. The two
  highest-ranked lights get `LocalShadowResolution` tiles (1024), the next four half that and the
  rest a quarter; a point light's faces get half of what a spot light of its rank would. A light
  keeps its size until its rank moves two places, so lights trading ranks don't flicker between
  sizes. When the atlas is full a light falls back to smaller tiles, then lights unshadowed.
- **The budget fade** ([Lighting](lighting.md#the-light-budget)) hands shadow slots over the same
  way it hands lights over: with `LightBudgetFade` above 0, a shadowed light ranked near the first
  casting light without a slot loses its shadow's strength gradually instead of at once.
- **Cost.** Every casting batch is drawn once for every tile: the cascades plus one or six per
  shadowed light. Eight point lights are 48 views of the scene, so keep casting point lights few
  and their ranges short. Spot lights are six times cheaper.

## Who casts

| `ShadowCasting` | Drawn | Casts |
|---|---|---|
| `On` (default) | yes | yes |
| `Off` | yes | no |
| `ShadowsOnly` | no | yes |

`ShadowsOnly` is for geometry the camera must not see but the light must still be stopped by: a
wall cut away so the player can see into a room. (`Visible = false` drops a mesh from both.) Set it on
the component's `Shadows` field, or pass it to `Renderer3D::SubmitMesh` / `SubmitSkinnedMesh` as their
last argument.

Every material casts through the renderer's own depth-only pass: a custom vertex shader's
displacement isn't in its shadow, and a translucent mesh casts a full shadow.

## Is this point in shadow?

A gameplay question, such as "can the brazier see the player", should get the answer the player
sees on screen. A physics ray tests colliders, not drawn meshes, and knows nothing of bias,
filtering, cascades or a light that lost its shadow slot this frame. **Shadow probes** ask the
renderer itself: the GPU runs the lit shader's own shadow lookup at the point and the answer is read
back.

```cpp
// Every frame you care about, with a key per point you track for the same light:
const float seen = scene->GetLightVisibility(brazier, playerChest);          // 1 lit .. 0 in its shadow
const float lit = scene->GetShadowedLightAttenuation(brazier, playerChest);  // falloff x cone x visibility
```

- **Late, never stalling.** Each call asks for the next frame and returns the latest answer for
  that light and key: one to three frames old, and 1 before the first arrives. The answers are read
  back only once a GPU event says the frame that drew them is done, so asking never makes the CPU
  wait for the GPU. The question goes out
  with the scene's next `SubmitLights` that takes them (the `SceneRenderer`'s 3D pass), and is
  answered from that pass's cascades and culling, so a scene that isn't rendered doesn't answer. A
  secondary view drawn first in a frame (a minimap) passes `shadowProbes` false to
  `SceneRenderer::Render(scene, target, false)` or `Scene::SubmitLights(renderer, false)`, leaving
  the questions to the main view.
- **What is drawn.** A light drawn without a shadow (no `CastShadows`, past the shadow slots, or out
  of the light budget) answers 1 at once; `ShadowStrength` scales the answer the way it scales the
  shadow, and the PCF edge reads in between.
- **The sun's shadow follows the camera.** Its cascades cover what the view sees, so a point behind
  the camera, off screen or past `MaxDistance` answers 1 for a directional light, the same as the
  lit shader would draw it there. A spot or point light's shadow covers its whole range wherever
  the camera looks, so AI asking about an off-screen point should ask about a local light, or
  treat the sun's answer there as unknown.
- **A point in the air.** The point has no surface normal to push it off a surface, so a point on
  the floor can read the floor's own depth; ask about a character's chest, not its feet.
- **Budget.** 256 probes a scene (warns once past them). Answers not asked for in 600 frames are
  forgotten.

Without the ECS, ask `Renderer3D` directly, between submitting the light and `EndScene`:

```cpp
renderer.SubmitLight(lamp);
renderer.AddShadowProbe(renderer.GetLastSubmittedLight(), point, key); // any 64-bit key you choose
// ... a few frames later:
if (std::optional<float> lit = renderer.GetShadowProbeResult(key)) { /* 0..1 */ }
```

The probes draw into a 256 x 1 R8 target after the shadow atlas. It is read back only once a GPU
event set behind the copy has signalled, so the CPU never waits for it. `Statistics::ShadowProbes`
counts the probes the GPU answered.

*Candlewick* (`examples/Candlewick/src/Detection.cpp`) is the worked example:
- A warden's cone weighs each of three points on the player by `GetShadowedLightAttenuation` of the
  warden's eye.
- A lit brazier counts only where its own shadow doesn't cover the player's chest.
- The lantern's glow is noticed only while `GetLightVisibility(lantern, warden's eye)` says the
  light reaches the warden.
- The player casts no shadow, since every probe point is inside the player's body.
- `--hide-check` logs every verdict once.

## Tuning

`Renderer3DParams::Shadows` (`Renderer3DShadowSettings`), or `Renderer3D::SetShadowSettings` at
runtime:

| Setting | Default | Meaning |
|---|---|---|
| `AtlasSize` | 4096 | One D32 depth atlas holds every cascade: 64 MB at 4096. A power of two, 2048 to 8192. Made the first time a scene casts. |
| `CascadeCount` | 4 | 1 to 4 for a perspective camera; an orthographic camera gets one cascade fitted to its view. |
| `CascadeResolution` | 1024 | Each cascade's tile, at most `AtlasSize / 2`. |
| `MaxDistance` | 60 | How far along the view shadows reach, in world units; the last cascade fades out before it. |
| `SplitLambda` | 0.75 | How the view splits between cascades: 0 evenly, 1 logarithmically (most detail near the camera). |
| `CascadeBlend` | 0.1 | The part of each cascade, at its far end, that fades into the next, so the change of resolution leaves no seam. |
| `DepthBias`, `SlopeBias` | 4, 2 | Push the stored depth away from the light, in depth steps and times the triangle's slope. |
| `NormalBias` | 1.5 | Moves the point a lit surface looks up along its normal, in the cascade's (or the light tile's) texels. |
| `LocalShadowResolution` | 1024 | The tile of the two highest-ranked shadowed spot lights; see [Point and spot lights](#point-and-spot-lights). A power of two from 128 to `AtlasSize / 2`. |
| `DebugCascades` | false | Tints the scene red, green, blue and yellow by cascade (also a checkbox in F4). |

**Acne** (a lit surface striped with its own shadow) means too little bias: raise `NormalBias` first,
then `SlopeBias`. **Peter-panning** (a shadow detached from the foot of its caster) means too much.

## How it works

- **Stable cascades.** Each cascade is fitted to the bounding sphere of its slice of the view, so
  turning the camera changes nothing, and snapped to whole texels, so moving it doesn't make edges
  shimmer. Its depth range reaches back toward the light to the bounds of every caster in the scene,
  so a caster outside the view still casts into it.
- **One atlas, one draw per batch.** Every cascade and every light's view is a tile of one depth
  texture, and every batch is drawn into all of them at once: the shadow pass draws it instanced, one
  instance per tile, with clip distances at each tile's edges. Skinned meshes are skinned again for it, so a skinned instance
  uploads its joints twice a frame (the skin buffer holds two writes per instance of the budget).
  The clip distances need Vulkan's `shaderClipDistance`; on a GPU without it
  (`GraphicsContext::SupportsClipDistance()` false) no shadows are drawn and Renderer3D warns once.
- **Filtering.** The lit shader reads each cascade through a comparison sampler: 3 x 3 taps of the
  hardware's 2 x 2 comparison, 16 texels in all, kept inside the tile.
- **Cost.** The shadow pass draws every casting batch's vertices once per cascade; per-mesh culling
  is v1.0's. Its GPU time shows as `Shadows` in the F8 Profiler tab. F4's Renderer tab shows the
  cascades, where each ends, the casters and the atlas itself.

## Custom shaders

The lit shader takes its shadows from an engine include, and a custom shader can too:

```glsl
// Before the include, move the three resources to bindings your shader leaves free if 5, 6 and 7 aren't.
#define DE_SHADOW_DATA_BINDING 9
#define DE_SHADOW_ATLAS_BINDING 10
#define DE_SHADOW_SAMPLER_BINDING 11
#include <DingoEngine/Shadows.glsl>

// ...
float shadow = DirectionalShadow(worldPosition, normal); // 1 lit, 0 in shadow
float lamp = LocalLightShadow(i, worldPosition, normal);  // point or spot light i, in CameraData's order
```

`Renderer3D` binds `ShadowData`, `u_ShadowAtlas` and `u_ShadowSampler` by name to any material whose
shader declares them (`Material::SetShadowResources`); a shader that doesn't is unaffected. Such a
shader can only be drawn through `Renderer3D`, which provides them.

## Checking it

The test app's **Shadow Test** (`--test=shadow`) has seven scenes: `--shadow=sun` (a box, pillars to
40 m and a sphere), `--shadow=acne` (a plane the sun grazes at 80 degrees), `--shadow=skinned` (the
Fox walking), `--shadow=spot` (a spot light past a box), `--shadow=point` (a point light among four
pillars), `--shadow=budget` (twelve casting spot lights for eight slots) and `--shadow=probe` (the
sun scene with a row of shadow probes across the box's shadow edge, drawn as spheres from red to
green). `--shadow-cascades` tints
by cascade and `--shadow-pan` pans the camera slowly. On start it draws each scene with its lights
casting and without and checks by readback that the floor behind a box, and behind a pillar in a far
cascade, goes dark; that the floor in the sun is unchanged to the byte; that a `ShadowsOnly` box
through the ECS shadows the floor without being drawn; that the grazed plane doesn't shadow itself;
that the Fox casts; that the spot light's box and each of the point light's pillars cast, on four
cube faces, while the open floor, across the faces' seams and below the light, is unchanged; that
eight of twelve casting lights get a shadow and the other four light unshadowed; that the same
still scene draws the same frame twice; and that the budget fade dims lights at the budget's edge.
Over the next frames it checks that probes read 0 behind the sun's box, the spot light's box, a
point light's pillar and (through `Scene::GetLightVisibility`) a `ShadowsOnly` box, 1 in the open
and at once for a light without a shadow, and in between at the sun's PCF edge.
The Animation Test's `--anim-shadow` lights its Foxes with a casting sun, and the Lighting Test's
`--budget-fade` fades its overbudget scene.
