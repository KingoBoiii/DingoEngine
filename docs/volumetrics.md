# Volumetric fog

Status: v1.3. Global froxel fog: height fog you can see light through, with sun shafts, on lit and
PBR meshes, translucent meshes, particles and the sky. Local `FogVolume`s (boxes and ellipsoids,
froxel or ray-marched), point and spot light scattering, scrolling noise and a debug view sit on top
of it.

`Renderer3D` splits the view frustum into a grid of cells ("froxels": 160 x 90 x 64 by default, the
depth slices spaced exponentially). Two compute passes fill it before the scene is drawn: one writes
the light each cell scatters towards the camera and how much it blocks, the other adds those up
along every ray. A shader then reads one value for its pixel's depth, so what you see is
`colour * transmittance + in-scatter`, however many surfaces and particles are in the way. The
sun's shadow cascades cut the in-scatter, which is what makes a shaft.

## Turning it on

It is a capability, off by default. It uses no GPU memory and costs nothing until first used; once
used, the grid stays allocated while the renderer lives, and the dispatches stop when it is off:

```cpp
Renderer3DParams params;
params.Capabilities.Volumetrics = true;
params.Capabilities.VolumetricGrid = glm::uvec3(160, 90, 64);  // the default
```

`SetVolumetricsEnabled(bool)` and `SetVolumetricGrid(glm::uvec3)` change both at runtime. The grid is
clamped to 8..256 wide and high and 8..128 deep, and changing it drops the temporal history. A scene
then needs a `VolumetricFog`:

```cpp
VolumetricFog fog;
fog.Density = 0.03f;
fog.Anisotropy = 0.6f;
renderer.SetVolumetricFog(fog);   // scene-scoped, like SetFog: call it before EndScene
// renderer.ClearVolumetricFog();
```

In the ECS, give the camera's `FogComponent` a `std::optional<VolumetricFog> Volumetric`.
`SubmitFog` passes it on, and the `UseClearColor` flag still only affects the distance fog's colour.
A scene with the capability on and a volumetric fog set **replaces** its distance fog (`Fog` is
ignored for that scene); a scene with only distance fog keeps it. A perspective camera only: an
orthographic camera's scene is drawn without the volume.

## VolumetricFog

Colours are in display space, like the lit shader's fog: a white medium under an ambient light of 0.5
scatters 0.5.

