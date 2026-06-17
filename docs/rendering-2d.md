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
world units, and a `TextParameters { Color, Kerning, LineSpacing }`. The position is
the **left baseline-ish origin** of the text (it grows to the right).

To center text, offset by half its measured width:

```cpp
float w = font->GetStringWidth(text, size);
r.DrawText(text, font, { centerX - w * 0.5f, y }, size, { .Color = color });
```

`Font::GetStringWidth(text, size)` and `Font::GetBoundingBox(text, size)` let you
measure and lay out text. Call `font->Destroy()` in `OnDetach`.

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

## Batching & tips

- Quads, circles, and text each batch separately and flush in `EndScene`. The
  renderer **auto-batches**: `MaxQuads` (default **2000**) is the size of a *single*
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
  first.

`r.GetViewportSize()` returns the current framebuffer size as a `glm::vec2`, and
`r.GetOutput()` returns the rendered colour texture if you need it.

---

Next: [Scenes & ECS](scenes-and-ecs.md) — manage your game objects as entities
instead of drawing everything by hand.
