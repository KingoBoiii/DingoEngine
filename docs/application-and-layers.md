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
        .Mode       = WindowMode::Windowed,   // or BorderlessFullscreen, ExclusiveFullscreen
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

### Logs and crashes

The log goes to `Dingo.log` in the game's per-user folder, `GetUserDataDir(Name)/logs`:
`%LOCALAPPDATA%\<Name>\logs` on Windows and `~/.local/share/<Name>/logs` (or under
`$XDG_DATA_HOME`) on Linux. A game installed under Program Files can't write beside itself,
and a player's bug report needs last session's log, so each run keeps the four before it as
`Dingo.1.log` (the last run) to `Dingo.4.log`. A second instance running at the same time
writes `Dingo-<pid>.log` instead of moving a log that is still open (the newest four of those
are kept too). A rotation that can't finish, say because a log viewer holds `Dingo.2.log`,
moves nothing it can't and leaves last run's log as `Dingo-<pid>-previous.log`.

```cpp
ApplicationParams params{
    .CommandLineArgs = args,
    .Name = "MyGame",               // the per-user folder; empty = the executable's name
    .LogDirectory = "",             // empty = GetUserDataDir(Name)/logs
};
```

On any app's command line, `--log-here` writes the log into the working directory (where it
used to go) and `--log-dir=<path>` anywhere else. What is logged before the `Application`
exists is kept in memory and lands at the top of the file. `Log::GetFilePath()` names the
file of this run.

A crash leaves its report there too:

- **Linux**: SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT and SIGTRAP write a backtrace into
  `Dingo.log` and stderr, then end the process as the signal would (exit code 128 + the
  signal, core dump included). Every frame of the game carries `executable(+offset)`, which
  `addr2line -C -f -e <executable> <offset>` turns into a function and line given the same
  build with symbols (shared libraries' frames name their own functions). Under
  AddressSanitizer the engine leaves the signals to ASan's report.
- **Windows**: an unhandled exception (a stack overflow included), `abort()`, a pure virtual
  call or an invalid C runtime parameter writes `Dingo-<date>-<time>-<pid>.dmp` beside the log
  (the newest five are kept)
  and a line into it. Open the dump in Visual Studio or WinDbg with the build's PDBs. A
  Distribution build also shows a message box naming the folder.
- **Both**: an uncaught exception logs its `what()` before the report. A second thread that
  crashes while the first is reporting waits for the exit, so the report is the first crash's.

`FatalError(message)` (`DingoEngine/Core/FatalError.h`) is the way out of an error the game
can't go on from: it logs the message with the file and line it was called from, flushes the
log, shows a message box (Windows Distribution builds) and exits with code 1. In Distribution
builds a failed `DE_VERIFY` / `DE_CORE_VERIFY` ends there too, rather than breaking into a
debugger that isn't there. The test app's `--crash=segv|thread|abort|terminate|throw|stack|verify|fatal`
crashes on purpose, which is how CI checks all of this.

### Window modes

`WindowParams::Mode` sets the start mode, and the window switches at runtime between three:

| `WindowMode` | |
|---|---|
| `Windowed` | A normal window. `SetSize(w, h)` sets its client size. |
| `BorderlessFullscreen` | A fullscreen window at the monitor's desktop mode: the display mode never changes. |
| `ExclusiveFullscreen` | A fullscreen window that switches the monitor to a display mode. |

```cpp
Window& window = Application::Get().GetWindow();
window.ToggleFullscreen();                               // e.g. bound to F11: borderless or windowed
window.SetWindowMode(WindowMode::BorderlessFullscreen);
window.SetWindowMode(WindowMode::ExclusiveFullscreen, { 1920, 1080, 144 });
window.SetWindowMode(WindowMode::Windowed);              // back to the windowed position and size

for (const DisplayMode& mode : window.GetDisplayModes())   // largest first, for an options screen
    AddRow(std::format("{} x {} @ {} Hz", mode.Width, mode.Height, mode.RefreshRate));
```

Everything happens on the monitor the window is on. Exclusive fullscreen takes the listed mode
nearest the one asked for (`ChooseDisplayMode`: the size if listed, else the nearest; then the
refresh rate, 0 = the highest) and reports it through `GetFullscreenDisplayMode()`; an invalid mode
means the desktop's. `GetDesktopDisplayMode()` keeps reporting the monitor's own mode meanwhile.
`SetSize` while fullscreen changes the size the window returns to, and `GetWindowedWidth/Height`
read it. The swap chain follows through the ordinary resize path on every back-end, and
`WindowResizeEvent` reaches the layers, so cameras can update their aspect there. Both fullscreen
modes are a GLFW fullscreen window, which an alt-tab minimizes: see
[In the background](#in-the-background). A [`Settings`](saves-and-settings.md#settings) slot saves the
mode and starts the window in it. The test app's Display Mode Test (`--test=display`) walks every
mode and checks the window and swap-chain sizes; `--display-exclusive=next` or `=<W>x<H>[@<Hz>]`
makes its exclusive step change the display mode.

### VSync

`WindowParams::VSync` sets the start value; the window switches it at runtime:

```cpp
Window& window = Application::Get().GetWindow();
window.SetVSync(!window.IsVSync());   // e.g. a Settings menu row
```

`SetVSync` is safe from event handlers and layers alike. It goes through the same queue
as a resize: the swap chain takes the new flag after the next present, so while the window
is minimized or paused it waits for the first frame after the restore. D3D11 and D3D12 read it on every
present; Vulkan bakes it into the present mode, so it recreates the swap chain, which bumps
`SwapChain::GetResizeGeneration()` as a resize does. `IsVSync()` reports the requested value
at once. The F3 Engine tab has a VSync checkbox.

Limits:

- With VSync off, Vulkan picks Mailbox, else Immediate, else FIFO (the only mode every
  driver has), so on some drivers "off" stays capped.
- The D3D swap chains are made without `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`, so a windowed
  D3D app with VSync off may still be paced by the compositor.
- While an ImGui window is dragged outside the main one, its own swap chain stays vsynced
  and paces the whole frame, so VSync off has no effect.
- `RequestRestart` rebuilds the app, so the window starts from `WindowParams::VSync` again.

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
| `GetWindow()` | Window info — `GetWidth()`, `GetHeight()` — and runtime switches: `SetWindowMode`, `SetFullscreen`, `SetSize`, `SetVSync` (see [Window modes](#window-modes), [VSync](#vsync)). |
| `GetSaveDirectory()` | Where `SaveSlots` keeps its files (see [Saves & Settings](saves-and-settings.md)). |
| `GetSwapChain()` | The active swap chain. |
| `Close()` | Request shutdown after the current frame. |
| `IsMinimized()` | True while the window is minimized: nothing renders (see [In the background](#in-the-background)). |
| `SetUpdateInBackground(bool)` / `GetUpdateInBackground()` | Keep updating while minimized or unfocused, or pause there (the default). |
| `RequestRestart(GraphicsAPI)` | Tear down and recreate the app on a different back-end. |
| `GetEngineVersion()` / `GetEngineBuildNumber()` | Packed engine version (decode with `DE_VERSION_MAJOR/MINOR/PATCH`). |
| `GetName()` | `ApplicationParams::Name`, or the executable's name: the per-user folder logs go to (see [Logs and crashes](#logs-and-crashes)). |

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
