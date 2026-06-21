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
        .Title     = "My Game",
        .Width     = 1600,
        .Height    = 900,
        .VSync     = true,
        .Resizable = false,
    },
    .Graphics = {
        .GraphicsAPI    = GraphicsAPI::Vulkan,   // Vulkan is the active back-end
        .FramesInFlight = 3,
    },
    .EnableUI = false,    // set true to get OnUIRender() callbacks
};
```

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

    void OnDestroy() override {}   // optional cleanup
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

> **Important — the naming is the opposite of most engines:**
>
> | Function | Meaning |
> |---|---|
> | `Input::IsKeyDown(key)` | **Edge-triggered** — true only on the frame the key is *first* pressed ("just pressed"). |
> | `Input::IsKeyPressed(key)` | **Held** — true every frame the key is down. |
>
> Use `IsKeyPressed` for continuous actions (movement) and `IsKeyDown` for one-shot
> actions (jump, shoot, confirm). The same split applies to
> `IsMouseButtonDown` (edge) vs. `IsMouseButtonPressed` (held).

```cpp
void OnUpdate(float dt) override
{
    // Held: move while the key is down.
    if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  m_X -= speed * dt;
    if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) m_X += speed * dt;

    // Edge: fire once per press.
    if (Input::IsKeyDown(Key::Space))
        Fire();

    if (Input::IsMouseButtonDown(Button::Left))
        Click();
}
```

Key codes live in `Key::` (`Key::Space`, `Key::Escape`, `Key::A`–`Key::Z`,
`Key::Left/Right/Up/Down`, `Key::Enter`, …) and mouse buttons in `Button::`
(`Button::Left`, `Button::Right`, `Button::Middle`).

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
`WindowCloseEvent`, `WindowResizeEvent`, `KeyPressedEvent`, `KeyReleasedEvent`,
`MouseButtonPressedEvent`, and `MouseButtonReleasedEvent`.

> For most gameplay, polling with `Input` is simpler than handling key events. Reach
> for events when you need the exact press/release moment, repeat counts
> (`KeyPressedEvent::GetRepeatCount()`), or window-level notifications like resize.

---

Next: [2D Rendering](rendering-2d.md).
