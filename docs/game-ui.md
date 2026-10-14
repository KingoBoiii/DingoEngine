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

## Widgets

Widgets are declared like boxes and answer in the same call. They sit inside a root (or any box),
take their look from the current style (see [Styles and themes](#styles-and-themes)), and return a `UIInteraction`.

| Call | |
|---|---|
| `Button(label, params)` | `Clicked` on the frame the button is released with the pointer still over it. |
| `Toggle(label, &bool, params)` | Flips the bool on a click; `Changed` that frame. |
| `Slider(label, &float, min, max, params)` | Drag the track; the value is clamped (and snapped to `Step`) on every call. `ShowValue` prints it. |
| `SliderInt(label, &int, min, max, params)` | The same, whole numbers. |
| `ProgressBar(id, value, params)` | A 0 to 1 bar; takes no input. |
| `Selectable(label, selected, params)` | A row that is `Clicked` to pick it; you keep the selection. |
| `BeginScrollArea(label, params)` / `EndScrollArea()` | A clipped column that scrolls with the wheel (and by dragging with `DragScroll`); `BeginScrollArea` returns `true`, and every `true` gets its `EndScrollArea`. |

`UIInteraction` has `Hovered`, `Pressed` (the press frame), `Down` (held), `Clicked`, `Changed`
(the value was changed by the player this call) and `Focused` (reserved for keyboard focus; always false for now).
A disabled widget (`UIWidgetParams::Enabled = false`) is drawn dimmed and reports nothing.

```cpp
DingoUI::BeginBox(UIBoxParams().SetDirection(UIDirection::Column).SetGap(8.0f));
if (DingoUI::Button("Play").Clicked)
    StartGame();
DingoUI::Toggle("Music", &m_Music);
DingoUI::Slider("Volume", &m_Volume, 0.0f, 1.0f);
DingoUI::EndBox();
```

### Ids

A widget's state (hover, held, a scroll offset) is kept by id: the hash of its label, the ids of the
boxes around it and the `PushId` stack. Two widgets with the same id in one frame share the state
and warn once (Debug builds); fix it with a suffix after `##`, which is hashed but not shown
(`"Delete##row3"`), or with `PushId(i)` / `PopId()` around a loop.
State of a widget that isn't declared for a couple of frames is dropped.

## Styles and themes

A `UIStyle` holds the look of the widgets: text font, size and colour, a fill per state (`Background`,
`HoverColor`, `PressedColor`, `DisabledColor`), the accent used by switched-on toggles, filled
slider parts and selected rows, track, handle, progress and scroll-bar colours, border, corner
radius, padding, gaps and the sizes of toggles, handles and bars. All sizes are design pixels, so a
root's scale applies. `UIStyle::Default()` (what a default-constructed style holds) is the engine's
theme; the setters chain.

Three ways to change it:

- **The theme**: `DingoUIParams::Style` at start-up, or `DingoUI::SetTheme(style)` later. It is the
  style when nothing is pushed.
- **A stretch of a frame**: `PushStyle(style)` ... `PopStyle()`. Styles nest, and what a widget uses is
  decided when it is declared. A push left open is dropped, with one warning, at the end of the frame.
  `GetStyle()` returns the current one.
- **One widget**: `UIWidgetParams::SetStyle(&style)` (also on `UISliderParams`, `UIScrollParams`).

`TextFont` null falls back to `DingoUIParams::DefaultFont` / `SetDefaultFont`. Plain `Text` keeps its
own size and colour but takes the style's font when its params name none.

`BeginPanel(params)` / `EndPanel()` open a box dressed from the style: `PanelColor` (or the `Panel`
9-slice), border, corner radius and `PanelPadding`, for whatever the box's own params leave at their
default.

```cpp
UIStyle warm = UIStyle::Default().SetBackground({ 0.55f, 0.28f, 0.12f, 1 }).SetCornerRadius(12.0f);
DingoUI::PushStyle(warm);
DingoUI::BeginPanel();
if (DingoUI::Button("Play").Clicked) StartGame();
DingoUI::EndPanel();
DingoUI::PopStyle();
```

### 9-slice

A `UINineSlice` cuts a picture by four borders (`Borders`: left, top, right, bottom, in texture
pixels as the picture is displayed) and draws nine quads: the corners keep their size, the edges
stretch along one axis, the centre along both. It is one more command in the same vertex stream, so a
panel made of 9-slices is still one draw call. The drawn border is `Borders * Scale * the root's
scale`, rounded to whole pixels, and shrinks if two opposite borders no longer fit. `FillCenter` off
leaves a frame; `PointFilter` samples with the nearest filter for pixel art; `Tint` multiplies the
picture. UVs follow `Image`'s convention (a picture loaded with `Texture::CreateFromFile` stands upright).

Use it on any box (`UIBoxParams::SetNineSlice`, drawn over `Background` and under the children) or in a style:

- `Surface` is the face of a `Button` and of a selected `Selectable`. With an image it replaces
  `Background`; `SurfaceHover`, `SurfacePressed` and `SurfaceDisabled` replace it per state, and a state
  with no image draws `Surface` lighter (hover), darker (pressed) or dimmed (disabled).
- `Panel` is the face of `BeginPanel`.

Toggles, sliders, progress bars and scroll bars stay colour-only. Tiling the centre is not supported.

## Pointer, hit-testing and the input you keep

The UI answers from **the last frame's layout**: a press hits what was on screen when the player
pressed. A widget that is new this frame (a popup that opens on the same click) ignores that press, so a
click can't go through to a button that appears under it. Where roots overlap, the root on the higher
`Layer` (then declared later) wins; inside it the deepest widget does. A `Modal` root
(`UIRootParams::SetModal`) takes everything on the screen, also where it draws nothing.

When the pointer is over a part of a blocking root that draws something, a press or scroll there is
consumed: `Input::IsMouseButtonPressed/Down` and `Input::GetScroll` read as not happening for the game,
and a consumed button stays up until the player releases it, so a drag that began on the UI
never turns into a game click on the way out. A root with `BlocksInput = false` never consumes.

- `DingoUI::IsPointerOverUI()`: the pointer is over a blocking element (or a modal is up).
- `DingoUI::IsCapturingInput()`: the UI holds the pointer: a press it took is still held, or a modal
  is up. Keep game drag-and-drop and camera input off while it is `true`.

## Keyboard and gamepad focus

Menus work without a mouse. The arrow keys, a pad's D-pad and its left stick move the focus to the
nearest focusable widget in that direction (measured on last frame's layout); holding a direction
repeats it after `UINavParams::RepeatDelay`. `Tab` (`Shift+Tab`) steps through the widgets in
declaration order. Enter, Space or pad A accepts: a Button, Selectable or Toggle reports `Clicked`,
and a focused Slider steps with Left and Right instead of moving the focus. Escape or pad B is
reported by `DingoUI::IsBackPressed()` for the frame it went down.

- Only roots that block input and have `Navigable` set (the default) take part, so a HUD that
  does not block input never takes the arrow keys from the game. A `Modal` root confines the focus
  to itself; closing it gives the focus back to the widget it interrupted.
- The first widget with `DefaultFocus` set the frame it appears gets the focus; without one the
  first widget does. A focused widget that vanishes hands the focus to its nearest neighbour.
- `DingoUI::SetFocus(label)` focuses a widget from code. Call it in the container the widget is
  declared in, with the same label, and it takes effect next frame.
- The ring (`UIStyle::FocusedColor`, `FocusWidth`) and `UIInteraction::Focused` show only while the
  player navigates: moving the mouse hides them (`DingoUI::IsFocusVisible()`), and the first key or
  pad press afterwards only brings the ring back without moving it.
- While a root with focusable widgets is up, the keys and pad buttons the UI acted on read as not
  pressed in `Input`, so a pause key bound to Escape does not also fire; use `IsBackPressed()`.
- `DingoUIParams::Nav` (`UINavParams`) turns navigation off or rebinds the keys, the pad buttons and
  the repeat timing. With `KeyboardAvailable` off (a capturing ImGui) the keys are ignored and the pad
  still works. `UIWidgetParams::SetFocusable(false)` skips a widget.

```cpp
DingoUI::BeginRoot("menu", UIRootParams().SetAnchor(UIAnchor::Center).SetModal(true));
if (DingoUI::Button("Resume", UIWidgetParams().SetDefaultFocus()).Clicked) Resume();
if (DingoUI::Button("Quit").Clicked) Quit();
DingoUI::EndRoot();
if (DingoUI::IsBackPressed()) Resume();
```

## Tweens and transitions

`DingoUI::Tween(id, target, duration, ease)` returns a value that eases toward `target` over
`duration` seconds; `TweenColor` does the same for a colour. The first call with an id returns the
target at once; when the target changes, the value restarts from where it was. A tween not declared
for two frames is forgotten. `DingoUI::Ease(ease, t)` gives the bare curve (`Linear`, `QuadIn`,
`QuadOut`, `QuadInOut`, `CubicOut`, `BackOut` and others in `UIEase`), exactly 0 at 0 and 1 at 1.

```cpp
float lift = DingoUI::Tween("menu_lift", open ? 0.0f : 40.0f, 0.25f);
```

Widgets blend their hover and pressed colours when the style asks for it: `UIStyle::SetTransition(0.15f)`.
The default is 0, so colours change at once. A disabled widget never animates.

A root can animate in and out: `UIRootParams::SetTransition(UITransition::Fade(0.3f).WithSlide({ -40.0f, 0.0f }))`.
The root plays in on its first declared frame. When the game stops declaring it, the root plays out
from the last frame it drew: it is not laid out again, takes no pointer or focus input and does not
block the widgets under it. Fade, slide (design pixels) and scale (about the root's centre) combine
and are applied to the drawn vertices, so a transitioning UI is still one draw call, and the frame
after the animation draws from the cache again.

The animations run on the real frame time (`UIInputState::DeltaTime`). The engine has no time scale,
so they keep running while a scene is paused.

## World-space UI

A root can hang on a point in the world, for a health bar over a head or a name over a door.
`BeginWorldRoot` projects the point to a pixel of the UI's target, puts the root's `Pivot` on it
(bottom-centre by default) and otherwise behaves like `BeginRoot`: lay out boxes, text and widgets
inside, then `EndRoot`.

```cpp
for (size_t i = 0; i < enemies.size(); ++i)
{
    if (DingoUI::BeginWorldRoot("hp" + std::to_string(i), enemies[i], { 0.0f, 1.8f, 0.0f },
            UIWorldRootParams().SetDistanceScale(12.0f).SetMaxDistance(40.0f, 8.0f)))
    {
        DingoUI::ProgressBar("hp", health, UIWidgetParams().SetSize(UISize::Fixed(60.0f), UISize::Fixed(6.0f)));
        DingoUI::EndRoot();
    }
}
```

The point comes from a position (`BeginWorldRoot(name, camera, position)` with a `UIWorldCamera` from a
view and projection or a `PerspectiveCamera`; `BeginWorldRoot(name, scene, position)` through the scene's
primary camera) or from an entity (its world position, through the hierarchy, plus an offset, seen by
its scene's primary camera). Give each root its own name.

- A root is not declared at all, and `BeginWorldRoot` returns `false` (no `EndRoot`), when the point is
  behind the camera, beyond `MaxDistance`, or outside the screen by more than `CullMargin` pixels.
  Nothing of it is laid out, drawn or hit-tested.
- The anchor pixel is rounded, so a moving point does not shimmer. `Offset` moves the root from it, in
  design pixels.
- `ReferenceDistance` scales the root by `ReferenceDistance / distance`, clamped to `MinScale` and
  `MaxScale`, on top of `Scale`. `FadeDistance` fades the root out over the last units before `MaxDistance`.
  The distance is the depth in front of the camera; an orthographic camera has none, so these do nothing.
- World roots draw below every `BeginRoot` root, farther ones first (`Layer` orders them first). They do
  not block the pointer unless `SetBlocksInput(true)`, and are not focus-navigable.
- Roots are positioned from the point of the frame they are declared in, so declare them after the
  entity has moved (an `OnUpdate` after the scene's own).
- There is no depth test against the scene, and no panel drawn onto a mesh; a world root is a screen-space
  overlay placed by a world point.

## Rules the frame follows

- Declare from `OnUpdate`; the layout and the draw happen after all layers have updated, and the UI
  draws over everything your layers drew to the swap chain.
- A `Begin...` that returns `false` declared nothing; its `End...` must not be called. Every `true`
  gets its `End...`. A mismatched call is ignored, and a `Begin` left open is closed when the frame
  ends; each warns once. The next frame starts clean.
- The same declarations in the same order give the same ids and the same layout, which widget
  state is keyed on. Give widgets distinct labels (or `PushId`) when their order can change.
- A UI that didn't change draws the same vertices as the last frame and uploads nothing; a menu of
  boxes, text and images with up to 32 textures is one draw call.
