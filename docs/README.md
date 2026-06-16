# DingoEngine Documentation

DingoEngine is a C++20 game engine with a Vulkan-backed rendering pipeline (via
[NVRHI](https://github.com/NVIDIAGameWorks/nvrhi)), a layered application
architecture, a batched 2D renderer, and — as of v0.3 — an
[EnTT](https://github.com/skypjack/entt)-backed scene/entity-component system.

These guides cover how to **use** the engine from your own game code, whether you
link against a prebuilt release package (`.lib` + headers) or integrate the
engine from source.

## Guides

| Guide | What it covers |
|---|---|
| [Getting Started](getting-started.md) | Prerequisites, what's in a release package, project setup (include dirs, link libraries, defines), and a minimal window app. |
| [Application & Layers](application-and-layers.md) | The entry point, `Application` lifecycle, the `Layer` stack, input, and events. |
| [2D Rendering](rendering-2d.md) | `Renderer2D` (quads, circles, MSDF text), the camera/projection model, textures, and fonts. |
| [Scenes & ECS](scenes-and-ecs.md) | The v0.3 scene system — `Scene`, `Entity`, components, writing systems, and `SceneManager` for multi-scene games. |

## A 30-second tour

```cpp
#include <DingoEngine/EntryPoint.h>   // include in exactly ONE .cpp — it defines main()
#include <DingoEngine.h>

using namespace Dingo;

class GameLayer : public Layer
{
public:
    GameLayer() : Layer("Game") {}

    void OnUpdate(float dt) override
    {
        Renderer2D& r = Application::Get().GetRenderer2D();
        r.BeginScene(m_Camera);
        r.Clear({ 0.1f, 0.1f, 0.12f, 1.0f });
        r.DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.9f, 0.3f, 0.3f, 1.0f });
        r.EndScene();
    }

private:
    glm::mat4 m_Camera = glm::ortho(-8.0f, 8.0f, -4.5f, 4.5f, -1.0f, 1.0f);
};

class Game : public Application
{
public:
    Game(const ApplicationParams& p) : Application(p) {}
    void OnInitialize() override { PushLayer(new GameLayer()); }
};

Application* Dingo::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationParams params{ .CommandLineArgs = args };
    params.Window.Title = "My Game";
    Game* app = new Game(params);
    app->Initialize();
    return app;
}
```

See [Getting Started](getting-started.md) for the full setup, then
[Scenes & ECS](scenes-and-ecs.md) for the recommended way to structure a real game.

## Conventions used in these docs

- Everything lives in the `Dingo` namespace.
- World units, not pixels: positions and sizes you pass to `Renderer2D` are in the
  coordinate space defined by your camera matrix. A quad's *position is its center*.
- Resources (`Texture`, `Font`) are created with `Create*()` factories that return
  raw pointers; you own them and call `Destroy()` when done.
