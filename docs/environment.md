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

A `SkyboxComponent` holds `Environment`, `Intensity`, `Rotation`, `Visible` and `Enabled`. `Visible = false`
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
to `false` and `Fog.Color` to the colour of the sky's horizon. Fog through the air (froxel fog) comes in
a later phase.

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

`Renderer3D::Statistics::EnvironmentLighting` says whether the last scene lit PBR materials from the
environment, and `EnvironmentLightingCopies` how many times it copied the lighting in (1 whenever the
environment or its bake changed, else 0).

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
