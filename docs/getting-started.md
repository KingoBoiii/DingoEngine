# Getting Started

This guide gets a DingoEngine application compiling, linking, and rendering its
first frame.

There are two ways to use the engine:

- **[Integrate from source](#option-a--integrate-from-source-recommended)** — clone
  the repo, let Premake wire everything up, and add your game as a project. This is
  the most reliable path today because Premake links every dependency for you.
- **[Link a prebuilt package](#option-b--link-a-prebuilt-release-package)** — consume
  the `.lib` + headers produced by the release pipeline.

Either way, your *code* is identical — only the build setup differs.

## Prerequisites

| Requirement | Notes |
|---|---|
| Windows 10/11, x64 | Vulkan is the active back-end. |
| [Vulkan SDK](https://vulkan.lunarg.com/) | Install it and make sure `VULKAN_SDK` is set. The engine links **ShaderC** and **SPIRV-Cross** from the SDK and loads Vulkan at runtime. |
| C++20 toolchain | MSVC (Visual Studio 2022/2026, toolset v143+). |

---

## Option A — Integrate from source (recommended)

**1. Clone recursively** (the dependencies are submodules):

```bash
git clone --recursive https://github.com/KingoBoiii/DingoEngine.git
# or, if you forgot --recursive:
git submodule update --init --recursive
```

**2. Add your game project.** Create `examples/MyGame/premake5.lua` modeled on the
existing examples:

```lua
project "MyGame"
    kind "ConsoleApp"      -- WindowedApp in Distribution (see below)

    targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
    objdir    ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

    files { "src/**.h", "src/**.cpp" }

    includedirs {
        "%{wks.location}/include",   -- DingoEngine public headers
        "src",
        "%{IncludeDir.glm}",
        "%{IncludeDir.imgui}",
        "%{IncludeDir.entt}"          -- required: the Scene/ECS headers use EnTT
    }

    links { "DingoEngine" }           -- pulls in every transitive dependency

    filter "system:windows"
        systemversion "latest"
        -- NOMINMAX: the public headers transitively include <Windows.h>, whose
        -- min/max macros otherwise clobber std::min / std::max in your code.
        defines { "DE_PLATFORM_WINDOWS", "NOMINMAX" }

    filter "configurations:Debug"
        symbols "On"
        defines { "DE_DEBUG" }
    filter "configurations:Release"
        optimize "On"
        defines { "DE_RELEASE" }
    filter "configurations:Distribution"
        kind "WindowedApp"            -- no console window
        optimize "On"
        defines { "DE_DISTRIBUTION" }
```

Register it in the root `premake5.lua`:

```lua
group "Examples"
    include "examples/MyGame"
group ""
```

**3. Generate & build.** Regenerate the Visual Studio solution and build:

```bash
./vendor/premake/bin/premake5.exe vs2026
```

> The repo also has `Generate-Windows.bat`, but it ends in `PAUSE`; call `premake5`
> directly from scripts/CI.

Open `DingoEngine.slnx`, set `MyGame` as the startup project, and build. The engine
is a static lib, so the first build compiles it once; afterwards your game links
against it quickly.

---

## Option B — Link a prebuilt release package

Release builds are published as
`DingoEngine-<version>-<Config>-windows-x86_64.zip` (Config = `Debug`, `Release`, or
`Distribution`). Extract it; you get:

```
DingoEngine.lib            # the engine (static library)
include/                   # engine public headers  ← add as an include root
  DingoEngine.h            #   the umbrella header
  DingoEngine/...
glm/                       # GLM math headers       ← public dependency
imgui/imgui.h              # Dear ImGui header (for OnImGuiRender)
entt/                      # EnTT headers           ← public dependency (ECS)
assimp-vc145-mt[d].dll     # model-loader runtime   ← copy next to your .exe
```

### Compiler / linker settings

| Setting | Value |
|---|---|
| C++ standard | C++20 (`/std:c++20`) |
| Include dirs | `include`, `glm`, `imgui`, `entt` |
| Defines | `DE_PLATFORM_WINDOWS`, `NOMINMAX`, `GLM_FORCE_DEPTH_ZERO_TO_ONE` |
| Runtime library | **Must match the package config:** Debug → `/MDd`, Release/Distribution → `/MD` (the engine is built with the *dynamic* CRT). |
| Link | `DingoEngine.lib` + the libraries below |

Define `GLM_FORCE_DEPTH_ZERO_TO_ONE` so any projection matrices you build with GLM
use the same `[0,1]` depth convention as the engine's Vulkan pipeline. Optionally
define `DE_DEBUG` (Debug) / `DE_RELEASE` / `DE_DISTRIBUTION` to match the package —
this toggles engine asserts (`DE_ASSERT`) in your translation units.

**Libraries you must also link** (a static lib does not embed its dependencies):

- From the **Vulkan SDK** (`$(VULKAN_SDK)/Lib`): `vulkan-1.lib`, and ShaderC /
  SPIRV-Cross / SPIRV-Tools — Debug: `shaderc_combinedd.lib`,
  `spirv-cross-cored.lib`, `spirv-cross-glsld.lib`, `spirv-cross-hlsld.lib`,
  `SPIRV-Toolsd.lib`; Release: the same names without the trailing `d`.
- **Windows system libs**: `Ws2_32 Winmm Version Bcrypt d3d12 d3d11 dxgi dxguid d3dcompiler`.
- **assimp import lib**: `assimp-vc145-mt[d].lib` (pairs with the bundled DLL).

> **Heads-up — bundling gap.** The release ZIP currently contains only the engine
> static lib (plus the glm/imgui/entt headers and the assimp DLL). The engine's other
> static dependencies — **NVRHI, GLFW, spdlog, Dear ImGui, msdf-atlas-gen,
> FreeType** — and the assimp import lib are **not** included in the archive yet.
> Until they are packaged, the smoothest path is **Option A**, or copy those `.lib`
> files out of a local build at `build/bin/<Config>-windows-x86_64/<dependency>/`.

### Runtime

- Put `assimp-vc145-mt[d].dll` next to your executable.
- Run with the **working directory set to wherever your `assets/` folder lives** —
  the engine resolves relative asset paths (textures, fonts) from the current
  directory and writes its log file `Dingo.log` there.

---

## Your first application

Two pieces are required: an `Application` subclass and a `CreateApplication()` factory
that the engine's `main()` calls.

**`src/main.cpp`**

```cpp
#include <DingoEngine/EntryPoint.h>   // defines main(); include in exactly ONE .cpp
#include <DingoEngine.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace Dingo;

// A Layer is where your game logic lives. The Application updates layers in order
// every frame.
class ExampleLayer : public Layer
{
public:
    ExampleLayer() : Layer("Example") {}

    void OnAttach() override
    {
        // 16:9 orthographic camera, 9 world units tall, centered on the origin.
        m_Camera = glm::ortho(-8.0f, 8.0f, -4.5f, 4.5f, -1.0f, 1.0f);
    }

    void OnUpdate(float deltaTime) override
    {
        if (Input::IsKeyDown(Key::Escape))
            Application::Get().Close();

        Renderer2D& r = Application::Get().GetRenderer2D();

        r.BeginScene(m_Camera);
        r.Clear({ 0.10f, 0.10f, 0.12f, 1.0f });
        r.DrawQuad({ 0.0f, 0.0f }, { 1.5f, 1.5f }, { 0.9f, 0.3f, 0.3f, 1.0f });
        r.EndScene();
    }

private:
    glm::mat4 m_Camera{ 1.0f };
};

class ExampleApp : public Application
{
public:
    ExampleApp(const ApplicationParams& params) : Application(params) {}

    void OnInitialize() override
    {
        PushLayer(new ExampleLayer());
    }
};

Application* Dingo::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationParams params{ .CommandLineArgs = args };
    params.Window.Title = "My First Dingo App";
    params.Window.Width = 1280;
    params.Window.Height = 720;
    params.EnableImGui = false;

    ExampleApp* app = new ExampleApp(params);
    app->Initialize();   // must be called before returning
    return app;
}
```

Build and run — you should get a window with a dark background and a red square in
the middle. Press `Escape` to quit.

From here:

- [Application & Layers](application-and-layers.md) — lifecycle, input, and events in detail.
- [2D Rendering](rendering-2d.md) — textures, fonts, and the camera model.
- [Scenes & ECS](scenes-and-ecs.md) — the recommended way to structure a real game.
