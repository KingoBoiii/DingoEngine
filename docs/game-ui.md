# Game UI (DingoUI)

*(v1.0)*

DingoUI is the engine's game UI: menus, HUDs and dialogs declared in code, every frame, with
`DingoUI::` calls from a layer or a script. You describe a tree of boxes, text and images; the
engine sizes and places them (a small flexbox), draws all of them in one batch and tells your game
whether the pointer is over any of them. Nothing is created or destroyed; a frame that doesn't
declare an element doesn't have it.

It does nothing under `GraphicsAPI::Headless` or with `DingoUIParams::Enabled = false`: every call
returns at once (`BeginRoot` returns `false`). Dear ImGui stays the tool for debug panels; DingoUI is
for what the player sees.

```cpp
void MenuLayer::OnUpdate(float deltaTime)
{
    if (!DingoUI::BeginRoot("menu", UIRootParams()
            .SetAnchor(UIAnchor::Center)
            .SetScale(UIScale::ScaleWithScreen())
            .SetLayout(UIBoxParams()
                .SetWidth(UISize::Fixed(320.0f))
                .SetGap(8.0f).SetPadding(16.0f)
                .SetAlign(UIAlign::Stretch)
                .SetBackground({ 0.1f, 0.1f, 0.15f, 0.9f }).SetCornerRadius(8.0f))))
        return;

    DingoUI::Text("Main menu", UITextParams().SetSize(28.0f).SetAlign(UITextAlign::Center));
    for (const char* label : { "Continue", "Options", "Quit" })
    {
        DingoUI::BeginBox(UIBoxParams().SetPadding(8.0f).SetBackground({ 0.25f, 0.3f, 0.45f, 1.0f }));
        DingoUI::Text(label, UITextParams().SetAlign(UITextAlign::Center));
        DingoUI::EndBox();
    }
    DingoUI::EndRoot();
}
```

## The font

`Text` needs a font. Set it once, on `ApplicationParams::DingoUI.DefaultFont` if you have one
before the `Application` exists, or with `DingoUI::SetDefaultFont(font)` after you load it (the font
stays yours: unset it before you destroy it). `UITextParams::SetFont` overrides it per text. A text
with no usable font is skipped, with one warning in the log.

## Roots

`DingoUI::BeginRoot(name, params)` opens a root and `EndRoot()` closes it. A root is a box that is
pinned to the screen, and there is one open at a time. Boxes, text and images go inside it.

| `UIRootParams` | |
|---|---|
| `Anchor` | One of the nine screen points (`TopLeft` ... `BottomRight`), placed on the same point of the root; or `Stretch`, which makes the root fill the screen and ignores its `Width` and `Height`. |
| `Offset` | Moves the root from its anchor, in design pixels. |
| `Pivot` | The point of the root that goes on the anchor, `(0,0)` top left to `(1,1)` bottom right. Defaults to the anchor's own point, so a `BottomRight` root sits inside the corner. |
| `Scale` | See [Scaling](#scaling). |
| `Layer` | Roots draw by `Layer`, then in the order declared; a higher layer is in front. |
| `BlocksInput` | Default `true`: a press, scroll or hover over a part of the root that draws something belongs to the UI (`IsPointerOverUI`, `Handled` events). `false` for a HUD that must never take the mouse. |
| `Layout` | The root's own box (below). |

## Boxes and layout

A box lays its children out in a row or a column. `BeginBox(params)` / `EndBox()` nest as deep as you
need; the root's `Layout` is a box as well.

| `UIBoxParams` | |
|---|---|
| `Direction` | `Column` (default, top to bottom) or `Row` (left to right). |
| `Gap` | Space between children along `Direction`. |
| `Padding` | Left, top, right, bottom: `SetPadding(all)`, `SetPadding(horizontal, vertical)` or four values. |
| `Justify` | Children along `Direction`: `Start`, `Center`, `End`, `SpaceBetween`. |
| `Align` | Children across it: `Start`, `Center`, `End`, `Stretch` (children that fit or grow fill the box). |
| `Width`, `Height` | A `UISize` (below). |
| `Background`, `BorderColor`/`BorderWidth`, `CornerRadius` | The box's own drawing. |
| `ClipChildren` | Nothing of the children is drawn outside the box. |
| `Id` | A name that is unique among its siblings. |

A `UISize` is one of

- `UISize::Fit()`: as big as the content (the default);
- `UISize::Fixed(pixels)`;
- `UISize::Grow(weight)`: a share of what the siblings leave along the container's direction, by
  weight; across it, the whole container;
- `UISize::Percent(fraction)`: a fraction of the parent's content area (without padding) on that axis,

and `.SetMin(pixels)` / `.SetMax(pixels)` bound any of them. A grow child that reaches its maximum
gives the rest to the others.

## Text and images

`DingoUI::Text(string, params)` adds a line or a paragraph. With `Wrap` on (default) it breaks at the
width its parent gives it, or at `MaxWidth` when that is smaller, and the box around it gets taller.
`Width` sizes the element like any other box; `Align` aligns each line inside it. `Size` is the height
of a line, in design pixels. The string is copied: it needn't outlive the call.

`DingoUI::Image(texture, params)` adds a texture, `Fit` meaning its own size. `UVMin`/`UVMax` pick a
part of it, `Tint` multiplies the colour, and `PreserveAspect` (default) keeps the texture's shape
inside a box that has another one instead of stretching it. The texture must stay alive until the
frame has been drawn.

## Scaling

Sizes, gaps, paddings, offsets and font sizes are in **design pixels**; a root's `Scale` turns them
into screen pixels.

- `UIScale::ConstantPixel(factor)` (default, factor 1): one design pixel is `factor` screen pixels
  at any resolution.
- `UIScale::ScaleWithScreen(reference, match)`: lay the UI out for `reference` (default 1920x1080)
  and let it grow and shrink with the screen. `match` 0 follows the width ratio, 1 the height ratio,
  and values between blend them (the default 0.5 is the geometric mean), so a 960x540 screen with the
  default reference scales by 0.5.

### Window pixels or framebuffer pixels

The UI lays out and draws in the pixels of the **render target it draws to** (the swap chain's size,
the framebuffer's when a test sets one), so one design pixel at scale 1 is one pixel you can count
in a screenshot, also on a high-DPI display where the window is smaller than its framebuffer. The
pointer arrives in window coordinates and is converted by the ratio of the two, so
`IsPointerOverUI()` and the later hit-tests agree with what is on screen. A resize changes the scale
the next frame.

## Rules the frame follows

- Declare from `OnUpdate`; the layout and the draw happen after all layers have updated, and the UI
  draws over everything your layers drew to the swap chain.
- A `Begin...` that returns `false` declared nothing; its `End...` must not be called. Every `true`
  gets its `End...`. A mismatched call is ignored, and a `Begin` left open is closed when the frame
  ends; each warns once. The next frame starts clean.
- The same declarations in the same order give the same ids and the same layout, which the
  interaction state of later releases will key on. Give siblings an `Id` when their order can change.
- A UI that didn't change draws the same vertices as the last frame and uploads nothing; a menu of
  boxes, text and images with up to 32 textures is one draw call.
