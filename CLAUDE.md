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
  Graphics/                Renderer, Renderer2D, Renderer3D, Shader, Texture, Framebuffer, Pipeline, Mesh
  Graphics/Enums/          GraphicsFormat, BufferType, TextureFormat, etc.
  Events/                  Event system (Window, Keyboard, Mouse)
  Windowing/               Window (GLFW-backed)
  ImGui/                   ImGuiLayer overlay
  Physics/2D/              Physics2D interface + 2D physics types (Box2D-backed)
  Physics/3D/              Physics3D interface + 3D physics types (Jolt-backed)

src/DingoEngine/           Implementations
  Core/                    Application loop, layer management
  Graphics/                Renderer, Renderer2D, Renderer3D, font, shader, texture, mesh
  Graphics/NVRHI/          NVRHI wrappers
    Vulkan/                VulkanGraphicsContext, VulkanSwapChain
    DirectX12/             DirectX12GraphicsContext (currently disabled)
  Physics/2D/              Physics2D factory
    Box2D/                 Box2DPhysics2D — the only box2d.h includer
  Physics/3D/              Physics3D factory
    JoltPhysics/           JoltPhysics3D — the only Jolt includer (+ JoltPhysics3DData PIMPL)

vendor/                    Third-party submodules (glfw, glm, spdlog, nvrhi, imgui, stb, msdf-atlas-gen, box2d, JoltPhysics). No changes can occur in vendor / submodules!
examples/
  FlappyBird/              Complete game (Renderer2D, input, audio, collision)
  SpaceInvaders/           Scene/ECS showcase (v0.3)
  AngryBirds/              2D physics showcase (v0.4) — slingshot, destructible towers, pigs
  DungeonCrawler3D/        3D dungeon-crawler prototype (v0.4.1) — first ECS-integrated 3D
                          scene: procedurally generated dungeons (rooms+corridors,
                          DungeonGenerator.h), walls/floor/player/enemies as RigidBody3D
                          entities, SPACE melee combat (enemy health + player health bar),
                          treasures, follow camera, drawn via Renderer3D. Runs on Vulkan,
                          D3D11 and D3D12. (Renamed from the v0.4 "Physics3D" box-tower demo.)
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
| box2d | 2D rigid-body physics backend — hidden behind the `Physics2D` interface |
| JoltPhysics | 3D rigid-body physics backend — hidden behind the `Physics3D` interface |

## Scenes, ECS & physics

- A `Scene` owns entities (EnTT) and, between `OnPhysicsStart()`/`OnPhysicsStop()`, a
  `Physics2D` world (Box2D backend) and/or a `Physics3D` world (Jolt backend). **None of
  EnTT, Box2D or Jolt appears in any public header**: EnTT lives only in
  `src/.../SceneData.h` behind the opaque `Internal::SceneData*`, Box2D only in
  `src/.../Physics/2D/Box2D/` behind the `Physics2D` interface, and Jolt only in
  `src/.../Physics/3D/JoltPhysics/` behind `Physics3D`. The `Scene` delegates physics to
  the owned worlds (`Scene::GetPhysics2D()` / `Scene::GetPhysics3D()`).
- New built-in component types must be explicitly instantiated via the
  `DE_INSTANTIATE_COMPONENT` macro in `src/.../Entity.cpp`, or client code can't use them.
- Physics components hold the simulated body/shape as an opaque handle (`PhysicsBodyId2D` /
  `PhysicsShapeId2D` for 2D; `PhysicsBodyId3D` for 3D — note **3D's "none" sentinel is
  `k_InvalidBody3D` (0xFFFFFFFF), not 0**). `Scene::OnUpdate` steps each live world after
  the script pass and writes simulated transforms back: 2D onto `TransformComponent`, 3D
  onto `Transform3DComponent`.
- **3D is now ECS-integrated (v0.4.1).** Mirroring the 2D side: `Transform3DComponent`
  (vec3 position, quat rotation, vec3 scale), `MeshRendererComponent` (a `Mesh*` + color,
  drawn through the engine's `Renderer3D`), `RigidBody3DComponent`, and
  `BoxCollider3DComponent` / `SphereCollider3DComponent`. `OnPhysicsStart` creates a 3D
  world only if the scene has `RigidBody3DComponent`s (and a 2D world only if it has 2D
  ones — a scene pays only for the dimension it uses). The 3D collider shape is **baked
  into the body at creation** (Physics3D has no separate shape handles, no `SetPosition`,
  and no angular-velocity control). Jolt's global init (allocator/Factory/RegisterTypes)
  is ref-counted across worlds. `Scene::OnRender3D(Renderer3D&, PerspectiveCamera&)` draws
  every `Transform3D`+`MeshRenderer` entity.

## Graphics API notes

- Vulkan is the primary back-end; **DirectX 11 and DirectX 12 are also functional** (selected via
  `--graphics=dx11` / `dx12`), including 3D rendering. Picked through `GraphicsAPI` in `GraphicsParams`.
- Shaders authored as GLSL: compiled to SPIR-V with ShaderC (Vulkan) and cross-compiled GLSL→HLSL→DXBC
  via SPIRV-Cross + D3DCompile for the D3D back-ends. Note: GLSL is built with `GLM_FORCE_DEPTH_ZERO_TO_ONE`,
  so SPIR-V already emits `[0,w]` depth — the HLSL path must NOT set SPIRV-Cross `fixup_clipspace`
  (that would re-convert and corrupt depth). Depth targets must set `isShaderResource = false` (D3D
  rejects a shader-resource view on a non-typeless D32 depth format with `E_INVALIDARG`).
- Shaders are reflected with SPIRV-Cross
- Vulkan validation layers are enabled when NVRHI validation is on
- `GraphicsFormat` has 80+ variants (colour, depth, BC compressed); prefer `RGBA8_UNORM` for standard colour textures

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned milestones from v0.1 to v1.0.

## Examples as reference

- **FlappyBird** (`examples/FlappyBird/`) — best end-to-end reference: sprites, input, state machine, collision, audio, score rendering