| Field | Default | What it does |
|---|---|---|
| `Density` | 0.02 | Extinction per world unit at `BaseHeight` and below. Same unit as `Fog::Density` in `Exponential` mode. 0 or more. |
| `BaseHeight` | 0 | World height above which the density falls off. |
| `HeightFalloff` | 0 | Per world unit above `BaseHeight`, density is multiplied by `e^(-HeightFalloff h)`. 0 is a uniform medium. 0 or more. |
| `Albedo` | (1, 1, 1) | The share of the extinguished light that scatters, per channel, clamped to 0..1. |
| `Anisotropy` | 0.2 | Henyey-Greenstein g, clamped to -0.95..0.95. Above 0 scatters towards where the light travels, so you see more of it looking at the sun. |
| `AmbientIntensity` | 1 | Scales the scene's ambient light in the medium: the flat ambient colour, or the sky's under an environment that lights the scene (below). |
| `LightIntensity` | 1 | Scales the directional lights in the medium. |
| `ScatterTint` | (1, 1, 1) | Multiplies the in-scatter per channel, apart from `Albedo` and the light's colour: a blue haze under a violet sun. Finite and 0 or more, **not clamped**, so a value above 1 brightens. It never changes extinction, so the transmittance is the same. |
| `OutsideShadowLight` | 1 | How much of the sun reaches a sample no shadow cascade covers, 0 (dark: only air the shadow map proves lit scatters) to 1 (lit, today's behaviour). The light's `ShadowStrength` scales it (`mix(1, OutsideShadowLight, ShadowStrength)`), so outside air is never darker than full shadow. Clamped to 0..1. |
| `NearDistance` | 0.5 | View depth where the volume starts. Above 0. |
| `MaxDistance` | 64 | View depth where it ends; surfaces past it take the fog at `MaxDistance`. Above `NearDistance`. |
| `Temporal` | true | Blend each frame with the previous one to smooth the sampling noise. |

`SetVolumetricFog` returns `false` and warns once for a non-finite value, a negative `Density`,
`HeightFalloff`, intensity or `ScatterTint`, `NearDistance <= 0`, or `MaxDistance <= NearDistance`. It
clamps `Albedo`, `Anisotropy` and `OutsideShadowLight`. It takes effect only with the capability on.

- **Sun shafts** need a directional light that casts shadows ([Shadows](shadows.md)): the medium in
  its shadow gets no sun. A light that casts no shadow, or a scene with shadows off, is unshadowed,
  and the ambient in-scatter is separate from it either way.
- **Past the last cascade** the shadow of the last cascade fades to `OutsideShadowLight` over the
  cascade's tail (the width `CascadeBlend` gives the surface shadows), so there is no edge where the
  map ends. With 1 (the default) the far air is lit; with 0 it goes dark, so a dusty hall shows
  haze only where the shadow map proves light reaches.
- **Temporal** blends 10 % of the current frame into the previous one, with the sample position
  jittered inside each slice by the same amount for every cell, so the glass shows no dither. Only
  the first temporal scene of a frame (`Renderer::GetFrameIndex`) reads and writes the history; any
  other scene that frame is drawn without it. The history is used only if the previous frame was
  also temporal and the grid, `NearDistance` and `MaxDistance` are unchanged.
- **After a camera cut**, call `Renderer3D::ResetVolumetricHistory()` so the new view doesn't blend
  with the old one. In the ECS, set `CameraComponent::ResetVolumetricHistory = true` for a frame:
  the `SceneRenderer` calls it for the camera it renders and clears the flag.

`Renderer3D::Statistics` has `Volumetrics` (the last scene was drawn with the volume),
`VolumetricTemporal` and `VolumetricDispatches`. The GPU timer `Volumetrics` ([Profiling](profiling.md))
covers both passes; the F9 Scene tab shows a `FogComponent`'s volumetric fields read-only.
`VolumetricFog::LocalLightIntensity` (default 1) and `MaxInScatter` (default 0, no cap) are the global
settings the local volumes below build on.

## Local volumes

A `FogVolume` is a box or an ellipsoid of medium that adds to the scene's fog inside it. Submit one
per scene with `Renderer3D::SubmitFogVolume(volume, transform)` (scene-scoped, like `SetVolumetricFog`;
it returns `false` and warns once for an invalid value), or put a `FogVolumeComponent` on an entity:
`Scene::SubmitLights` submits every enabled one in entity order with the entity's world transform.
The shape is `HalfExtents` scaled by the transform, so a volume's world size is `HalfExtents` times
the transform's scale; an `InactiveComponent` or `Enabled = false` takes it out.

```cpp
FogVolume hall;
hall.Shape = FogVolumeShape::Box;
hall.HalfExtents = glm::vec3(8.0f, 3.0f, 12.0f);
hall.Density = 0.04f;
hall.NoiseMin = 0.4f;                       // scrolling noise: density x 0.4..1.6
hall.NoiseMax = 1.6f;
hall.NoiseVelocity = glm::vec3(0.2f, 0.0f, 0.0f);
renderer.SetFogVolumeTime(sceneTime);       // the Scene advances this clock for you
renderer.SubmitFogVolume(hall, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f)));
```

| Field | Default | What it does |
|---|---|---|
| `Shape`, `Mode`, `HalfExtents` | Box, Froxel, 1 | The shape, how it is drawn (below) and its half sizes before the transform's scale. |
| `Density`, `Albedo`, `Anisotropy`, `ScatterTint`, `OutsideShadowLight`, `AmbientIntensity`, `LightIntensity` | 0.05, 1, 0.2, 1, 1, 1, 1 | As on `VolumetricFog`. Overlapping media add their extinction and in-scatter; the phase function's `g` is the extinction-weighted mean. |
| `LocalLightIntensity` | 1 | Scales the point and spot lights in this volume, on top of `VolumetricFog::LocalLightIntensity` and each light's `VolumetricIntensity`. |
| `EdgeFade` | 0.5 | World units over which the density falls to 0 towards the surface of the shape. 0 is a hard edge. |
| `HeightFadeStart`, `HeightFadeEnd` | 0, 0 | Metres above the volume's bottom along its local up: full density up to the start, 0 at the end. Off when End is not past Start. |
| `HeightBandBottom`, `HeightBandTop`, `HeightBandMultiplier`, `HeightBandSoftness` | 0, 0, 1, 0.5 | A band, in metres above the volume's bottom, where the density is multiplied, easing over `HeightBandSoftness` at each edge. Off when Top is not past Bottom or the multiplier is 1. |
| `NearFadeStart/End/Keep`, `FarFadeStart/End/Keep` | 0/0/0, 0/0/1 | Thins the in-scatter with the distance from the camera (linear). Keep is the share left at the near start and closer (near) or from the far end (far). **Never the density**: transmittance and the dimming of what is behind the volume stay the same. |
| `MaxInScatter` | 0 | Raymarch mode: the most one pixel's in-scatter may reach, per channel (the strongest channel is scaled down to it). 0 is no cap. Froxel volumes share `VolumetricFog::MaxInScatter` instead. |
| `RaymarchSteps` | 16 | Raymarch mode: samples along the ray, 1..64. |
| `NoiseScale`, `NoiseVelocity`, `NoiseMin`, `NoiseMax` | 4, 0, 1, 1 | Scrolling 3D noise: the world size of one tile, the drift per second, and the range the density is multiplied into. Off when Min equals Max. |

- **A box alone is enough** (H15): the froxel pass runs with a global `VolumetricFog` *or* a froxel
  volume. Only a global `VolumetricFog` replaces the distance `Fog`; a scene with froxel volumes and no
  global fog keeps its distance fog and a froxel range of 0.5 to 64.
- **Budget.** `Capabilities.MaxFogVolumes` (default 8, at most `Renderer3D::k_MaxFogVolumes` = 16) is the most
  a scene draws, at most `k_MaxRaymarchVolumes` = 4 of them Raymarch. Volumes outside the view (and
  froxel ones past the froxel range) are culled first, then the nearest win; the rest are counted in
  `Statistics::FogVolumesDropped` and warn once.
- **Local lights.** Point and spot lights scatter in the medium with the lit shader's falloff and one
  shadow-map tap, scaled by `VolumetricFog::LocalLightIntensity`, the volume's `LocalLightIntensity` and the
  light's own `VolumetricIntensity` (on `DirectionalLight`, `PointLight`, `SpotLight` and their
  components; 0 keeps a light out of the haze while it still lights surfaces) (H13). Set a global
  `LocalLightIntensity` of 0 for directional-only. Raymarch volumes ignore local lights.
