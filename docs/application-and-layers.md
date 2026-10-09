# Application & Layers

This guide covers the backbone of every DingoEngine program: the entry point, the
`Application` object, the `Layer` stack, input polling, and the event system.

## The entry point

You do **not** write `main()`. The engine provides it in
`<DingoEngine/EntryPoint.h>` — include that header in **exactly one** translation
unit. It calls a factory function you implement:

```cpp
#include <DingoEngine/EntryPoint.h>
#include <DingoEngine.h>

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
    Dingo::ApplicationParams params{ .CommandLineArgs = args };
    // ...configure params...
    auto* app = new MyApp(params);
    app->Initialize();   // REQUIRED before returning
    return app;
}
```

The engine's `main()` runs a small loop: it creates your application, runs it, and
— if a graphics-API restart was requested via `Application::RequestRestart()` —
recreates it. Return a heap-allocated `Application*`; the engine deletes it.

## ApplicationParams

`CreateApplication` configures the app through an `ApplicationParams` struct:

```cpp
ApplicationParams params{
    .CommandLineArgs = args,
    .Window = {
        .Title      = "My Game",
        .Width      = 1600,
        .Height     = 900,
        .VSync      = true,
        .Resizable  = true,       // live window resizing works on every back-end
        .Fullscreen = false,      // start in borderless fullscreen at desktop resolution
    },
    .Graphics = {
        .GraphicsAPI    = GraphicsAPI::Vulkan,   // Vulkan is the active back-end
        .FramesInFlight = 3,
    },
    .EnableUI = false,    // set true to get OnUIRender() callbacks
};
```

`.Graphics` may be left out: `GraphicsParams` defaults to `GraphicsAPI::Vulkan` with
`FramesInFlight = 3`. `GraphicsAPI::Headless` has no graphics context, so asking for it
(or for a back-end the platform lacks, such as DirectX on Linux) logs an error and stops
in `Application::Initialize`, in every configuration.

### Fullscreen

Besides `WindowParams::Fullscreen` for starting fullscreen, the mode can be switched
at runtime (borderless fullscreen at the desktop resolution of the monitor the window
is on; the windowed position/size is restored when leaving):

```cpp
Application::Get().GetWindow().ToggleFullscreen();   // e.g. bound to F11
Application::Get().GetWindow().SetFullscreen(true);  // or explicitly
bool fs = Application::Get().GetWindow().IsFullscreen();
```

