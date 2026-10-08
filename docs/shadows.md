# Shadows

*(v0.9)*

`Renderer3D` draws **cascaded shadow maps** for the sun: the first directional light with
`CastShadows` darkens what stands behind something from it, skinned meshes included. Shadows are
opt-in per light, so a scene whose lights cast nothing renders exactly as before v0.9.

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
warns once. Point and spot shadows are the next step of v0.9.

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
| `NormalBias` | 1.5 | Moves the point a lit surface looks up along its normal, in the cascade's texels. |
| `DebugCascades` | false | Tints the scene red, green, blue and yellow by cascade (also a checkbox in F4). |

**Acne** (a lit surface striped with its own shadow) means too little bias: raise `NormalBias` first,
then `SlopeBias`. **Peter-panning** (a shadow detached from the foot of its caster) means too much.

## How it works

- **Stable cascades.** Each cascade is fitted to the bounding sphere of its slice of the view, so
  turning the camera changes nothing, and snapped to whole texels, so moving it doesn't make edges
  shimmer. Its depth range reaches back toward the light to the bounds of every caster in the scene,
  so a caster outside the view still casts into it.
- **One atlas, one draw per batch.** Every cascade is a tile of one depth texture, and every batch is
  drawn into all of them at once: the shadow pass draws it instanced, one instance per cascade, with
  clip distances at each tile's edges. Skinned meshes are skinned again for it, so a skinned instance
  uploads its joints twice a frame (the skin buffer holds two writes per instance of the budget).
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
```

`Renderer3D` binds `ShadowData`, `u_ShadowAtlas` and `u_ShadowSampler` by name to any material whose
shader declares them (`Material::SetShadowResources`); a shader that doesn't is unaffected. Such a
shader can only be drawn through `Renderer3D`, which provides them.

## Checking it

The test app's **Shadow Test** (`--test=shadow`) has three scenes: `--shadow=sun` (a box, pillars to
40 m and a sphere), `--shadow=acne` (a plane the sun grazes at 80 degrees) and `--shadow=skinned` (the
Fox walking). `--shadow-cascades` tints by cascade and `--shadow-pan` pans the camera slowly. On start it
draws each scene with the sun casting and without and checks by readback that the floor behind a box,
and behind a pillar in a far cascade, goes dark; that the floor in the sun is unchanged to the byte;
that a `ShadowsOnly` box through the ECS shadows the floor without being drawn; that the grazed plane
doesn't shadow itself; and that the Fox casts. The Animation Test's `--anim-shadow` lights its Foxes
with a casting sun.