- **Noise and the clock** (H5). `Renderer3D::SetFogVolumeTime(seconds)` is the caller's clock; the
  `Scene` advances its own from `OnUpdate`'s delta time, so a game that scales `deltaTime` scales the
  drift. The temporal history is reprojected by the drift of the densest noisy volume, so the
  history keeps up at any timescale. While a noisy volume with a non-zero `NoiseVelocity` is kept, a
  jump back or of more than 0.5 s drops the history; the clock is not looked at otherwise.
- `FogVolumeComponent` is a built-in component: duplicated with its entity and shown read-only in
  the F9 Scene tab.

## Raymarch volumes

`FogVolume::Mode = FogVolumeMode::Raymarch` draws the volume per pixel instead of through the grid,
so a thin shaft keeps its edges and truss shadows at any distance. After the opaque meshes and the
sky are drawn (and after ambient occlusion, so AO does not darken the haze), a compute pass marches
each Raymarch volume's ray, at half or quarter resolution (`SetVolumeRaymarchResolution`, global,
default half; changing it at run time keeps the textures), and a fullscreen pass applies the result with
`colour * T + L`, upsampled by depth so silhouettes get no halo.

- **Needs the post chain.** The march reads the post chain's copy of the scene depth, so a scene
  drawn without `PostProcessStack` (or to another target) draws the volume as a froxel one instead,
  logs one warning and counts it in `Statistics::RaymarchFallbacks`. If the raymarch shader fails to
  compile, the volumes are dropped with a logged error, with no froxel fallback.
- **With the froxel fog** (H10): the result is `(c * Tf + Lf) * Tr + Tf(entry) * Lr`: the global
  fog in front of the box also takes the box's transmittance, a known small error. Several Raymarch
  volumes are combined in order of entry.
- **Translucent meshes and particles are drawn after the composite and do not see a Raymarch
  volume.** Use a Froxel volume where glass or smoke has to sit in the haze. A custom shader needs no
  change: the composite applies on top of its output, including its own distance fog.
- The march samples at a frame-stable per-pixel jitter (no history), so a still scene gives a still
  picture; at quarter resolution a regular pattern can show on smooth haze.
- **No near-fade clipping of the ray start**: the march starts at the volume's entry, and the near
  and far fades only weight each sample's in-scatter.