The swap chain follows automatically via the normal resize path, on every graphics
back-end. `WindowResizeEvent` is forwarded to layers, so cameras can update their
aspect ratio there. Fullscreen is exclusive, so an alt-tab minimizes the window: see
[In the background](#in-the-background).

### Command-line arguments

`ApplicationCommandLineArgs::Get(name)` parses `--name` flags and `--name=value`
pairs, returning a `std::optional<std::string_view>`:

```cpp
GraphicsAPI ParseGraphicsAPI(const ApplicationCommandLineArgs& args)
{
    if (auto val = args.Get("graphics"))   // --graphics=vulkan
    {
        if (*val == "vulkan") return GraphicsAPI::Vulkan;
        if (*val == "dx11")   return GraphicsAPI::DirectX11;
        if (*val == "dx12")   return GraphicsAPI::DirectX12;
    }
    return GraphicsAPI::Vulkan;
}
```

On a machine without a GPU (a virtual machine, a build server), DirectX 11 and DirectX 12 run on
WARP, Windows' software rasterizer: DirectX 12 falls back to it with a warning when no hardware
adapter supports feature level 12_0 (since v0.8.2). Vulkan needs an installed driver; without one,
start-up stops with an error that names the missing instance extensions. WARP draws on the CPU, so
it is slow: fine for running a game, not for measuring it.

## The Application object

Subclass `Application` and override `OnInitialize()` to push your layers:

```cpp
class MyApp : public Application
{
public:
    MyApp(const ApplicationParams& params) : Application(params) {}

    void OnInitialize() override   // called after the window + renderer are ready
    {
        PushLayer(new GameLayer());
    }

    void OnDestroy() override {}   // once, when Run() returns - layers and renderer still live
};
```

Useful members (access the singleton anywhere with `Application::Get()`):

| Member | Purpose |
|---|---|
| `PushLayer(Layer*)` / `PushOverlay(Layer*)` | Add a layer (overlays update/draw on top). The app takes ownership. |
| `GetRenderer2D()` | The batched 2D renderer (see [2D Rendering](rendering-2d.md)). |
| `GetWindow()` | Window info — `GetWidth()`, `GetHeight()`. |
| `GetSwapChain()` | The active swap chain. |
| `Close()` | Request shutdown after the current frame. |
| `IsMinimized()` | True while the window is minimized: nothing renders (see [In the background](#in-the-background)). |
| `SetUpdateInBackground(bool)` / `GetUpdateInBackground()` | Keep updating while minimized or unfocused, or pause there (the default). |
| `RequestRestart(GraphicsAPI)` | Tear down and recreate the app on a different back-end. |
| `GetEngineVersion()` / `GetEngineBuildNumber()` | Packed engine version (decode with `DE_VERSION_MAJOR/MINOR/PATCH`). |

### The frame loop

Once running, each frame the `Application`:

1. `Input::Update()` — snapshots current vs. previous key/mouse state.
2. Polls window events (which fan out through `OnEvent`).
3. `Renderer::BeginFrame()`.
4. Calls `OnUpdate(deltaTime)` on every layer, bottom to top.
5. If ImGui is enabled, calls `OnUIRender()` on every layer.
6. `Renderer::EndFrame()` — presents.

`deltaTime` is seconds since the previous frame.

#### In the background

The window is in the background while it is minimized (a `WindowResizeEvent` with a zero width or
height; an alt-tab out of exclusive fullscreen minimizes it too) or unfocused. What happens then is
set by `ApplicationParams::UpdateInBackground`, which `Application::SetUpdateInBackground` changes
at runtime (v0.7.2; v0.7.0 and v0.7.1 always paused while minimized, and never while unfocused):

- **Off (the default): the app pauses.** No `OnUpdate`, no `OnUIRender`, nothing renders, and
  `AssetManager` loads and hot-reload wait; an unfocused window keeps showing its last frame. The
  loop sleeps on window events, the paused time is left out of the `deltaTime`s after it, and audio
  keeps playing. The first frame runs even without focus, so a window that opens unfocused is not
  left blank.
- **On: layers keep updating**, for a game that must not stop there, such as one that pumps a
  network or a simulation in `OnUpdate`. They get the real `deltaTime`, and `AssetManager` carries
  on. An unfocused window renders as usual. A minimized one has no swap-chain image to render into,
  so its frames update but render nothing:
  - The loop waits on window events between updates instead of spinning, aiming at 60 a second;
    with Windows' default 15.6 ms timer it gets about 35.
  - Step 3 becomes `Renderer::SkipFrame()`, and steps 5–6 are skipped: no `OnUIRender`, no debug
    window.
  - `Renderer` uploads, clears and draws, `Renderer2D` and `Renderer3D` scenes and
    `SceneRenderer::Render` are no-ops, so a layer that renders inside `OnUpdate` needs no guard of
    its own. A dropped `Renderer::Upload` is not redone after the restore, so data written once
    belongs in a `DirectUpload` buffer. Code that records into `Renderer::GetCommandList()` itself,
    or calls `Renderer::Begin`/`Close`/`Execute`, must check `Renderer::IsFrameSkipped()` first.
  - `Window::GetWidth()`/`GetHeight()` read 0, so an aspect ratio worked out from them every frame
    divides by zero. `Window::GetAspectRatio()` keeps the last one (since v0.8.3; before, it returned
    NaN), and `Renderer2D::GetViewportSize()` keeps the swap chain's size.
  - The restore logs how many updates ran meanwhile.

Either way, every key and button edge reaches exactly one `OnUpdate`. After a pause, the first
`OnUpdate` sees what changed during it, gamepads included; the scroll and cursor motion an inactive
window collected meanwhile are dropped.

## Layers

A `Layer` is where your code lives. Subclass it and override the hooks you need:

```cpp
class GameLayer : public Layer
{
public:
    GameLayer() : Layer("Game Layer") {}   // pass a debug name to the base ctor

    void OnAttach() override {}                 // pushed onto the stack
    void OnDetach() override {}                 // removed / app shutting down
    void OnUpdate(float deltaTime) override {}  // once per frame
    void OnEvent(Event& e) override {}          // input/window events
    void OnUIRender() override {}            // only if EnableUI == true
};
```

Layers update in push order; overlays sit on top. Multiple layers are handy for
separating, say, gameplay from a debug HUD.

## Input

Poll input anywhere (typically in `OnUpdate`) via the static `Input` class.

All state is a frame-coherent snapshot taken once per frame. The naming follows
the common convention and applies uniformly to keys, mouse buttons and gamepad
buttons (reworked in v0.5.1 — earlier versions had `Pressed`/`Down` swapped):

| Function | Meaning |
|---|---|
| `Is...Pressed(x)` | **Edge** — true only on the frame it became pressed ("just pressed"). |
| `Is...Down(x)` | **Held** — true every frame it is down. |
| `Is...Released(x)` | **Edge** — true only on the frame it was let go. |
| `Is...Up(x)` | Not held. |

Use `Down` for continuous actions (movement) and `Pressed` for one-shot actions
(jump, shoot, confirm).

Mouse extras: `GetMousePosition()` (window pixels, top-left origin, +Y down),
`GetMouseDelta()` (movement since last frame) and `GetMouseScrollDelta()`
(wheel movement this frame, +Y = up).

```cpp
void OnUpdate(float dt) override
{
    // Held: move while the key is down.
    if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  m_X -= speed * dt;
    if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) m_X += speed * dt;

    // Edge: fire once per press.
    if (Input::IsKeyPressed(Key::Space))
        Fire();

    if (Input::IsMouseButtonPressed(Button::Left))
        Click();
}
```

Key codes live in `Key::` (`Key::Space`, `Key::Escape`, `Key::A`–`Key::Z`,
`Key::Left/Right/Up/Down`, `Key::Enter`, …) and mouse buttons in `Button::`
(`Button::Left`, `Button::Right`, `Button::Middle`).

For "press any key" screens, or to notice that the player switched from a gamepad back
to keyboard and mouse, use `IsAnyKeyPressed()` / `IsAnyKeyDown()` /
`IsAnyMouseButtonPressed()` / `IsAnyMouseButtonDown()` (v0.6.2).

### Cursor modes (v0.6.2)

`Input::SetCursorMode` controls the cursor for the whole window:

| Mode | Behaviour |
|---|---|
| `CursorMode::Normal` | Visible and free (default). |
| `CursorMode::Hidden` | Invisible over the window, still free to leave it. For a custom crosshair or gamepad play. |
| `CursorMode::Locked` | Invisible and confined, with unbounded motion. For mouse-look: read `GetMouseDelta()`. |

```cpp
void Capture() { Input::SetCursorMode(CursorMode::Locked); }
void Release() { Input::SetCursorMode(CursorMode::Normal); }

void OnUpdate(float dt) override
{
    if (Input::GetCursorMode() == CursorMode::Locked)
    {
        const glm::vec2 look = Input::GetMouseDelta() * m_Sensitivity;
        m_Yaw -= look.x;
        m_Pitch = glm::clamp(m_Pitch - look.y, -89.0f, 89.0f);
    }
}
```

- Changing the mode moves the cursor, so `GetMouseDelta()` reads zero for the next
  2 frames (also after a Locked window regains focus). Capturing never kicks the
  camera, and you don't need to skip frames yourself.
- While `Locked`, ImGui ignores the mouse, so the invisible cursor can't click debug
  widgets. `GetMousePosition()` is an unbounded virtual position until you unlock.
- While `Locked`, raw mouse motion (no OS pointer acceleration) is on wherever the
  platform supports it. Toggle it with `SetRawMouseMotion(bool)`, and query it with
  `IsRawMouseMotionSupported()`.
- On alt-tab the OS frees the cursor, and it is locked again when the window
  regains focus. The mode itself never changes on its own. If your game pauses on
  focus loss, handle `WindowFocusEvent` (see [Events](#events)) and set `Normal`
  there, or the pause menu comes back with a locked cursor.
- The debug window's **F5** Input tab shows the current mode, focus and raw-motion
  state. The test app's **Cursor Test** (`--test=Cursor`) lets you try all three.

### Gamepads (v0.5.1)

Controllers are polled every frame; any device GLFW recognises as a gamepad
(XInput pads, DualShock/DualSense, most USB/Bluetooth pads) works out of the
box. Up to 16 pads are tracked; every call takes an optional gamepad index
defaulting to `0` (the first pad).

```cpp
if (Input::IsGamepadConnected())
{
    glm::vec2 stick = Input::GetGamepadLeftStick();   // radial deadzone, +Y down
    m_Velocity = glm::vec3(stick.x, 0.0f, stick.y) * speed;

    if (Input::IsGamepadButtonPressed(GamepadButton::A))
        Jump();

    float aim = Input::GetGamepadAxis(GamepadAxis::RightTrigger); // 0..1
}
```

- Buttons (`GamepadButton::`): `A/B/X/Y` (with `Cross/Circle/Square/Triangle`
  aliases), `LeftBumper/RightBumper`, `Back/Start/Guide`,
  `LeftThumb/RightThumb`, `DPadUp/Down/Left/Right` — same
  `Pressed/Down/Released` split as keys.
- Axes (`GamepadAxis::`): `LeftX/LeftY/RightX/RightY` in [-1, 1] and
  `LeftTrigger/RightTrigger` remapped to [0, 1]. `GetGamepadAxis` applies the
  deadzone (default 0.15, tune via `SetGamepadDeadzone`); `GetGamepadAxisRaw`
  does not. `GetGamepadLeftStick/RightStick` return a deadzone-filtered
  `glm::vec2`.
- Connection changes arrive as `GamepadConnectedEvent` /
  `GamepadDisconnectedEvent` (see Events below), which carry the slot id plus
  the detected `GetGamepadType()` and `GetGamepadName()` — ready to use inside
  the handler. These fire only for changes after startup — a pad already
  plugged in at launch produces no event, so check `IsGamepadConnected` for
  the initial state. The same `Input::GetGamepadName`/`GetGamepadType`
  (`Xbox` / `PlayStation` / `Nintendo` / `Steam` / `Unknown`, detected from
  the USB vendor id with a name fallback) cover polling, e.g. for
  button-prompt glyphs.

**Rumble (v0.8.3).** `Input::SetGamepadRumble(lowFrequency, highFrequency, seconds, gamepad)`
runs the pad's two motors, each from 0 to 1 (`lowFrequency` the heavy left one, `highFrequency`
the light right one), for `seconds`; setting it again replaces the last, and `StopGamepadRumble`
or 0 seconds stops it. It stops on time even while the app is paused in the background, and when
the app closes.

```cpp
void Player::OnHit()
{
    Input::SetGamepadRumble(0.6f, 0.3f, 0.25f);   // a short thump on pad 0
}
```

GLFW's gamepad API is input only, so rumble goes through XInput: Xbox-compatible pads on Windows.
On any other pad, or platform, it does nothing and returns false;
`Input::IsGamepadRumbleSupported(gamepad)` says which, e.g. to hide a VIBRATION setting. The F5
Input tab has a button to try each motor.

## Events

Beyond polling, layers receive discrete events through `OnEvent`. Events propagate
**top-down** (overlays first); setting `Handled` stops further propagation. Use an
`EventDispatcher` to route by type:

```cpp
void OnEvent(Event& e) override
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(OnResize));
    dispatcher.Dispatch<KeyPressedEvent>(DE_BIND_EVENT_FN(OnKeyPressed));
}

// Return true to mark the event handled (stops it reaching lower layers).
bool OnResize(WindowResizeEvent& e)
{
    RebuildCamera(e.GetWidth(), e.GetHeight());
    return false;   // let others see it too
}

bool OnKeyPressed(KeyPressedEvent& e)
{
    if (e.GetKeyCode() == Key::P) TogglePause();
    return false;
}
```

`DE_BIND_EVENT_FN(fn)` wraps a member function as the callback. Event types include
`WindowCloseEvent`, `WindowResizeEvent`, `WindowFocusEvent` (v0.6.2), `KeyPressedEvent`,
`KeyReleasedEvent`, `MouseButtonPressedEvent`, and `MouseButtonReleasedEvent`.

`WindowFocusEvent::IsFocused()` tells you whether the window gained or lost focus, which
is the natural place to auto-pause and release a locked cursor. To poll instead, use
`Application::Get().GetWindow().IsFocused()`.

```cpp
bool OnFocus(WindowFocusEvent& e)
{
    if (!e.IsFocused() && m_State == State::Playing)
    {
        Input::SetCursorMode(CursorMode::Normal);
        Pause();
    }
    return false;
}
```

> For most gameplay, polling with `Input` is simpler than handling key events. Reach
> for events when you need the exact press/release moment, repeat counts
> (`KeyPressedEvent::GetRepeatCount()`), or window-level notifications like resize.

---

Next: [2D Rendering](rendering-2d.md).
