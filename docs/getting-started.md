# Getting Started

This guide gets a DingoEngine application compiling, linking, and rendering its
first frame.

There are two ways to use the engine:

- **[Integrate from source](#option-a--integrate-from-source-recommended)** — clone
  the repo, let Premake wire everything up, and add your game as a project. This is
  the most reliable path today because Premake links every dependency for you.
- **[Link a prebuilt package](#option-b--link-a-prebuilt-release-package)** — consume
  the static library + headers produced by the release pipeline (`.lib` on Windows,
  `.a` on Linux).

Either way, your *code* is identical — only the build setup differs.

## Prerequisites

| Requirement | Windows | Linux |
|---|---|---|
| OS | Windows 10/11, x64 | x86-64; verified on Ubuntu 24.04 (X11, or Wayland through XWayland) |
| [Vulkan SDK](https://vulkan.lunarg.com/) ≥ 1.4 | The installer; it sets `VULKAN_SDK` | LunarG's Linux tarball: `source <sdk>/setup-env.sh` sets `VULKAN_SDK`. Distro packages are too old. |
| C++20 toolchain | MSVC (Visual Studio 2022/2026, toolset v143+) | GCC ≥ 13 (the engine's `std::format` needs libstdc++ 13) |
| Packages | — | `build-essential cmake ninja-build python3 zlib1g-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` (Ubuntu/Debian names) |
| Runtime | A Vulkan 1.3 driver | `libvulkan1` and a Vulkan 1.3 driver |

The engine links **ShaderC** and **SPIRV-Cross** from the SDK and loads Vulkan at runtime.
Outside Distribution it enables the SDK's validation layer when it is installed, and warns
when it isn't.

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
        "%{IncludeDir.imgui}"
    }

    links { "DingoEngine" }

    filter "system:windows"
        systemversion "latest"
        -- NOMINMAX: the public headers transitively include <Windows.h>, whose
        -- min/max macros otherwise clobber std::min / std::max in your code.
        defines { "DE_PLATFORM_WINDOWS", "NOMINMAX" }

    filter "system:linux"
        defines { "DE_PLATFORM_LINUX" }

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

    filter {}
    copyAssimpRuntime()               -- required, see below
```

`copyAssimpRuntime()` (defined in the root `premake5.lua`) is what makes an executable
link and start. On Windows it copies assimp's DLLs next to the `.exe`, without which it
dies with `STATUS_DLL_NOT_FOUND` before `main`. On Linux it gives the executable the
engine's whole dependency list in a link group: `make` links only the libraries a
project names, where MSBuild also links the engine's own.

Register it in the root `premake5.lua`:

```lua
group "Examples"
    include "examples/MyGame"
group ""
```

**3. Generate & build.**

*Windows*: regenerate the Visual Studio solution and build:

```bash
./vendor/premake/bin/premake5.exe vs2026
```

> The repo also has `Generate-Windows.bat`, but it ends in `PAUSE`; call `premake5`
> directly from scripts/CI.

Open `DingoEngine.slnx`, set `MyGame` as the startup project, and build. The engine
is a static lib, so the first build compiles it once; afterwards your game links
against it quickly.

*Linux*: assimp isn't vendored for Linux, so build assimp 6.0.4 (the version of the
Windows binaries, whose headers the repo shares) into `vendor/assimp/lib/linux-x86_64/`
once. The folder is git-ignored:

```bash
git clone --depth 1 -b v6.0.4 https://github.com/assimp/assimp.git /tmp/assimp
cmake -S /tmp/assimp -B /tmp/assimp/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF \
      -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF
cmake --build /tmp/assimp/build
mkdir -p vendor/assimp/lib/linux-x86_64 && cp /tmp/assimp/build/lib/libassimp.a vendor/assimp/lib/linux-x86_64/
```

Then generate makefiles (`Generate-Linux.sh` runs the repo's own premake 5.0.0-beta8,
`vendor/premake/bin/premake5 gmake`) and build (`config` is `debug`, `debug-asan`,
`release` or `distribution`; name a project to build only it and what it needs):

```bash
./Generate-Linux.sh
make -j"$(nproc)" config=debug MyGame
cd examples/MyGame && ../../build/bin/Debug-linux-x86_64/MyGame/MyGame
```

Run from the project's own directory, so relative `assets/...` paths resolve. Outputs
follow the Windows layout: `build/bin/<Config>-linux-x86_64/<Project>/`.

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
imgui/imgui.h              # Dear ImGui header (optional; UI now via Dingo::UI — DingoEngine/UI/UI.h)
assimp-vc145-mt[d].dll     # model-loader runtime   ← copy next to your .exe
```

### Compiler / linker settings

| Setting | Value |
|---|---|
| C++ standard | C++20 (`/std:c++20`) |
| Include dirs | `include`, `glm`, `imgui` |
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

### Linux

Release builds are also published as
`DingoEngine-<version>-<Config>-linux-x86_64.tar.gz`:

```
libDingoEngine.a           # the engine with every vendor library and assimp merged in
include/                   # engine public headers  ← add as an include root
glm/                       # GLM math headers       ← public dependency
```

Build with GCC ≥ 13 and link the archive with the Vulkan SDK's ShaderC and SPIRV-Cross,
zlib, dl and pthread. Nothing else is needed, not even a link group:

```bash
g++ -std=c++20 -O2 -DDE_PLATFORM_LINUX -DDE_RELEASE -DGLM_FORCE_DEPTH_ZERO_TO_ONE \
    -I <pkg>/include -I <pkg>/glm main.cpp -o MyGame \
    <pkg>/libDingoEngine.a -L"$VULKAN_SDK/lib" \
    -lshaderc_combined -lspirv-cross-hlsl -lspirv-cross-glsl -lspirv-cross-core -lz -ldl -lpthread
```

Use the package's own config define (`DE_DEBUG`, `DE_RELEASE` or `DE_DISTRIBUTION`). The
executable needs only `libz`, the C/C++ runtime, `libvulkan1` and a Vulkan driver at run
time.

### Runtime

- Windows: put `assimp-vc145-mt[d].dll` next to your executable.
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
        if (Input::IsKeyPressed(Key::Escape))
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
    params.Graphics.GraphicsAPI = GraphicsAPI::Vulkan;   // the default; DirectX11/DirectX12 on Windows
    params.EnableUI = false;

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
