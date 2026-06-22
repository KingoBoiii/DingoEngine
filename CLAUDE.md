# DingoEngine — Claude Context

## What this project is

DingoEngine is a C++20 game engine built around a graphics abstraction layer (NVRHI) that targets Vulkan (primary) and DirectX12 (disabled by default). It follows a layered architecture where user code lives in `Layer` subclasses pushed onto an `Application`-owned `LayerStack`.

## Build system

- **Tool**: Premake5 (`premake5.lua` at root)
- **Generate**: `Generate-Windows.bat` → targets Visual Studio 2026
- **Configs**: `Debug`, `Debug-ASan`, `Release`, `Distribution`
- **Output**: DingoEngine builds as a static lib; examples build as executables
- **Requires**: Vulkan SDK (env var `VULKAN_SDK` must be set)
- **PCH**: `depch.h` / `depch.cpp`

## Project structure

```
include/DingoEngine/       Public API headers
  Core/                    Application, LayerStack, Input, Timer
  Graphics/                Renderer, Renderer2D, Shader, Texture, Framebuffer, Pipeline
  Graphics/Enums/          GraphicsFormat, BufferType, TextureFormat, etc.
  Events/                  Event system (Window, Keyboard, Mouse)
  Windowing/               Window (GLFW-backed)
  ImGui/                   ImGuiLayer overlay

src/DingoEngine/           Implementations
  Core/                    Application loop, layer management
  Graphics/                Renderer, Renderer2D, font, shader, texture
  Graphics/NVRHI/          NVRHI wrappers
    Vulkan/                VulkanGraphicsContext, VulkanSwapChain
    DirectX12/             DirectX12GraphicsContext (currently disabled)

vendor/                    Third-party submodules (glfw, glm, spdlog, nvrhi, imgui, stb, msdf-atlas-gen, box2d, JoltPhysics). No changes can occur in vendor / submodules!
examples/
  FlappyBird/              Complete game (Renderer2D, input, audio, collision)
  SpaceInvaders/           Scene/ECS showcase (v0.3)
  AngryBirds/              2D physics showcase (v0.4) — slingshot, destructible towers, pigs
  Physics3D/              3D physics showcase (v0.4, Jolt) — knock down a box tower with spheres
```

Note: upstream Box2D ships CMake rather than Premake, so the fork carries its own
`premake5.lua` (like the other vendor forks), `include`d from the root workspace.

## Key architecture patterns

### Entry point
User implements `CreateApplication()` which is called from `EntryPoint.h`'s `main()`. Return a heap-allocated `Application*`.

### Layer system
```cpp
class MyLayer : public Dingo::Layer {
    void OnAttach() override {}
    void OnDetach() override {}
    void OnUpdate(float deltaTime) override {}
    void OnUIRender() override {}   // only if ImGui enabled
};
```

### Params / builder pattern
All major resource types are constructed via `*Params` structs with a fluent setter API, then passed to a static `Create()` factory:
```cpp
auto tex = Texture::Create(TextureParams().SetWidth(512).SetHeight(512)...);
auto shader = Shader::Create(ShaderParams().SetFilePath("assets/shader.glsl"));
```

### Bindable resources
`Texture`, `GraphicsBuffer`, and `Sampler` implement `IBindableShaderResource`, making them slot-bindable in a `RenderPass`.

### Event system
Type-safe dispatcher; layers receive an `Event&` and dispatch by type:
```cpp
EventDispatcher dispatcher(event);
dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(OnResize));
```

## Naming conventions

| Thing | Convention |
|---|---|
| Namespace | `Dingo` (everything lives here) |
| Private/protected members | `m_VariableName` |
| Static members | `s_StaticName` |
| Constants / macros | `UPPER_SNAKE_CASE` |
| Platform-specific classes | `Vulkan*`, `DirectX12*`, `Win*` |
| NVRHI wrappers | `Nvrhi*` |
| Getters | `Get*()` and `As<T>()` |
| Log macros (engine) | `DE_CORE_INFO`, `DE_CORE_WARN`, `DE_CORE_ERROR`, `DE_CORE_ASSERT` |
| Log macros (client) | `DE_INFO`, `DE_WARN`, `DE_ERROR`, `DE_ASSERT` |
| Event binding | `DE_BIND_EVENT_FN(fn)` |
| Bit flag helper | `BIT(x)` |

