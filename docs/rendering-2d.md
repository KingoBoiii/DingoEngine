# 2D Rendering

`Renderer2D` is a batched immediate-mode 2D renderer: quads (solid or textured),
circles, and MSDF text. Get it from the application:

```cpp
Renderer2D& r = Application::Get().GetRenderer2D();
```

## The scene block

All drawing happens between `BeginScene` and `EndScene`. `BeginScene` takes a
**projection-view matrix** (your camera); `EndScene` flushes the batches to the GPU.

```cpp
r.BeginScene(camera);                       // camera = glm::mat4 (proj * view)
r.Clear({ 0.1f, 0.1f, 0.12f, 1.0f });       // clear to a colour (RGBA)
// ...draw calls...
r.EndScene();
```

Do one `BeginScene`/`EndScene` pair per frame for a given camera. Each pair resets
and then flushes the quad, circle, and text batches, so you can freely mix all three
kinds of draw call inside one block.

A minimized app that keeps updating (`ApplicationParams::UpdateInBackground`) skips rendering, and
a block begun then draws nothing (`Renderer::IsFrameSkipped()`), so drawing from `OnUpdate` needs
no guard. See [In the background](application-and-layers.md#in-the-background).

### The camera

There is no camera *class* for 2D — you pass a matrix, which keeps the model simple
and explicit. Positions and sizes are in **world units** defined by that matrix. The
standard orthographic setup (used by the examples) is:

```cpp
// "orthoSize" world units tall, width derived from the window aspect ratio,
// centered on the origin.
float aspect = (float)Application::Get().GetWindow().GetWidth()
             / (float)Application::Get().GetWindow().GetHeight();
float halfH  = orthoSize * 0.5f;
float halfW  = halfH * aspect;
glm::mat4 camera = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
```

To move the camera (e.g. a follow camera), multiply by the inverse of its transform:

```cpp
glm::mat4 view   = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(cameraPos, 0.0f)));
glm::mat4 camera = projection * view;
```

> Build the workspace with `GLM_FORCE_DEPTH_ZERO_TO_ONE` defined (the examples and
> engine do) so GLM matches the Vulkan `[0, 1]` depth range. With the `-1/1` near/far
> ortho above, 2D content sits at z = 0.

## Quads

A quad's **position is its center**; size is its full width/height in world units.

```cpp
// Solid colour:
r.DrawQuad({ 0.0f, 0.0f }, { 1.5f, 1.5f }, { 0.9f, 0.3f, 0.3f, 1.0f });

// Textured (optionally tinted by the colour, default white = untinted):
r.DrawQuad({ 2.0f, 0.0f }, { 1.0f, 1.0f }, myTexture);
r.DrawQuad({ 2.0f, 0.0f }, { 1.0f, 1.0f }, myTexture, { 1, 1, 1, 0.5f }); // 50% alpha

// Rotated (rotation is in DEGREES, counter-clockwise about +Z):
r.DrawRotatedQuad({ -2.0f, 0.0f }, 45.0f, { 1.0f, 1.0f }, myTexture);
```

`DrawQuad`/`DrawRotatedQuad` accept a `glm::vec2` or `glm::vec3` position — use the
`vec3` form to control draw order via z when needed.

## Circles

`DrawCircle` takes a full transform matrix (so it can be scaled/positioned freely),
plus `thickness` (1.0 = filled disc, smaller = ring) and `fade` (edge softness):

```cpp
glm::mat4 t = glm::translate(glm::mat4(1.0f), { x, y, 0.0f })
            * glm::scale(glm::mat4(1.0f), { diameter, diameter, 1.0f });
r.DrawCircle(t, { 0.2f, 0.8f, 1.0f, 1.0f }, /*thickness*/ 1.0f, /*fade*/ 0.005f);
```

## Text

Text uses an MSDF atlas, so it stays crisp at any scale. Load a font once, then draw:

```cpp
Font* font = Font::Create("assets/fonts/arialbd.ttf");   // in OnAttach
...
r.DrawText("Score: 42", font, { x, y }, /*size*/ 0.5f, { .Color = { 1, 1, 1, 1 } });
```

`DrawText` parameters: the string, the font, a position (`vec2`/`vec3`), a `size` in
world units, and a `TextParameters { Color, Kerning, LineSpacing, Centered, Rotation }`. The
position is the **left baseline-ish origin** of the text (it grows to the right).

`TextParameters::Rotation` (v0.8.3) turns the string, in degrees counter-clockwise like
`DrawRotatedQuad`, about its position: where the first line's baseline starts, or its middle when
`Centered`.

```cpp
r.DrawText("GAME OVER", font, { 0.0f, 1.0f }, 0.8f, { .Color = red, .Centered = true, .Rotation = -8.0f });
```

To center text, offset by half its measured width:

```cpp
float w = font->GetStringWidth(text, size);
r.DrawText(text, font, { centerX - w * 0.5f, y }, size, { .Color = color });
```

`Font::GetStringWidth(text, size, kerning)` returns the width of the widest line, in the units
`DrawText` lays glyphs out in — pass the same `Kerning` you draw with. `TextParameters::Centered`
centers a string in one pass without measuring it first. Call `font->Destroy()` in `OnDetach`.

**Encoding (v0.6.2).** Strings are UTF-8. The atlas bakes Latin-1 (`U+0020`–`U+00FF`), the
printable General Punctuation (dashes, curly quotes, bullet, ellipsis, primes, guillemets) and
the euro sign; any other codepoint draws as `?`. A byte that is not valid UTF-8 reads as Latin-1,
so Latin-1-encoded text still draws. Only the engine itself compiles with MSVC's `/utf-8`: add it
to your game project too (`buildoptions { "/utf-8" }`), or write non-ASCII literals as escapes
(`"Caf\xC3\xA9"`), otherwise MSVC reads the source through the system code page.

## Textures

```cpp
Texture* tex = Texture::CreateFromFile("assets/sprites/player.png");
uint32_t w = tex->GetWidth();
uint32_t h = tex->GetHeight();
...
tex->Destroy();   // in OnDetach
```

- `Texture::CreateFromFile(path)` — load from PNG/JPG/etc. (via stb_image).
- `Texture::CreateFromData(width, height, data, format, debugName)` — from raw pixels.
- `Texture::Create(TextureParams{}...)` — full control (render targets, wrap modes,
  formats) via the fluent params builder.

A common pattern is sizing a quad to the texture's aspect ratio so sprites aren't
stretched:

```cpp
float aspect = (float)tex->GetWidth() / (float)tex->GetHeight();
float height = 1.0f;                       // desired height in world units
r.DrawQuad(pos, { height * aspect, height }, tex);
```

### Render targets

A `Framebuffer`'s colour attachments are textures. Draw into one with
`Renderer::SetRenderTarget(framebuffer)` (back to the window with `ResetRenderTarget()`), or
render a whole scene into it with `SceneRenderer::Render(scene, framebuffer)`
([Rendering into a texture](scenes-and-ecs.md#rendering-into-a-texture)), then draw
`framebuffer->GetAttachment(0)` like any texture. Give it an RGBA8 colour attachment, the format
`ReadPixels` reads, and depth if 3D draws into it. Its formats needn't match the window's (most
Linux drivers give the window BGRA8): `Renderer2D` and every `Material` build a pipeline for the
formats of the target they draw into. A render target's first row is the **top** of its picture, the opposite of an image loaded from a file, so
a quad shows it upright with a negative height:

```cpp
Framebuffer* target = Framebuffer::Create(FramebufferParams()
    .SetWidth(640).SetHeight(360)
    .SetEnableDepth(true)
    .AddAttachment({ TextureFormat::RGBA8_UNORM }));
...
r.DrawQuad({ 0.0f, 0.0f }, { 6.4f, -3.6f }, target->GetAttachment(0));
```

### Reading a texture back (v0.8.3)

`Texture::ReadPixels(done)` copies an RGBA8 texture back to the CPU and hands `done` a
`TexturePixels { Width, Height, Data }` (8-bit RGBA, rows in the texture's own order).
`Texture::SaveToFile(path, done)` writes one as a PNG (or BMP, TGA, JPEG by extension), and
`FileSystem::WriteImage(path, width, height, channels, pixels, flipVertically)` writes pixels you
already have.

```cpp
SceneRenderer& scenes = Application::Get().GetSceneRenderer();
scenes.Render(*iconScene, iconTarget);                       // this frame's draw...
iconTarget->GetAttachment(0)->SaveToFile("icons/sword.png", [](bool saved)
{
    DE_INFO("icon {}", saved ? "saved" : "failed");          // ...arrives next frame
});
```

- **When.** From `OnUpdate` or `OnUIRender` the copy follows the draws recorded so far, and
  `Renderer2D` and `Renderer3D` record theirs at `EndScene`, so read back after it. `done` then
  runs on the main thread at the start of the next frame, before its command list opens: it may
  read back or create textures, not draw. From `OnAttach`, or in a frame the app skips while
  minimized, `done` runs before the call returns; from an event handler or a post-execution
  callback, at the next frame's start. Capture nothing in `done` that may be freed meanwhile.
- **Paused and skipped frames.** A paused app (unfocused, `UpdateInBackground` off) has no next
  frame until it resumes, so `done` waits that long. A frame that renders nothing
  (`Renderer::IsFrameSkipped()`) reads back what the texture last held.
- **Which way up.** `ReadPixels` gives rows as the texture stores them: a render target's first
  row is the top of its picture, an image `CreateFromFile` loaded is stored bottom row first.
  `SaveToFile` writes either so the file looks right: a render target as it is, any other texture
  flipped back the way its file held it.
- RGBA8 2D textures only (`TextureFormat::RGBA`, `RGB` and `RGBA8_UNORM`); anything else logs an
  error and hands `done` empty pixels. The window's own swap chain has no texture to read.

## Batching & tips

- Quads, circles, and text each batch separately, so `EndScene` draws every quad, then
  every circle, then all text, whatever order you called them in. `Flush()` (v0.8.3) draws
  what has been submitted so far, so what comes after lands on top of it: call it between a
  text and the panel that must cover it. A scene does this itself to keep sprites, circles and
  text in z order ([Drawing order](scenes-and-ecs.md#drawing-order)).
- The renderer **auto-batches**: `MaxQuads` (default **2000**) is the size of a *single*
  batch, not a per-frame limit — when a batch fills up (or runs out of texture slots)
  it is flushed automatically and a fresh one begins, so you can draw any number of
  quads per frame and nothing is ever dropped.
- Tune the batch size via `ApplicationParams::Renderer2D` — bigger batches mean fewer
  draw calls but more memory per batch buffer:
  ```cpp
  params.Renderer2D.Capabilities.MaxQuads = 5000;
  ```
- Each batch has **32 texture slots**; introducing a 33rd texture forces a flush.
  Reusing the same texture is free, and solid-colour quads share a single built-in
  white texture.
- Keep one `BeginScene`/`EndScene` per camera per frame. If you need a second pass
  with a different camera (e.g. a screen-space HUD), open a second block after the
  first. A renderer runs at most `Renderer2D::k_MaxScenesPerFrame` (32) blocks a frame:
  on Vulkan, later ones draw with an earlier block's camera.

- **2D is never tone mapped** (v0.9). The [post chain](post-processing.md) takes the 3D pass alone:
  a scene's 2D overlay, and anything `Renderer2D` draws after `PostProcessStack::End`, goes into the
  target as it always did, so HUD colours stay exactly what you pass.

`r.GetViewportSize()` returns the current framebuffer size as a `glm::vec2`.
`r.GetOutput()` returns `nullptr`: `Renderer2D` draws straight into the swap chain,
whose image is not a `Texture` you can sample (before v0.6.3 the call read out of bounds).

---

Next: [Scenes & ECS](scenes-and-ecs.md) — manage your game objects as entities
instead of drawing everything by hand.
