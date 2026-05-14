# DingoEngine

A C++20 game engine built on top of [NVRHI](https://github.com/NVIDIAGameWorks/nvrhi) with Vulkan as the primary graphics back-end.

## Features

- **Rendering** — Vulkan-backed graphics pipeline via NVRHI; batched 2D renderer (quads, circles, MSDF text)
- **Windowing & Input** — GLFW window management with a frame-accurate keyboard/mouse input system
- **Layer System** — Stack-based middleware architecture for organizing application logic
- **Event System** — Type-safe, decoupled event dispatcher (window, keyboard, mouse)
- **ImGui Integration** — Built-in debug/overlay UI layer with docking and viewport support
- **Font Rendering** — MSDF atlas-based text rendering via `msdf-atlas-gen`
- **Asset Utilities** — Texture, shader, and buffer creation with a fluent params/builder API

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full milestone plan (v0.1 → v1.0).

## Getting Started

**Prerequisites**
- Windows 10/11
- [Vulkan SDK](https://vulkan.lunarg.com/) installed and `VULKAN_SDK` environment variable set
- Visual Studio 2026

**1. Clone the repository**

```bash
git clone --recursive https://github.com/KingoBoiii/DingoEngine.git
```

If cloned non-recursively, fetch submodules afterwards:

```bash
git submodule update --init
```

**2. Generate project files**

Run [Generate-Windows.bat](Generate-Windows.bat) from the root directory. This will invoke Premake5 and produce a Visual Studio solution with all projects and dependencies configured.

**3. Build & run**

Open the generated `.sln` file in Visual Studio, set one of the example projects (`Dingo-FlappyBird` or `Dingo-Sandbox`) as the startup project, and build.

## Examples

| Project | Description |
|---|---|
| `Dingo-FlappyBird` | Complete game — sprites, input, collision, audio, score rendering |
| `Dingo-Sandbox` | Step-by-step graphics layers from a raw triangle up to a textured, camera-transformed quad |

## Project Structure

```
include/DingoEngine/    Public API
src/DingoEngine/        Engine implementation
vendor/                 Third-party dependencies (submodules)
examples/               Example projects
```

## Dependencies

| Library | Purpose |
|---|---|
| [NVRHI](https://github.com/NVIDIAGameWorks/nvrhi) | Graphics API abstraction (Vulkan / D3D12) |
| [GLFW](https://github.com/glfw/glfw) | Windowing and input |
| [GLM](https://github.com/g-truc/glm) | Math (vectors, matrices, quaternions) |
| [spdlog](https://github.com/gabime/spdlog) | Logging |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug UI |
| [stb](https://github.com/nothings/stb) | Image loading |
| [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) | Font SDF atlas generation |