- Cost: about 0.2 to 0.4 ms at half resolution with 16 steps on the RX 7900 XTX (an estimate; the
  mid-range figure for H9 is still open). The timer is `VolumeRaymarch`.
- Each post scene target (one per `Begin` of a frame) owns its own pair of low-resolution textures, so
  two `Begin`s of different sizes in a frame do not resize each other's. A pair is resized in place when
  its target changes size or the resolution changes, so its ids stay stable, and is freed after 120 frames
  without use.

`Renderer3D::Statistics` adds `FogVolumes` (kept), `RaymarchVolumes`, `FogVolumesCulled`,
`FogVolumesDropped`, `RaymarchFallbacks` and `RaymarchDispatches`.

## Debug view

`Renderer3D::SetVolumetricDebugView(VolumetricDebugView::ShadowSamples)` paints the directional
light's contribution in false colour, in the froxel grid and in the Raymarch march alike: **green** where the
shadow map proves the sample lit, **red** in shadow, **blue** past the last cascade (scaled by how
far past), **yellow** when the sun casts no shadow. The brightness is the in-scatter the sample would have, so a
uniform medium reads as a flat colour. It is persistent, and the normal picture returns, bit for bit,
with `VolumetricDebugView::None`.

The F9 panel's **Renderer3D volumetrics** block (renderer stats section, after the shadow atlas) shows the counters above,
the on/off switch, the grid, the raymarch resolution and the debug view as toggles, the
`Volumetrics` and `VolumeRaymarch` timers, and a **froxel slice**: pick the integrated volume, the
scatter volume or its alpha (transmittance, or extinction), a slice and an exposure and it is shown as
an image. In code, `Internal::Renderer3DAccess::RequestVolumetricSlice` asks the next dispatching
scene for one, copied into a 2D texture of the grid's width and height after the two fog passes.

## What gets fogged

| Drawn by | How |
|---|---|
| Lit and [PBR](pbr.md) materials, translucent or not, static, skinned and instanced | The fragment shader samples the volume at the surface. `Additive` fogs towards black and `Multiply` towards white, like distance fog. |
| [Particles](particles.md) | Every particle fragment, alpha and additive, before it is premultiplied. |
| The [sky](environment.md) | The sky takes the fog to `MaxDistance` (the last slice), so the horizon meets the haze. |
| Custom shaders | Include the contract below. |
| Raymarch volumes | The composite after the opaque meshes and the sky; translucent meshes and particles are not hazed by them (see Raymarch volumes). |