## Application lifecycle (what happens inside `Application::Run()`)

1. `Log::Initialize()`
2. `CacheManager::Initialize()`
3. `Window::Initialize()` — GLFW, event callbacks
4. `GraphicsContext::Initialize()` — GPU device (Vulkan or D3D12)
5. `SwapChain::Initialize()`
6. `AppRenderer::Create()` + `Renderer::InitializeStaticResources()` (white texture, samplers)
7. `Renderer2D::Create()` — batch renderer
8. User's `OnInitialize()` — custom layer push
9. *(if enabled)* `ImGuiLayer` setup

Main loop per frame:
- `Input::Update()` — snapshot current vs previous key/mouse state
- `Window::Update()` — poll GLFW events
- `Renderer::BeginFrame()`
- `layer->OnUpdate(deltaTime)` for each layer
- ImGui render pass (if enabled)
- `Renderer::EndFrame()` → present

## Renderer2D capabilities

- Batched quads: up to 1 000 per flush, 32 texture slots
- Circles: `DrawCircle(transform, color, thickness, fade)`
- Text: `DrawString(text, font, transform, color)` — MSDF atlas-based
- Fonts use `msdf-atlas-gen` with 8 worker threads by default

## Third-party vendors

| Vendor | Role |
|---|---|
| glfw | Windowing + input polling |
| glm | Math (vec, mat, quat) |
| spdlog | Logging back-end |
| nvrhi | Graphics API abstraction (Vulkan / D3D12) |
| imgui | Debug/editor UI |
| stb | Image loading (`stb_image`) |
| msdf-atlas-gen | Font SDF atlas generation |
| entt | Entity-component system backend (scenes) — hidden behind `Internal::SceneData` |
| box2d | 2D rigid-body physics backend — hidden behind the `Scene` physics API |
| JoltPhysics | 3D rigid-body physics backend — hidden behind `PhysicsWorld3D` |

## Scenes, ECS & physics

- A `Scene` owns entities (EnTT) and, between `OnPhysicsStart()`/`OnPhysicsStop()`, a
  Box2D world. **Neither EnTT nor Box2D appears in any public header** — both live only
  in `src/.../SceneData.h` behind the opaque `Internal::SceneData*`.
- New built-in component types must be explicitly instantiated via the
  `DE_INSTANTIATE_COMPONENT` macro in `src/.../Entity.cpp`, or client code can't use them.
- Physics components hold the Box2D body/shape as an opaque `std::uint64_t` handle
  (`b2StoreBodyId`/`b2StoreShapeId`). `Scene::OnUpdate` steps the world after the script
  pass and writes simulated transforms back onto the `TransformComponent`s.
- **3D physics is separate**: a standalone `PhysicsWorld3D` (Jolt) under
  `include/DingoEngine/Physics/`, NOT wired into the ECS (the engine has no 3D scene yet).
  Jolt is confined to `src/.../Physics/` (`Physics3DData.h` PIMPL); bodies are opaque
  `PhysicsBodyId3D` (a packed Jolt `BodyID`). You drive it directly and render from
  `GetTransform`. Jolt's global init (allocator/Factory/RegisterTypes) is ref-counted across worlds.

## Graphics API notes

- Vulkan is the only active back-end; D3D12 is compiled out with `#if USE_NVRHI_D3D12`
- Shaders are compiled with ShaderC (GLSL→SPIR-V) and reflected with SPIRV-Cross
- Vulkan validation layers are enabled when NVRHI validation is on
- `GraphicsFormat` has 80+ variants (colour, depth, BC compressed); prefer `RGBA8_UNORM` for standard colour textures

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned milestones from v0.1 to v1.0.

## Examples as reference

- **FlappyBird** (`examples/FlappyBird/`) — best end-to-end reference: sprites, input, state machine, collision, audio, score rendering
