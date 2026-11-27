# Volumetric fog

Status: v1.3. Global froxel fog: height fog you can see light through, with sun shafts, on lit and
PBR meshes, translucent meshes, particles and the sky. Local volumes and local-light scattering
come in a later v1.3 phase.

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
| `AmbientIntensity` | 1 | Scales the scene's ambient light in the medium. |
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

## What gets fogged

| Drawn by | How |
|---|---|
| Lit and [PBR](pbr.md) materials, translucent or not, static, skinned and instanced | The fragment shader samples the volume at the surface. `Additive` fogs towards black and `Multiply` towards white, like distance fog. |
| [Particles](particles.md) | Every particle fragment, alpha and additive, before it is premultiplied. |
| The [sky](environment.md) | The sky takes the fog to `MaxDistance` (the last slice), so the horizon meets the haze. |
| Custom shaders | Include the contract below. |

There is no ambient occlusion fade under the volume: AO is applied after the fog (it multiplies the
fogged colour), so SSAO darkens dense haze as well as the surfaces.

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

- No ambient occlusion fade under the volume: SSAO also darkens dense haze.
- An orthographic camera gets no volume.
- One temporal scene a frame: a second view that frame, such as a minimap or a mirror, draws
  without the history.
- The volume is a global fog; local volumes, point and spot light scattering and noise come later
  in v1.3.
- Only the first casting directional light shadows the shafts, as for surfaces.

## Checking it

`--test=froxel` runs the Froxel Fog Test: the checks first (uniform fog against closed forms on lit,
PBR, particle and sky pixels, the inject and integrated slices, the height profile, the
Henyey-Greenstein ratios, shadow shafts, the temporal history, the custom shader, the grid resize and
invalid settings, `ScatterTint`, the fade past the last cascade and `OutsideShadowLight`, the history
reset), then a 2 x 2 view: sun shafts through a slatted wall, height fog over boxes with a lit and a
PBR sphere, one sun seen with `Anisotropy` -0.5, 0 and 0.8, and a dense fog over spheres at four
distances with a translucent sphere, particles and the sky.

## Headstone questions

A game's hall-haze note (the Coffin Works hall) asked these. Answers: **yes** with the API, **later**
with the phase, or **no** with why.

| # | Question | Answer |
|---|---|---|
| H1 | An in-scatter tint apart from the light's colour | **Yes**: `VolumetricFog::ScatterTint`, finite and 0 or more, not clamped, in-scatter only. Per-volume tint comes with local volumes. |
| H2 | A vertical density profile inside a box | **Later** (local volumes): a height gradient or curve on the volume component. Globally, `BaseHeight` and `HeightFalloff`. |
| H3 | A cap on the in-scatter per pixel; a per-volume near fade; does the volume dim the surface behind it | **Dims: yes**, the apply is `colour * T + L`. The cap and the near fade come with local volumes. |
| H4 | A smooth fade where the cascades end | **Yes**, internal: the shadow fades over the last cascade's tail with `CascadeBlend`'s width. |
| H5 | Scrolling noise: scale, drift, min and max density | **Later** (local volumes): all four as fields. |
| H6 | High, low and off knobs at runtime | **Yes**: `SetVolumetricsEnabled` and `SetVolumetricGrid`. The ray-march quality knobs come with local volumes. |
| H7 | A box volume with shadows off | **Yes**: an unshadowed light is lit and the ambient in-scatter is separate; both are checked. |
| H8 | The inject and integrate passes on DX11 and Vulkan, Windows and Linux | **Yes**: compute passes on Vulkan, DX11 and DX12, and Linux runs Vulkan. |
| H9 | Measured cost on a mid-range GPU | **Later** (the showcase phase): only the RX 7900 XTX is measured so far. |
| H10 | A ray march and the froxel fog in one view; translucents against the march; the march against a custom shader's own distance fog | **Later** (local volumes). |
| H11 | A debug view of lit, shadowed and outside-cascade samples | **Later** (local volumes), with the grid debug panel. |
| H12 | Past the last cascade: lit or dark, fade kept | **Yes**: `VolumetricFog::OutsideShadowLight`, 0..1, default 1. A light with no shadow stays unshadowed. |
| H13 | Only the directional light scatters | **Later** (local-light scattering): a local-light intensity where 0 is directional only. |
| H14 | Ray-length thinning of the in-scatter; a fade when looking along the beam | **Later**: a far fade beside the near fade. Looking along a beam is `Anisotropy` and the cap. |
| H15 | A box volume as the only fog, no global fog | **Later** (local volumes). Until then a global fog with `Density` 0 works. |
| H16 | Reset the history on a camera cut; stability at four times the timescale | **Yes**: `Renderer3D::ResetVolumetricHistory()` and `CameraComponent::ResetVolumetricHistory`. The history is per frame and the medium is static, so the timescale doesn't matter. |
