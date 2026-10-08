# Post-processing

*(v0.9)*

The 3D pass can run through a **post chain**: the scene is drawn into an HDR target, where light is
free to go past 1.0, and a **tone curve** then decides how that light reaches the screen. Without it,
a pixel clips at 1.0 per channel, so overlapping bright lights burn out to white (GitHub #76).

The chain is off unless you ask for it, and off it changes nothing: the 3D pass draws straight into its
target, pixel for pixel as before v0.9.

## Turning it on

**In the ECS**, give the primary perspective camera a `PostProcessComponent`:

```cpp
Entity camera = scene->CreateEntity("Camera");
camera.AddComponent<Transform3DComponent>();
camera.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;

PostProcessComponent& post = camera.AddComponent<PostProcessComponent>();
post.Settings.Enabled = true;                         // the Soft curve by default
post.Settings.Tone.Exposure = 0.5f;                   // half a stop brighter
```

`SceneRenderer` runs the chain around the scene's 3D pass. The 2D overlay drawn after it (sprites,
text, the HUD) goes straight into the target, never tone mapped.

**Without the ECS**, wrap your `Renderer3D` scene in the engine's `PostProcessStack`:

```cpp
PostProcessStack& post = Renderer::GetPostProcessStack();
post.Begin(settings, camera.GetProjectionMatrix()); // draws now go to the HDR scene target
renderer3D.BeginScene(camera);
renderer3D.Clear(clearColor);
// ... lights and meshes ...
renderer3D.EndScene();
post.End();                          // tone-maps into the render target that was current at Begin
```

`Begin` sizes the scene target to the render target current at the time (the window, or a framebuffer
set with `Renderer::SetRenderTarget`), so the chain works for a scene rendered into a texture too.
Inside a `Renderer::SetViewport` rectangle (one view of a split screen) it takes the rectangle's size
and `End` tone-maps into that rectangle alone, leaving the viewport set; the `SceneRenderer` also
takes a viewport's aspect for its projections. The
projection is what ambient occlusion rebuilds positions from; `Begin(settings)` without one runs
every pass but AO (which then warns once).

## The tone curves

| `ToneMapOperator` | Below 1.0 | Above 1.0 | For |
|---|---|---|---|
| `None` | identity | clips, as without the chain | proving the chain changes nothing |
| `Soft` **(default)** | identity up to `Knee` (0.8) | rolls off smoothly to exactly 1.0 at `WhitePoint` (4.0); on the brightest channel, so hue holds | every existing game: its pixels under the knee keep their values, and what used to clip now rolls off |
| `ACES` | a filmic toe and contrast | a filmic shoulder | a game re-tuned for it: every value shifts |
| `Neutral` | Khronos PBR Neutral: subtracts a small offset, so the darks shift | compresses near 1 | a game re-tuned for it |

`Exposure` is in stops and multiplies the scene by `2^Exposure` before the curve.

The engine stays *display-referred*: textures, light colours and the lit shader mean what they always
did, and the curve only decides what happens to light that goes past 1. So a game can switch the
chain on without re-tuning, and then raise its lights or emissive strengths past what used to clip.

## Bloom

Light past a threshold glows: the dual-filter bloom of Jimenez's *Next Generation Post Processing in
Call of Duty: Advanced Warfare* (2014). The bright parts of the scene are filtered into six levels,
each half the size of the one before (from half resolution, R11G11B10F), then added back up level by
level with a 3 x 3 tent, and the result is added to the scene before the tone curve.

```cpp
post.Settings.Bloom.Enabled = true;
post.Settings.Bloom.Intensity = 0.6f;
```

| `BloomSettings` | Default | Meaning |
|---|---|---|
| `Enabled` | false | |
| `Intensity` | 0.5 | How much of the glow is added. |
| `Threshold` | 1.0 | Nothing blooms until its brightest channel passes this. At 1.0, only light that would have clipped glows, so a scene that stays inside 0..1 looks exactly as without bloom. |
| `Knee` | 0.1 | The glow grows in smoothly over this much more brightness, so a light brightening past the threshold doesn't switch its glow on. |
| `Radius` | 1.0 | The spread of each upsample, in texels of the smaller level. |

The first downsample uses Karis' average, which weighs each group of four pixels less the brighter it
is, so a single very bright pixel (a firefly from a specular highlight) can't flood the screen.