Under a procedural sky with `TintSun`, the main directional light's colour is already multiplied by the
atmosphere's transmittance, so the shafts and the fog's in-scatter redden at sunset and fade out when the sun
sets (see [Procedural sky](environment.md#procedural-sky)).

With an [environment](environment.md#image-based-lighting) set and `Lighting` on, the medium's ambient
light is the sky's mean radiance (band 0 of its irradiance) times `Intensity x LightingIntensity`, in
place of the flat ambient colour, so a blue noon sky and a red dusk light the haze differently. It is
encoded to display space like the PBR surfaces' ambient: a uniform sky of radiance 1 scatters as a white
ambient colour of 1. Froxel and Raymarch volumes both take it.

[Ambient occlusion](post-processing.md#ambient-occlusion) fades under the volume by the transmittance
at each pixel's depth, so it darkens the surface behind the haze (`colour x T`) and leaves the
in-scattered light alone: a mesh no light reaches through dense fog keeps no AO.

## Custom shaders

With the volume active, `FogColor` and `FogParams` are zeroed, so a custom shader that does its own
distance fog from them gets none unless it includes `Volumetrics.glsl` and samples the volume.

A shader of your own can sample the same volume. The camera block lives in its own include, so a
shader that only needs the view can take it without the scene's varyings:

```glsl
#include <DingoEngine/Renderer3DCamera.glsl>   // binding 0: the CameraData block, VolumeParams and VolumeGrid included
// Move the volume if 12 and 13 aren't free:
// #define DE_VOLUMETRIC_FOG_BINDING 14
// #define DE_VOLUMETRIC_FOG_SAMPLER_BINDING 15
// or reuse a clamp sampler you already declare: #define DE_VOLUMETRIC_FOG_SAMPLER u_MySampler
#include <DingoEngine/Volumetrics.glsl>

vec3 fogged(vec3 color, vec3 worldPosition, int target)   // target: 0 opaque/alpha, 1 additive, 2 multiply
{
	if (VolumeParams.x > 0.5)                              // uniform: an unfogged scene fetches nothing
	{
		vec4 fog = DE_SampleVolumetricFog(ViewProjection, VolumeParams, VolumeGrid, worldPosition);
		return DE_ApplyVolumetricFog(color, fog, target);  // color * fog.a + L
	}
	return color;
}
```

- `Renderer3D` binds the texture by the name `u_VolumetricFog` (and the sampler by
  `u_VolumetricFogSampler`, only if the shader declares it) to any material whose shader declares
  it, as it does for the shadow resources. Defaults: texture 12, sampler 13. The lit shader uses 12
  and 13; the PBR shader's texture is 22, the particle shader's 7 and the sky's 3, each aliasing a
  sampler it already has.
- Branch on `VolumeParams.x` and put the sample inside the branch: the fetch is `textureLod`, so it
  needs no derivatives, but an unfogged scene should not pay for it.
- **`CameraData` is append-only.** The first 96 bytes are what custom shaders rely on, and the block
  only grows by appending (it is now 1888 bytes; [Lighting](lighting.md#custom-material-shaders)).
  `Renderer3DCamera.glsl` mirrors it byte for byte: mirror the whole block or stop after any member.
- The batched `Renderer3D_MeshVertex.glsl` needs `a_ObjectIndex` and an object-data buffer, so a
  custom shader that wants the world position computes it in a vertex stage of its own, as the
  Froxel Fog Test's custom shader (check 12) does.

## Cost and memory

- At the default grid, two 160 x 90 x 64 RGBA16F textures (the scatter history pair) and one for the
  integrated result: 7 MiB each, 21 MiB in all, allocated at first use and kept while the renderer
  lives.
- On the RX 7900 XTX the `Volumetrics` timer reads about 0.13 to 0.17 ms a frame in the Froxel Fog
  Test, which runs four scenes a frame, so a single scene is less. The test logs one line with the
  figure after its checks (`Froxel cost: ...`).
- Per pixel: one trilinear fetch and about 15 arithmetic instructions in each shader that applies it.
- A smaller grid is cheaper and blurrier: thin shadows (a truss, a fence) are sharper at 160 x 90
  than at 64 x 36.

## Limits

- Ambient occlusion fades by the froxel volume of the last volumetric scene drawn into the post
  target before AO is applied; with several such scenes in one `Begin`, the earlier ones fade by it too.
- An orthographic camera gets no volume.
- One temporal scene a frame: a second view that frame, such as a minimap or a mirror, draws
  without the history.
- Only the first casting directional light shadows the shafts, as for surfaces.
- The temporal reprojection under a drifting noisy volume is less accurate with strong anisotropy:
  the worst error measured is 0.012 at `Anisotropy` other than 0, against 0.0078 for `g = 0`.
- Raymarch volumes ignore point and spot lights, do no near-fade clipping of the ray start (the fades
  weight samples only), are not seen by translucent meshes and particles, and the froxel fog in front
  of one also takes its transmittance (see Raymarch volumes).
- A Raymarch volume needs the post chain; a failed raymarch shader compile drops the volumes with a
  logged error instead of falling back to froxel.
- When one post `Begin` draws several Renderer3D scenes, ambient occlusion is applied at the end of the
  first Raymarch scene, so later scenes in that `Begin` (a viewmodel pass, say) get no AO.
- The depth-aware upsample can leave a faint pattern on smooth haze at quarter resolution.
- A very thin bright haze, where the transmittance rounds to 1 in the RGBA16F result, is dropped by
  the composite: the pixel keeps no in-scatter.

## Checking it

`--test=froxel` runs the Froxel Fog Test: the checks first (uniform fog against closed forms on lit,
PBR, particle and sky pixels, the inject and integrated slices, the height profile, the
Henyey-Greenstein ratios, shadow shafts, the temporal history, the custom shader, the grid resize and
invalid settings, `ScatterTint`, the fade past the last cascade and `OutsideShadowLight`, the history
reset), then a 2 x 2 view: sun shafts through a slatted wall, height fog over boxes with a lit and a
PBR sphere, one sun seen with `Anisotropy` -0.5, 0 and 0.8, and a dense fog over spheres at four
distances with a translucent sphere, particles and the sky.

`--test=volume` runs the Fog Volume Test (budget and culling, `Scene` records against the direct API,
density and edge profiles, height profile, fades, noise against the CPU noise, drift, tint and
overlap mixtures, local lights, the debug view and its slice texture), and `--test=raymarch` the
Raymarch Fog Test (uniform box against the closed form at half and quarter resolution, depth edges,
shafts, cap, fades, steps, the froxel mix, translucents, a custom shader, resize, two scenes in
one `Begin`, and the debug view). Both read results back from the GPU.

## Showcase

`examples/Dustlight` puts the froxel fog, local volumes, the raymarch mode and the lantern scattering in one room, with a panel and `--check`.

## Headstone questions

A game's hall-haze note (the Coffin Works hall) asked these. Answers: **yes** with the API, **later**
with the phase, or **no** with why. H9 alone is still open.

| # | Question | Answer |
|---|---|---|
| H1 | An in-scatter tint apart from the light's colour | **Yes**: `VolumetricFog::ScatterTint`, finite and 0 or more, not clamped, in-scatter only. Per-volume tint comes with local volumes. |
| H2 | A vertical density profile inside a box | **Yes**: `HeightFadeStart/End` and `HeightBandBottom/Top/Multiplier/Softness` on `FogVolume`, in metres above the volume's bottom along its local up, so Headstone's heights hold when the box sits at y = 0. Globally, `BaseHeight` and `HeightFalloff`. |
| H3 | A cap on the in-scatter per pixel; a per-volume near fade; does the volume dim the surface behind it | **Yes**: `FogVolume::MaxInScatter` per pixel in Raymarch mode (froxel volumes share `VolumetricFog::MaxInScatter`; no global fog, no cap) and `NearFadeStart/End/Keep`, in-scatter only. The volume dims what is behind it (`colour * T + L`). |
| H4 | A smooth fade where the cascades end | **Yes**, internal: the shadow fades over the last cascade's tail with `CascadeBlend`'s width. |
| H5 | Scrolling noise: scale, drift, min and max density | **Yes**: `NoiseScale`, `NoiseVelocity`, `NoiseMin`, `NoiseMax`, with the clock `SetFogVolumeTime` (the Scene advances it). |
| H6 | High, low and off knobs at runtime | **Yes**: `SetVolumetricsEnabled`, `SetVolumetricGrid`, `SetVolumeRaymarchResolution` (global, half or quarter) and `FogVolume::RaymarchSteps` per volume. |
| H7 | A box volume with shadows off | **Yes**: an unshadowed light is lit and the ambient in-scatter is separate; both are checked. |
| H8 | The inject and integrate passes on DX11 and Vulkan, Windows and Linux | **Yes**: compute passes on Vulkan, DX11 and DX12, and Linux runs Vulkan. |
| H9 | Measured cost on a mid-range GPU | **Later** (the showcase phase): only the RX 7900 XTX is measured so far. |
| H10 | A ray march and the froxel fog in one view; translucents against the march; the march against a custom shader's own distance fog | **Yes, with limits**: a Raymarch volume and the froxel fog combine as `(c * Tf + Lf) * Tr + Tf(entry) * Lr`; translucent meshes and particles do **not** see a Raymarch volume (use a Froxel volume for glass); the composite lands on top of a custom shader's output, its distance fog included. Needs the post chain, else a froxel fallback. |
| H11 | A debug view of lit, shadowed and outside-cascade samples | **Yes**: `SetVolumetricDebugView(ShadowSamples)` (green lit, red shadowed, blue outside, yellow unshadowed) in both paths, and the F9 volumetrics block with a froxel slice view. |
| H12 | Past the last cascade: lit or dark, fade kept | **Yes**: `VolumetricFog::OutsideShadowLight`, 0..1, default 1. A light with no shadow stays unshadowed. |
| H13 | Only the directional light scatters | **Yes**: `VolumetricFog::LocalLightIntensity` and `FogVolume::LocalLightIntensity` (0 is directional only) and `VolumetricIntensity` on every light. |
| H14 | Ray-length thinning of the in-scatter; a fade when looking along the beam | **Yes**: `FarFadeStart/End/Keep` beside the near fade, in-scatter only. Looking along a beam is `Anisotropy` and the cap. |
| H15 | A box volume as the only fog, no global fog | **Yes**: a froxel volume alone turns the pass on (the distance `Fog` stays; the range is 0.5 to 64 unless a `Density` 0 global fog sets it). |
| H16 | Reset the history on a camera cut; stability at four times the timescale | **Yes**: `Renderer3D::ResetVolumetricHistory()` and `CameraComponent::ResetVolumetricHistory`. The history is per frame, and drifting noise is reprojected by the drift `SetFogVolumeTime` reports, so the timescale doesn't matter. |
