# Environment and skybox

Status: v1.3. A high-dynamic-range panorama drawn as the scene's sky. The same `Environment` feeds
image-based lighting in a later v1.3 phase; until then it lights nothing.

An `Environment` is an equirectangular Radiance `.hdr` panorama converted on the GPU to a cube map of
linear `RGBA16F` radiance with a full mip chain. The sky draws it behind the opaque meshes, under
translucents and particles. The caller owns it, or the `AssetManager` does for one it loaded.

## Loading one

```cpp
// Direct: the .hdr is read now; nothing reaches the GPU yet.
Environment* sky = Environment::Create(EnvironmentParams()
    .SetPath("env/meadow.hdr")
    .SetFaceSize(512));            // 0 (the default) picks a size from the panorama

// Through the asset manager: ".hdr" is AssetType::Environment.
AssetHandle handle = assets.Load("env/meadow.hdr");
Environment* sky = assets.GetEnvironment(handle);   // or assets.Get<Environment>(handle)
```

`Create` returns `nullptr`, with an error, when there is no graphics device (a headless app), the file
is not a readable Radiance `.hdr`, or `FaceSize` is not valid. Delete an `Environment` you created
before `Renderer::Destroy`; never delete or `Destroy()` one the `AssetManager` owns. The file is looked up
like any asset path: the asset root, the working directory and a mounted [asset package](distribution.md).

**Face size.** Each cube face is `FaceSize` texels square.

- `FaceSize = 0` (auto) takes a quarter of the panorama's width, rounded down to a power of two, within
  32 to 1024. A 4096-wide panorama gets 1024.
- An explicit size must be a power of two from 16 to 2048. Anything else makes `Create` fail.
- A 1024 face with its mips takes about 64 MB (6 faces at 8 bytes a texel, a third more for the chain);
  a 512 face a quarter of that. The panorama's own pixels sit in system memory until the bake.
- The `AssetManager` always uses auto.

## When it is baked

`Create` only reads the file. The conversion runs on the GPU, lazily: the first `EndScene` that uses the
environment records it in the frame, one compute dispatch for mip 0 (the panorama resampled into the
cube, 2 x 2 taps a texel) and one per further mip (filled from the mip above). The scene that triggers it
pays the cost once (`Renderer3D::Statistics::EnvironmentBakes` is 1 for it); later scenes find the cube
ready.

To pay the cost earlier, for instance behind a loading screen, call `Environment::Prepare()` inside a
frame (between `Renderer::BeginFrame` and `EndFrame`). It returns `true` once the cube is ready, at once
if it already was, and `false` outside a frame, in a frame that renders nothing
(`Renderer::IsFrameSkipped`), or when the bake shader failed to compile. `IsReady()` says whether the
bake has been recorded. A scene whose environment is not ready draws no sky.

## Using it

### From the renderer

```cpp
renderer.BeginScene(camera);
SceneEnvironment sky;
sky.Environment = environment;
sky.Intensity = 1.0f;       // linear multiplier on the radiance, 0 or more
sky.Rotation = 90.0f;       // degrees about +Y
renderer.SetEnvironment(sky);
// ... SubmitMesh ...
renderer.EndScene();
```

`SetEnvironment` is scene-scoped like `SetFog` and the lights: the last call before `EndScene` wins and
`EndScene` clears it, so set it again each scene. `ClearEnvironment()` drops it earlier, and a `nullptr`
`Environment` clears it too. It returns `false` and warns once for an `Intensity` that is negative or not
finite, or a `Rotation` that is not finite, and leaves the scene as it was. `DrawSkybox = false` keeps the
environment for lighting without drawing the sky.

### From the scene

```cpp
Entity sky = scene.CreateEntity("Sky");
sky.AddComponent<SkyboxComponent>(environment, 1.0f, 0.0f);   // environment, intensity, rotation
```

A `SkyboxComponent` holds `Environment`, `Intensity`, `Rotation`, `Visible`, `FollowSun`, `TintSun` and `Enabled`. `Visible = false`
draws no sky but keeps the environment as the scene's, which the later lighting phase reads. `Enabled = false`
takes the component out of the choice. The component does not own the `Environment`.