What is it for: lit materials with `EmissiveStrength` above 1 (a lamp, a brazier, a glowing orb) and
lights bright enough to burn a surface past 1.

## Ambient occlusion

Creases, corners and the ground under things darken a little, from the scene's depth alone:
Scalable Ambient Obscurance (McGuire, Mara and Luebke 2012). For each pixel, 12 taps on a spiral
around it, as far as `Radius`, ask how much of the space above the surface is closed off; a two-pass
blur that ignores taps of a different depth smooths the result without blurring across an edge.

```cpp
post.Settings.AmbientOcclusion.Enabled = true;
post.Settings.AmbientOcclusion.Radius = 0.5f; // metres around a point that count
```

| `AmbientOcclusionSettings` | Default | Meaning |
|---|---|---|
| `Enabled` | false | |
| `Radius` | 0.5 | How far around a point occluders count, in world units. Only what lies within it darkens the point, so a box floating a metre before a wall leaves no halo on the wall. |
| `Intensity` | 1.0 | How dark a closed corner gets. |
| `Power` | 1.5 | The result is raised to this power: higher keeps open surfaces lighter and corners darker. |
| `Bias` | 0.02 | Occluders closer than this to the surface's plane, in world units, don't count, so a flat floor stays exactly as it was. |
| `HalfResolution` | true | Works at half width and height: a quarter of the cost, slightly softer at edges. |

A forward renderer doesn't keep ambient light apart from direct light, so the result multiplies the
whole HDR colour after the opaque 3D pass, sunlit faces included; corners in full sun darken a little
too. `PostProcessStack::ApplyAmbientOcclusion` applies it early, once per `Begin`, so what is drawn
after it (particles, a translucent pass) isn't darkened; `End` applies it if nothing has.

## What it costs and what differs

- **Memory**: one RGBA16F colour target and a D32 depth per output size in use, 12 bytes a pixel (25 MB
  at 1920 x 1080). Targets idle for 300 frames are freed; a resized window resizes its target in place.
- **GPU**: two fullscreen passes (`Post` in the F8 Profiler tab): the tone map, and the scene's depth
  written into the caller's depth when it has one, so a 3D draw after `End` (a gizmo, a custom pass)
  depth-tests against this frame's scene, as without the chain. Plus the HDR target's bandwidth, and
  with bloom 11 small passes (`Bloom`, inside `Post`) over levels of a quarter of the screen's pixels
  and less, and another 4 bytes a pixel of memory for the levels. Ambient occlusion is four passes
  (`AO`, inside `Post`): 12 taps a pixel, two 9-tap blurs and the multiply, at a quarter of the
  pixels at half resolution, with two R8 targets.
- **Blending in 16-bit float** rounds differently from 8-bit, so the chain with `None` comes within
  1/255 of the frame without it, not exactly to it.

The F4 Renderer tab's *Post chain* section shows the scenes that ran it, the target size and memory.

## Checking it

The test app's **Post Test** (`--test=post`) draws a scene of overbright lights through the chain, with
the operator, exposure, knee and white point in its panel, and strips of each curve applied to a 0..8
gradient. Flags: `--tonemap=none|soft|aces|neutral`, `--exposure=<EV>`, `--post-off`. On start it checks
by readback that every curve rises, `Soft` is the identity to 1/255 up to its knee and reaches 1 at
its white point, `None` clips at 1, `Soft` keeps an overbright gradient's hue, the chain with `None`
is within 1/255 of no chain, and a disabled `Begin` draws exactly what no chain does; then that bloom
adds nothing to a gradient inside 0..1 and makes a small square at 8 glow past its edge, evenly on
both sides. `--post=bloom` (or `--bloom`) starts with bloom on. Then ambient occlusion, in a room:
the open floor is unchanged, the crease where floor meets wall darkens, and the wall beside a box
floating before it isn't darkened right up to the box's silhouette (no halo); `--post=ao` shows the
room with AO on. The Lighting Test takes `--post`, `--bloom` and `--ao` too: its materials mode's lamp
has an emissive strength of 1.5.

## For custom shaders

The curves live in an engine include, so a shader of your own can use them:

```glsl
#include <DingoEngine/ToneMapping.glsl>
// ToneMap(color, op, knee, white): op is a ToneMapOperator value (TONEMAP_NONE ... TONEMAP_NEUTRAL)
```
