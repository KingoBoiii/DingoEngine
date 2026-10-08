# Lighting & Shading

*(v0.7)*

`Renderer3D` lights every mesh it draws, per pixel, with up to four **directional** lights, an
**ambient** colour (summed from any number of components), and up to 32 **point** and **spot**
lights per scene. A material can
add a specular highlight and an emissive glow. The defaults reproduce the v0.6 look, and specular
stays off until a material asks for it.

> **Two ways to use it.** In the Scene/ECS, add light components to entities and the
> `SceneRenderer` submits them every frame ([Lights on entities](#lights-on-entities)). Outside the
> ECS, call `Renderer3D::SubmitLight` and `SetAmbientLight` yourself
> ([Lights without the ECS](#lights-without-the-ecs)). Both follow the same rules below.

## Light types

| Light | Direct API (`Graphics/Light.h`) | Component | Notes |
|---|---|---|---|
| Directional | `DirectionalLight` | `DirectionalLightComponent` | Parallel rays, no falloff. At most `Renderer3D::k_MaxDirectionalLights` (4) per scene. |
| Point | `PointLight` | `PointLightComponent` | Light from a position in every direction, reaching zero at `Range`. |
| Spot | `SpotLight` | `SpotLightComponent` | A point light limited to a cone. |
| Ambient | `Renderer3D::SetAmbientLight(color, intensity)` | `AmbientLightComponent` | Reaches every face equally. |

| Field | Default | Meaning |
|---|---|---|
| `Color` | white | Multiplied by `Intensity`. |
| `Intensity` | 1.0 (`AmbientLightComponent`: 0.1) | Brightness. For point and spot lights, the brightness at the light. |
| `Direction` | (-0.4, -1, -0.35) for directional | The way the light travels. |
| `Range` | 10 | Point and spot only, in world units. The light reaches exactly zero here. |
| `InnerConeAngle`, `OuterConeAngle` | 20, 30 | Spot only, degrees from the cone's axis. |
| `Enabled` | true | Point and spot components only. |

- A scene's lights add up, and so do its ambient components (the direct API has one ambient,
  which `SetAmbientLight` replaces). Past 1.0 the frame clips (see [Limits](#limits)) unless the
  [post chain](post-processing.md) tone-maps it.
- **Components:** point and spot lights take their position from the entity's world transform (its
  `Transform3DComponent`, through any parents) and are ignored without one, with a one-time
  warning. A spot's `Direction` is in the entity's local space (default (0, 0, -1)) and is turned by
  its world rotation; scale has no effect.
- **Direct API:** `SpotLight::Direction` is a world direction and defaults to (0, -1, 0).

## Lights on entities

```cpp
void AddNightLighting(Scene& scene)
{
    DirectionalLightComponent& moon = scene.CreateEntity("Moon").AddComponent<DirectionalLightComponent>();
    moon.Color = { 0.55f, 0.65f, 1.0f };
    moon.Intensity = 0.12f;
    moon.Ambient = 0.05f;

    Entity lantern = scene.CreateEntity("Lantern");
    lantern.AddComponent<Transform3DComponent>().Position = { 2.0f, 1.5f, 0.0f };
    lantern.AddComponent<PointLightComponent>(PointLightComponent({ 1.0f, 0.72f, 0.42f }, 1.5f, 8.0f));

    Entity spotlight = scene.CreateEntity("Spotlight");
    spotlight.AddComponent<Transform3DComponent>().Position = { 0.0f, 6.0f, 0.0f };
    SpotLightComponent& beam = spotlight.AddComponent<SpotLightComponent>();
    beam.Direction = { 0.0f, -1.0f, 0.0f };
    beam.Range = 12.0f;
    beam.InnerConeAngle = 12.0f;
    beam.OuterConeAngle = 20.0f;
}
```

A scene driven by `SceneManager` needs nothing more: the `SceneRenderer` calls
`Scene::SubmitLights` inside its 3D pass. A custom pass calls it like `RenderEntities3D`:

```cpp
renderer.BeginScene(camera);
renderer.Clear({ 0.02f, 0.02f, 0.04f, 1.0f });
scene.SubmitLights(renderer);
scene.RenderEntities3D(renderer);
renderer.EndScene();
```

Switch a lantern off with `Enabled = false` rather than zeroing `Intensity`, so the value is kept.

## Lights without the ECS

```cpp
void DrawLitScene(const PerspectiveCamera& camera)
{
    Renderer3D& renderer = Application::Get().GetRenderer3D();

    renderer.BeginScene(camera);
    renderer.Clear({ 0.02f, 0.02f, 0.04f, 1.0f });

    renderer.SetAmbientLight({ 0.5f, 0.6f, 0.9f }, 0.06f);
    renderer.SubmitLight(DirectionalLight{ { 0.3f, -1.0f, -0.5f }, { 0.6f, 0.7f, 1.0f }, 0.25f });
    renderer.SubmitLight(PointLight{ .Position = { 2.0f, 1.5f, 0.0f }, .Color = { 1.0f, 0.72f, 0.42f },
                                     .Intensity = 1.5f, .Range = 8.0f });
    renderer.SubmitLight(SpotLight{ .Position = { 0.0f, 6.0f, 0.0f }, .Direction = { 0.0f, -1.0f, 0.0f },
                                    .Range = 12.0f, .InnerConeAngle = 12.0f, .OuterConeAngle = 20.0f });

    renderer.DrawSphere(glm::mat4(1.0f), { 0.8f, 0.8f, 0.8f, 1.0f });
    renderer.EndScene();
}
```

`SetAmbientLight` replaces the scene's ambient, which is black if you never set it.

## Scene-scoped lighting and the default light

Lighting is rebuilt for every scene. Whatever you have submitted since the last `EndScene`, before
or after `BeginScene`, lights the next `EndScene`, which then clears it. So "set the light, then
`BeginScene`" works, and a second scene in the same frame starts with no lights. Until an
`EndScene` runs, at most 8192 point and spot lights can wait; further ones are ignored with a
warning. One renderer can run up to `Renderer3D::k_MaxScenesPerFrame` (32) scenes a frame: on Vulkan
its scene buffer has room for that many writes, and later scenes would draw with stale lighting.

**Direct API.** A scene that submits no light and no ambient is lit by the default light from
`Renderer3DParams`: `LightDirection` (-0.4, -1, -0.35) and `Ambient` 0.35. It is emitted as a white
ambient of `Ambient` plus one white directional light of intensity `1 - Ambient`, which is the
pre-v0.7 image. Set it at startup through `ApplicationParams::Renderer3D`, or at run time with
`SetDirectionalLight(direction, ambient)`, which changes only this default. Any `SubmitLight` or
`SetAmbientLight` call in a scene switches it off, so a scene with only point lights gets no sun.
For no light at all, call `SetAmbientLight({ 0, 0, 0 }, 0)`.

**Components.** `LightSystem` (behind `Scene::SubmitLights`) submits every light component each
frame, and always calls `SetAmbientLight` with the summed ambient, black if there is none.

- A registry with **no light component at all** gets a default `DirectionalLightComponent`, so a
  3D scene never renders black by accident.
- Any light component turns that default off, even a disabled one, except a point or spot light
  with no `Transform3DComponent`, which can't be placed. A scene whose lights are all switched off
  therefore goes dark.
  It does not fall back to the renderer's default light, which the `SceneRenderer` never touches.

**Legacy `Ambient`.** `DirectionalLightComponent::Ambient` (default 0.35) is the engine's original
single knob. It adds white ambient and scales the light by `(1 - Ambient)`, which is exactly the
old `ambient + (1 - ambient) * N.L` for any `Ambient`. A face squarely turned to the light is
`Ambient + Intensity * (1 - Ambient)` bright, exactly 1 at the default `Intensity`. Set `Ambient`
to 0 to use `AmbientLightComponent` instead and get `Intensity` unscaled. Only the directional
components the renderer accepts (the first four valid ones) add their ambient, and a non-finite
`Ambient` or ambient intensity is skipped rather than blanking every other ambient source.

## The light budget

Directional lights are not culled or ranked: the first four in submission order are used, and
further ones are dropped. Light components are submitted in entity order, which is roughly the
order their entities were created and doesn't change when another light is removed.

Point and spot lights share one budget.
`Renderer3DCapabilities::MaxLocalLights` defaults to 32 and is capped at
`Renderer3D::k_MaxLocalLights` (also 32), so the setting can only lower it.
`Renderer3D::GetLocalLightBudget()` returns the value in force.

```cpp
ApplicationParams params;
params.Renderer3D.Capabilities.MaxLocalLights = 16;
params.Renderer3D.Capabilities.AssertOnOverflow = true;
```

At each `EndScene`:

1. Lights whose range sphere misses the view frustum, near and far planes included, are skipped
   first. They do not count against the budget, since nothing they could light is on screen.
2. If the rest fit the budget, all are drawn.
3. Otherwise the strongest as seen from the camera are kept. A light's score is its strongest
   colour channel multiplied by `Intensity`, divided by `1 + gap²`, where `gap` is how far the
   camera is outside the light's range (0 when inside it). The strongest channel is used, not
   luminance, so a saturated blue light ranks the same as a green one of equal intensity. With an
   orthographic view-projection the camera has no position, so distance does not count.
4. Every light whose range holds the camera scores the same, so ties go to the light the camera
   is nearer to relative to its range, then to the earlier-submitted one. A still scene submitting
   lights in a stable order picks the same lights every frame and never flickers.

Overflow logs one warning per renderer, not one per frame. `AssertOnOverflow = true` makes it an
assert instead (also for directional overflow and a mesh too big for a batch). Asserts compile out
of release builds, which still warn and drop.

A point or spot light is ignored without a message when its `Intensity` or `Range` is not above
zero, or when its position, colour, direction or cone angles are non-finite. A directional light is
only rejected when its direction or colour × intensity is non-finite: one at `Intensity` 0 still
takes one of the four slots, a negative one subtracts light, and its `Direction` must be non-zero.
Spot angles are clamped: `OuterConeAngle` to 1-179 degrees, `InnerConeAngle` to 0 up to the outer
angle.

**Seeing it.** `Renderer3D::GetStatistics()` reports `DirectionalLights` (the default light
included), `LocalLights`, `CulledLights` and `DroppedLights`, complete after `EndScene`. The F4
Renderer tab shows them under "Renderer3D lights": directional `n / 4`, a point/spot bar against
the budget, "Out of view", and a red "Dropped" line when anything was dropped. "Dropped" counts
both kinds; the log says which. `UI::RendererStatsSection()` embeds it in your own window.

**The budget fade** (v0.9). By default the cut-off is hard: when a light's rank crosses the budget
edge, as the camera moves, it switches on or off within one frame. Set
`Renderer3DCapabilities::LightBudgetFade` (or call `Renderer3D::SetLightBudgetFade`) to a band
above 0 and, whenever more lights reach the view than the budget holds, a drawn light fades out as
its priority nears that of the first light left out: fully lit at `1 + band` times it, dark at it.
Two lights trading places at the edge are both dark at the moment they trade, so nothing pops.

```cpp
renderer3D.SetLightBudgetFade(0.5f); // fade over the last 50 % above the cut
```

With the fade on, lights are ranked by one continuous priority, the brightness seen from the camera
(as above) over `1 + distance / Range`, instead of brightness with ties broken by distance; under the
budget nothing fades and nothing changes. Off (the default, 0) keeps the ranking and the hard cut of
v0.7, so existing scenes render exactly as before. `Statistics::FadedLights` and the F4 tab count the
dimmed lights. The frustum test still cuts a light whose range leaves the view, so a light whose
range just reaches the screen's edge can still change the ranking at once. Keep ranges short so that
few lights overlap on screen, or fade your own lights to fit, as [Candlewick does](#gameplay-queries).

## Falloff and cones

Each light contributes `Color * Intensity * max(N.L, 0) * falloff * cone` to a pixel, and the
pixel is `albedo * (ambient + sum of lights) + specular + emissive`.

```
falloff = (1 - (d / Range)²)²                              // 1 at the light, 0 at and past Range
cone    = saturate((cos θ - cos Outer) / (cos Inner - cos Outer))²   // spot only
```

- `d` is the distance to the light. Past `Range` a light adds nothing, so a light's reach is
  exactly its range sphere.
- `θ` is the angle between the spot's axis and the direction from the light to the pixel. The cone
  is 1 inside `InnerConeAngle` and fades to 0 at `OuterConeAngle`: linear in the cosine of the
  angle, then squared like the falloff. Equal angles give a nearly hard edge.
- Directional lights have neither term.

## Gameplay queries

`GetLightAttenuation(light, point)` (`Graphics/Light.h`, for a `PointLight` or a `SpotLight`)
returns the weight the lit shader gives that light at a world point, from 0 to 1: `falloff` for a
point light and `falloff * cone` for a spot, each exactly as defined above (both already squared,
so 0.5625 at half range on a spot's axis). It sets the cone up with the renderer's own code (the
angle clamps, the direction normalised, a zero direction pointing down), so the cone a game tests
is the cone the player sees.

- It leaves out the surface's `N.L` and the light's `Color` and `Intensity`: it says how much of
  the light reaches the point, not how bright a surface there looks.
- It ignores occlusion: shadows ([Shadows](shadows.md)) are drawn, not part of this weight.
  `Scene::GetShadowedLightAttenuation` multiplies in what the shadows let through, read back from the
  GPU ([Is this point in shadow?](shadows.md#is-this-point-in-shadow)); otherwise pair it with a
  raycast when walls should block.
- It knows nothing about this frame's budget. A light dropped past `MaxLocalLights`, or refused
  because too many were submitted, is not drawn, yet still has a weight. A game whose rules depend
  on a light being seen should keep that light within the budget.
- A light `SubmitLight` ignores weighs 0 everywhere: `Intensity` or `Range` not above zero, or a
  non-finite position, colour × intensity, direction or cone angle. An infinite `Range` is
  accepted, and then the falloff is 1 everywhere.

`PointLightComponent::ToLight(transform)` and `SpotLightComponent::ToLight(transform)` build the
light `Scene::SubmitLights` draws for a component: the transform's position and, for a spot,
`Rotation * Direction`. They do not look at `Enabled`; check it yourself. The transform is read as
world space. For a light under a [parent](scenes-and-ecs.md#parenting-v071), pass one built from
`GetWorldPosition()` and `GetWorldRotation()`, not the entity's local `Transform3DComponent`.

```cpp
bool SeesPoint(Entity warden, const glm::vec3& point)
{
    const SpotLightComponent& eye = warden.GetComponent<SpotLightComponent>();
    const SpotLight eyeLight = eye.ToLight(warden.GetComponent<Transform3DComponent>());
    return eye.Enabled && GetLightAttenuation(eyeLight, point) >= 0.2f;
}
```

`examples/Candlewick/src/Detection.cpp` is the full worked example. A warden's eye is a
`SpotLightComponent`, and every frame the game builds its light with `ToLight` and weighs it at three
points on the player: feet (0.1 m), chest (1.0 m) and head (1.6 m). A sample counts as seen when its
weight is at least 0.1, which is where the lit pool on the floor fades out of sight, and a ray from the
eye towards it hits nothing more than 0.3 m short of it (the player is a character controller, so no
ray ever hits the player itself). The eye's `Range` is also clamped every frame to the wall it faces,
found with a level ray from the eye, so neither the drawn cone nor its weight reaches through that
wall. The cone the player sees lit on the floor is therefore the cone that catches them.

**Keeping gameplay lights inside the budget.** The weight ignores the frame's budget, so a game
whose rules read a light should not leave the choice of which lights are drawn to the engine's
ranking. Candlewick's `LightLod` (`examples/Candlewick/src/LightLod.cpp`) splits its lights in two. The
gameplay lights (the lantern, every warden's lamp and eye, lit braziers) are never touched. The
decorative flames (sconces and candles) are the pool it manages. Each frame it counts the gameplay
lights that are on and inside the view frustum, with the same plane extraction and sphere test
`Renderer3D` culls with, and gives the flames the slots left under `GetLocalLightBudget()` minus two
spare. Flames are ranked by distance to the camera's focus, with a small bonus for one already lit so
the cut-off does not flicker. The ones that do not fit fade their `Intensity` to 0 over 0.3 s and are
then disabled, and a flame fades in only when there is room, starting just before it enters the view.
If the exact in-view count is still over, the lowest-ranked flames in view drop at once. So the
engine's own selection never has to drop a gameplay light, and the flames that go do so by fading, not
by popping at the budget edge. The cost is a copy of the engine's cull test in game code, which has to
change if the engine's does, so the game warns once if the renderer ever drops a light with the LOD on.
`--no-light-lod` turns the LOD off to show the engine's own selection, and `--light-budget` stops at 16,
the 14 gameplay lights that can burn at once plus the two spare.

## Lit materials

Meshes with no material, and every material from `Renderer3D::CreateLitMaterial`, use the lit
shader. `MaterialParams` carries the surface settings:

| Field | Default | Meaning |
|---|---|---|
| `Roughness` | 0.5 | 0 gives a small, sharp highlight; 1 a wide, soft one. |
| `Specular` | 0 | Highlight strength. 0 turns specular off, which keeps existing scenes unchanged. |
| `EmissiveColor`, `EmissiveStrength` | black, 0 | Additive glow, independent of any light. |

```cpp
Renderer3D& renderer = Application::Get().GetRenderer3D();

Material* gold = renderer.CreateLitMaterial(MaterialParams()
    .SetDebugName("Gold").SetRoughness(0.25f).SetSpecular(0.6f));
Material* lamp = renderer.CreateLitMaterial(MaterialParams()
    .SetDebugName("Lamp").SetEmissiveColor({ 1.0f, 0.85f, 0.6f }).SetEmissiveStrength(1.5f));

entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color)).Material = gold;

delete gold;   // in Layer::OnDetach, before the renderer shuts down
delete lamp;
```

- **Specular** is normalised Blinn-Phong, tinted by the light's colour, not the albedo. Normalising
  keeps a highlight's energy constant as roughness changes its size, so a low `Roughness` gives a
  tiny, very bright highlight that clips to white; lower `Specular` for it. At 0 the code is
  skipped entirely.
- **Albedo texture:** `material->SetTexture(0, texture)` and `SetSampler(0, sampler)` multiply into
  the mesh colour. Slot 0 is the only slot the lit shader has: a texture or sampler in another slot
  keeps the material from being drawn, with a one-time warning. An empty slot 0 draws white with the clamp sampler.
- **Transparency** comes from the mesh colour's alpha, never the texture's. A lit draw below
  alpha 1 blends, but lit draws are not sorted and still write depth.
- **Both faces are drawn.** `CreateLitMaterial` sets the shader and `CullMode::None` for you, so
  open meshes and mirrored entities (a negative scale) still show. A custom material can cull; see
  [Winding and culling](scenes-and-ecs.md#winding-and-culling).
- **At run time,** `SetRoughness`, `SetSpecular` and the emissive setters are free to call every
  frame. A texture can be assigned after the first draw (a `LoadAsync` result, say), but changing a
  slot rebuilds the material's pipelines, so do not swap one every frame. A NaN setting falls back
  to roughness 0.5, specular 0 and no emissive.
- **`GetDefaultMaterial()`** holds the shared settings of every mesh with no material of its own.
  Settings belong to the material, not the mesh: give an entity its own lit material for its own
  glow or shine.
- **Ownership.** You own the material and must delete it before the renderer that created it shuts
  down. Any `Renderer3D` can draw it.

A material with your own `Shader` is not a lit material: its emissive, roughness and specular
settings are ignored, and the shader does its own lighting.

## Custom material shaders

The scene block (`CameraData`) is bound at **binding 0** on every material the renderer draws, with
your uniforms at 1 and your textures at 2 and up (see
[Scenes & ECS](scenes-and-ecs.md#custom-materials-per-mesh-shaders)). The block grew in v0.7, but
its first 96 bytes (the first three members below) are **frozen** and new members are only ever
appended after them, so a shader that declares only those three keeps compiling and rendering.

| Offset | Member | Contents |
|---|---|---|
| 0 | `mat4 ViewProjection` | |
| 64 | `vec4 LightDirection` | The first directional light's direction (xyz, the way it travels, not normalised). |
| 80 | `vec4 Ambient` | x = the strongest channel of the scene's ambient colour × intensity. |
| 96 | `vec4 CameraPosition` | w = 1: xyz is the camera's world position. w = 0: orthographic, xyz points towards the camera. |
| 112 | `vec4 AmbientColor` | rgb = colour × intensity, all ambient summed. |
| 128 | `ivec4 LightCounts` | x = directional lights, y = point and spot lights in use. |
| 144 | `DirectionalLight DirectionalLights[4]` | 32 bytes each: `vec4 Direction` (xyz, not normalised), `vec4 Color` (rgb × intensity). |
| 272 | `LocalLight LocalLights[32]` | 48 bytes each: `vec4 PositionRange` (xyz, w = range), `vec4 Color` (rgb × intensity, w = cone scale), `vec4 SpotDirection` (xyz normalised, w = cone offset). |

The whole block is 1808 bytes. Declare members in this order from the start; you may stop after
any of them. To use the lights, mirror the whole block, struct definitions included, from
[`Renderer3D_Lit.glsl`](../src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl), the reference
implementation. Either stage may declare binding 0 (the lit shader's vertex stage declares only
`ViewProjection`). Loop to `LightCounts`, never to the array size: entries past the counts hold
stale data. The cone factor is `saturate(dot(-toLight, SpotDirection.xyz) * Color.w +
SpotDirection.w)`, squared; for a point light `Color.w` is 0 and `SpotDirection.w` is 1, so it is
always 1.

The prefix carries **only the first directional light**, and no point or spot light. In a scene
with no directional light it still carries the default light's direction, so a shader using the old
`ambient + (1 - ambient) * N.L` formula draws a sun the lit shader does not.

## Hot-reloading the lit shader

The lit shader is a file, [`Renderer3D_Lit.glsl`](../src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl),
and a copy of it is embedded in the engine library at build time.

- **Debug and Debug-ASan** builds load it from the source tree and reload it when it changes on
  disk. This needs the AssetManager's hot-reload on: `params.Assets.EnableHotReload = true`, or the
  toggle in the F6 Assets tab (the test app enables it). The file is polled every
  `HotReloadInterval` (0.5 s by default), and the log reports each reload.
- A compile error while running is logged and the old program keeps drawing, as with any shader
  hot-reload ([Asset Pipeline](asset-pipeline.md#hot-reload-shaders--textures)). A compile error at
  startup is fatal in these builds, as for any shader.
- **Release and Distribution** builds always use the embedded copy, so they carry no build-machine
  path, and a prebuilt package needs no shader file beside it. A prebuilt Debug lib carries the
  path it was built from, and uses its embedded copy wherever that path doesn't exist.
- The source path is fixed when the project files are generated. If the file is not there, such as
  after moving the checkout, the embedded copy is used without a message.
- **Gotcha:** a Debug executable reads the file at startup, not the copy it was built with, so an
  old Debug build also runs the current file. After switching branches or pulling, rebuild.
- Shading maths is free to change. The C++ writes `CameraData` and the material block byte for byte
  (`static_assert`s in `Renderer3D.h` check `CameraData`), so a layout change needs `Renderer3D.h`
  to change too.

## Limits

- **Clipping without the post chain.** A pixel clips at 1.0 per channel, so many bright lights
  overlapping, or a bright light on a pale surface, burn out to white. Keep intensities
  conservative (ambient plus every light reaching a pixel near 1), or turn on the v0.9 post chain,
  whose default tone curve leaves everything under 0.8 alone and rolls off what would have clipped
  ([Post-processing](post-processing.md)).
- **Light passes through walls unless it casts.** The first directional light with `CastShadows`
  gets cascaded shadows, and up to 8 (at most 16) casting point and spot lights get shadows of their
  own ([Shadows](shadows.md)); every other light passes through walls and floors, so in interiors
  keep their ranges short.
- **No per-mesh surface parameters.** Roughness, specular and emissive are per material. A mesh
  that needs its own takes its own material, which is at least one more draw call.
- **Blinn-Phong, not PBR:** no metalness, normal maps or reflections.
- **Cost:** every pixel loops over every light in the scene (up to 4 + 32), with no tiling or
  per-object light lists, and there is no depth pre-pass, so heavy overdraw multiplies the cost.
- **Budget-edge popping** unless the budget fade is on, described [above](#the-light-budget).

## Migrating from v0.6

Scenes that used a single `DirectionalLightComponent` look the same. Three things change for code
that did more:

- **Custom passes no longer inherit the scene's sun.** The `SceneRenderer` used to write the first
  `DirectionalLightComponent` into `Renderer3D`'s default light every frame. It now submits the
  scene's lights instead, so 3D you draw on the shared renderer outside it, without
  `scene.SubmitLights(renderer)`, is lit by `Renderer3DParams`' default light, not the last scene's
  sun. Call `SubmitLights` in the pass, or submit lights directly.
- **Directional lights add up.** Several `DirectionalLightComponent`s now all light the scene, up to
  four, and each one's legacy `Ambient` adds up too; before, only the first counted. Two default
  components give an ambient of 0.7. Keep one sun, or set `Ambient` to 0 on the others.
- **The legacy prefix still carries only the first directional light.** A custom shader that reads
  only `LightDirection` and `Ambient` ignores the rest. Mirror the full block to see them all.

`Renderer3D::SetDirectionalLight` still works, but only for scenes that submit no light of their
own, since submitting any light or ambient switches the default off.

## See also

- [Scenes & ECS](scenes-and-ecs.md) - 3D entities, `MeshRendererComponent` and custom materials.
- [Asset Pipeline](asset-pipeline.md) - hot-reload and the F6 Assets tab.
- The test app's **Lighting Test** (`test/`, `--test=light`), with
  `--lighting=default|lights|overbudget|materials`, `--entities` (the same lights as components)
  and `--specular=off`.
- **DungeonCrawler3D** with `--night` (`examples/DungeonCrawler3D/`): a dim moon, a lantern that
  follows the hero, and a point light on each treasure.
- **Candlewick** (`examples/Candlewick/`): the reference for lights that are gameplay. A lantern whose
  range is its oil, wardens whose spot-light cones are tested with `GetLightAttenuation`, a game-side
  light LOD, and lit emissive braziers as checkpoints. `--debug-cone` draws the tested cones and
  `--no-light-lod` shows the engine's own selection.