`Scene::SubmitLights` picks one, as for `FogComponent` (see [Fog](lighting.md#fog)):

1. an enabled one on the camera entity passed in (the `SceneRenderer` passes its perspective camera),
2. else the first enabled one, in entity order, on an entity without a `CameraComponent`,
3. else, for a pass that names no camera, the first enabled one on a camera's entity.

A view whose camera is named never takes another camera's sky. More than one scene-wide candidate
warns once and uses the first. An entity that is inactive in the hierarchy doesn't count. A sky is no light.

A direct `SetEnvironment` call and a `SkyboxComponent` share the one slot of the scene: whichever call
comes last before `EndScene` wins. `SubmitLights` makes the component's call, so a `SetEnvironment` after
it overrides the component.

## Orientation

The panorama's centre column (u = 0.5) looks down **-Z**, u = 0.75 looks down **+X**, and the top row
is **+Y**. `Rotation` turns the sky about +Y in degrees, right-handed: a positive angle carries what was
at -Z toward -X (and +X toward -Z). The `.hdr` rows are flipped on load, as `Texture::CreateFromHdrFile`
does, so the file's top row ends up as +Y.

## Units and colour space

The cube holds **linear radiance**, as the file stores it, and `Intensity` is a linear multiplier. The
sky draws `LinearToSrgb(radiance * Intensity)` into the scene target, the same encoding the lit and
PBR shaders output, so a radiance of 1 shows as white, like a fully lit white surface.

- With a [post chain](post-processing.md) the target is HDR, so values above 1 survive into bloom and
  the tone curve. Without one they clip on the swap chain.
- Exposure acts on the encoded values, as it does for every lit pixel: -2 EV is not a quarter of the
  radiance.
- A half-float texel tops out at 65504, so a sun brighter than that is cut off (see
  [Textures](textures.md)).

## Fog

Distance fog does not apply to the sky. To make the horizon meet the fog, set `FogComponent::UseClearColor`
to `false` and `Fog.Color` to the colour of the sky's horizon. Fog through the air ([volumetric fog](volumetrics.md)) does: a scene with it
fogs the sky to the volume's `MaxDistance`, so the horizon meets the haze without matching colours.

## Image-based lighting

A ready environment also lights [PBR materials](pbr.md). Three things are derived from the cube map
when it bakes: the **irradiance** as nine spherical-harmonics coefficients (the diffuse term), a
**prefiltered specular** cube (128 per face, 6 mips, roughness m/5 at mip m) and, once per renderer, a
128 x 128 **BRDF lookup** (the split-sum scale and bias). Lit materials and the flat ambient colour are
untouched, and `SceneEnvironment::Lighting` / `SkyboxComponent::Lighting` (default true) switch it off.

- **When.** Inside `Prepare`, so the first `EndScene` that uses the environment (or your own `Prepare`
  call) pays for it, together with the sky bake. The BRDF lookup bakes on the first environment bake of a
  frame. `IsReady()` is true once all of it is recorded.
- **Replaces ambient.** With an environment set and `Lighting` on, PBR materials take their ambient from
  the environment and `AmbientColor` no longer reaches them; analytic lights are unchanged. With
  `Lighting` off, or no environment, the flat ambient colour applies as before.
- **Units.** Uniform radiance 1 at intensity 1 lights a surface exactly as a white `AmbientColor` of 1
  does (diffuse). The lighting scale is `Intensity x LightingIntensity`: `Intensity` is the sky's
  brightness and is shared, `LightingIntensity` (0 or more, default 1) is the extra multiplier for the
  lighting alone, so you can dim the light without dimming the sky. There is no separate exposure
  control; [post exposure](post-processing.md) acts on the encoded sky and the PBR surfaces alike.
- **Rotation** is the sky's: `Rotation` turns the lighting with the sky.
- **Hidden sky.** `DrawSkybox` / `Visible` false still lights.
- **Specular occlusion.** A material's occlusion map also darkens its reflections (see
  [PBR materials](pbr.md#ambient-and-image-based-lighting)).
- **Hot reload.** A reloaded `.hdr` bakes all of it again at the next scene that uses it.
- **Memory.** About 1 MB per `Environment` (the specular cube) and per `Renderer3D` (its own copy), and
  128 KB for the lookup. The renderer keeps its copy so materials never bind an environment's own
  textures; it re-copies whenever the environment or its bake changes. None of this is public.
- **Limits.** One environment per scene. No multiple scattering, so a rough metal reads a little darker
  than it should, most visibly in a furnace test. The diffuse term is band-limited (SH9): a sun in the
  panorama lights softly, not as a shadow-casting light, so use a directional light for the sun.
- **Custom shaders.** The renderer binds the environment only to a shader that declares all four of
  `DE_EnvironmentSh` (the SH buffer), `u_EnvironmentSpecular`, `u_EnvironmentBrdf` and `u_EnvironmentSampler`
  (as `Renderer3D_Pbr.glsl` does); one or two of them bind nothing. Declare all four or none.

`Renderer3D::Statistics::EnvironmentLighting` says whether the last scene lit PBR materials from the
environment, and `EnvironmentLightingCopies` how many times it copied the lighting in (1 whenever the
environment or its bake changed, else 0).

## Procedural sky

An `Environment` can also be a sky computed from a sun instead of read from a file: a physically based
atmosphere (Hillaire 2020: Rayleigh, Mie and ozone, with multiple scattering), baked on the GPU into the same
cube map. Twilight, the red sunset and the dark of night come from the physics, and the sun's light colour
can follow them.

```cpp
ProceduralSky sky;
sky.SunDirection = { -0.4f, -0.5f, -0.3f };   // the way the light travels, as DirectionalLight::Direction
Environment* environment = Environment::CreateProcedural(ProceduralEnvironmentParams().SetSky(sky));   // 256 texel faces

SceneEnvironment scene;
scene.Environment = environment;
renderer.SetEnvironment(scene);
```

Everything else about it is an ordinary `Environment`: the same `SceneEnvironment`, `SkyboxComponent`,
image-based lighting and `IsReady()`. `IsProcedural()` tells them apart, `GetSky()` returns the sky last
accepted, and `SetSky` replaces it (it returns false, with one warning, for a field that is not finite or out of
range, and keeps the old sky).

| `ProceduralSky` field | Default | Meaning |
|---|---|---|
| `SunDirection` | (-0.4, -1, -0.35) | The way the sun's light travels. |
| `SunColor` | white | sRGB colour of the light above the atmosphere. |
| `SunIntensity` | 2 | Linear; a white floor under an unveiled sun has this radiance. Scales the whole sky. |
| `SunDiscRadius` | 0.5 deg | 0 to 10; 0 draws no disc. |
| `SunDiscIntensity` | 20 | How much brighter the disc is than the sunlight on the ground. |
| `Turbidity` | 1 | Haze, 0 to 50. |
| `MieAnisotropy` | 0.8 | How tight the glow around the sun is. |
| `RayleighScale`, `OzoneScale` | 1 | Density of the air and of the ozone layer. |
| `GroundAlbedo` | 0.3 | sRGB colour below the horizon. |
| `NightColor` | (0.0015, 0.002, 0.004) | Linear radiance added everywhere, so night is not black. |
| `Altitude` | 0 | Camera height in metres. |
| `RebakeAngle` | 0.25 deg | How far the sun may move before the sky is baked again. |

### Following the sun

`SceneEnvironment` and `SkyboxComponent` have two switches, both on by default and ignored for a panorama:

- `FollowSun` points the sky's sun along the scene's main directional light (the one that casts shadows, else
  the first; a scene with no light follows the default one). A scene that moves the light re-bakes the sky when
  the sun has moved `RebakeAngle`; one that does not move it never bakes again.
- `TintSun` multiplies that light's colour by the atmosphere's transmittance
  (`Environment::GetSunTransmittance`), so lit and PBR surfaces, fog and [volumetric shafts](volumetrics.md)
  redden at sunset and go dark when the sun sets. It needs `FollowSun` to agree with the sky.

`Rotation` is ignored for a procedural environment, because the sun and the lights must keep agreeing.

### The sun disc

The cube holds scattered light only. The skybox adds the disc itself, at the live light direction, after the
cube lookup: its radiance is `SunDiscIntensity x SunIntensity x SunColor x transmittance x Intensity`, with limb
darkening and an antialiased edge, and the post-process bloom catches it. The disc, its radius and its intensity
never re-bake the sky. Below the horizon it fades out over one degree.

### Cost

A bake runs in one frame, at most once per environment per frame (a second scene that frame with another sun
uses the cube as it is). It costs about 0.4 ms at a 512 texel face with nothing to upload; a 256 face is less. That figure was
measured on an RX 7900 XTX by the Procedural Sky test's check 13 (30 re-bakes at a 512 face; mean per bake:
Vulkan 0.38-0.48 ms, DX11 0.52-0.54 ms, DX12 0.32-0.39 ms). Mid-range GPUs are unmeasured. A 256 cube is about 4 MB (512: 17 MB). Check it with the `Environment` GPU timer.

### Limits

- Chrome (a perfect mirror) shows the sky's GGX highlight in the lighting, not a sun disc, because the disc is
  not in the cube.
- `Rotation` has no effect.
- One bake per environment per frame.
- The volumetric fog's ambient term stays the flat `AmbientColor` and does not follow the sky (#344).

## Limits

- **Perspective cameras only.** An orthographic camera's scene draws no sky.
- **Cube maps are not for 2D drawing.** `Renderer2D`, ImGui and DingoUI can't take a cube texture (see
  [Textures](textures.md#limits)); `Environment::GetCubeMap()` is for your own shaders and compute passes.
  Its contents are undefined until `IsReady()`.
- **No MSAA.**
- **Lifetime.** The `Environment` must stay alive from `SetEnvironment` (or the scene's `SubmitLights`)
  until that scene's `EndScene`. It is safe to delete afterwards: the renderer keeps the sky's material by
  the cube's id and drops it once the scene has gone unused for a few scenes.

## Hot reload

With `AssetManager`'s `EnableHotReload` on, saving the `.hdr` reloads the `Environment` in place: the
`Environment*` stays valid, so does the cube map's `Texture::GetId()`, and the face size is chosen again
(auto) for the new panorama. It bakes again at the next scene that uses it. A file that fails to decode
keeps the loaded one. A file read from a package never reloads. `AssetManager::Reload` does the same.

## Statistics and checking it

`Renderer3D::Statistics::Skybox` says whether the last scene drew a sky (a ready environment, `DrawSkybox`
and a perspective camera), and `EnvironmentBakes` how many environments it baked; the lighting counters are under
[Image-based lighting](#image-based-lighting).

The test app's **Skybox Test** (`--test=skybox`) draws the sky and reads pixels back, including an
orthographic camera, `DrawSkybox` off, an invalid intensity and `SkyboxComponent` selection. The doctests
(`EnvironmentTests.cpp`) cover the orientation maths and the face-size rule.
