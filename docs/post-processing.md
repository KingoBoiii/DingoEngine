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
post.Begin(settings);                // draws now go to the HDR scene target
renderer3D.BeginScene(camera);
renderer3D.Clear(clearColor);
// ... lights and meshes ...
renderer3D.EndScene();
post.End();                          // tone-maps into the render target that was current at Begin
```

`Begin` sizes the scene target to the render target current at the time (the window, or a framebuffer
set with `Renderer::SetRenderTarget`), so the chain works for a scene rendered into a texture too.

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

## What it costs and what differs

- **Memory**: one RGBA16F colour target and a D32 depth per output size in use, 12 bytes a pixel (25 MB
  at 1920 x 1080). Targets idle for 300 frames are freed; a resized window resizes its target in place.
- **GPU**: one fullscreen pass (`Post` in the F8 Profiler tab), plus the HDR target's bandwidth.
- **Blending in 16-bit float** rounds differently from 8-bit, so the chain with `None` comes within
  1/255 of the frame without it, not exactly to it.

The F4 Renderer tab's *Post chain* section shows the scenes that ran it, the target size and memory.

## Checking it

The test app's **Post Test** (`--test=post`) draws a scene of overbright lights through the chain, with
the operator, exposure, knee and white point in its panel, and strips of each curve applied to a 0..8
gradient. Flags: `--tonemap=none|soft|aces|neutral`, `--exposure=<EV>`, `--post-off`. On start it checks
by readback that every curve rises, `Soft` is the identity to 1/255 up to its knee and reaches 1 at
its white point, `None` clips at 1, `Soft` keeps an overbright gradient's hue, the chain with `None`
is within 1/255 of no chain, and a disabled `Begin` draws exactly what no chain does.

## For custom shaders

The curves live in an engine include, so a shader of your own can use them:

```glsl
#include <DingoEngine/ToneMapping.glsl>
// ToneMap(color, op, knee, white): op is a ToneMapOperator value (TONEMAP_NONE ... TONEMAP_NEUTRAL)
```
