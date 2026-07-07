# DingoEngine — Claude Context

## What this project is

DingoEngine is a C++20 game engine built on a graphics abstraction layer (NVRHI) targeting Vulkan (primary), DirectX 11, and DirectX 12. User code lives in `Layer` subclasses pushed onto an `Application`-owned `LayerStack`; games are usually thin layers driving Scene/ECS scripts.

## Build system

- **Tool**: Premake5. Regenerate with `./vendor/premake/bin/premake5.exe vs2026` from the repo root (do NOT use `Generate-Windows.bat` from a non-interactive shell — it ends in `PAUSE`).
- **Build**: MSBuild on `DingoEngine.slnx` (or a single example's `.vcxproj`), `/p:Configuration=Debug /p:Platform=x64`. Configs: `Debug`, `Debug-ASan`, `Release`, `Distribution`. Requires the `VULKAN_SDK` env var.
- **Output**: engine = static lib; examples = executables. Run examples with cwd = the example's source dir so relative `assets/...` paths resolve.
- **PCH**: `depch.h` / `depch.cpp`.

## Project structure

```
include/DingoEngine/       Public API headers (Core, Graphics, Events, Windowing,
                           Physics/2D, Physics/3D, Scene, UI, Audio)
src/DingoEngine/           Implementations, incl. backend-only code:
  Graphics/NVRHI/          NVRHI wrappers (Vulkan/, DirectX11/, DirectX12/)
  Physics/2D/Box2D/        the only box2d.h includer
  Physics/3D/JoltPhysics/  the only Jolt includer
  Scene/SceneData.h        the only EnTT includer (PIMPL)
vendor/                    Third-party submodules — NEVER modify vendor code
examples/                  FlappyBird, Breakout3D, SpaceInvaders (Scene/ECS showcase),
                           AngryBirds (2D physics), DungeonCrawler (top-down 2D slice),
                           DungeonCrawler3D (flagship: ECS-integrated 3D, procedural
                           dungeons, script-driven, custom materials)
```

Vendor forks that upstream as CMake (e.g. box2d) carry their own `premake5.lua`, `include`d from the root workspace.

## Code conventions

| Thing | Convention |
|---|---|
| Namespace | `Dingo` (everything) |
| Members | `m_Name` (instance), `s_Name` (static), `UPPER_SNAKE_CASE` constants/macros |
| Backend classes | `Vulkan*`, `DirectX12*`, `Nvrhi*`, `Win*` |
| Log macros | engine `DE_CORE_INFO/WARN/ERROR/ASSERT`; client `DE_INFO/...` |
| Event binding | `DE_BIND_EVENT_FN(fn)`; bit flags via `BIT(x)` |

- `DE_CORE_ASSERT(cond, msg)` takes a **plain string only** — NOT `std::format` args (adjacent-literal pasting; format args fail to compile). `DE_CORE_WARN/ERROR` do take format args.
- On-screen text must be **pure ASCII**: the MSDF atlas covers U+0020–U+00FF and rendering is byte-wise (no UTF-8 decode) — an em-dash renders as garbage.

## Comments — keep them to a minimum

Do not write code comments unless absolutely necessary. A comment must earn its place by stating a load-bearing "why" — a constraint, invariant, or non-obvious consequence the code cannot express itself. Never narrate what the next line does, never leave change-tracking or review commentary ("added X", "fixed Y"), and apply the same restraint to scripts, YAML, shaders, and example code. When in doubt, leave it out.

## Key patterns

- **Entry point**: implement `CreateApplication()` returning a heap-allocated `Application*`; `EntryPoint.h` owns `main()`.
- **Layers**: override `OnAttach/OnDetach/OnUpdate(dt)/OnUIRender` (UI only if ImGui enabled).
- **Input** (reworked v0.5.1): frame-coherent snapshot with standard semantics — `Is...Pressed` = edge ("just pressed"), `Is...Down` = held, plus `Released`/`Up` — uniform across keys, mouse buttons, and gamepad buttons (pre-0.5.1 code had `Pressed`/`Down` inverted). Gamepads: `GamepadButton`/`GamepadAxis` codes, `GetGamepadLeftStick/RightStick` (deadzone-filtered), triggers remapped to [0,1], up to 16 pads, `GamepadConnected/DisconnectedEvent`. Mouse adds `GetMouseDelta`/`GetMouseScrollDelta` + `MouseMoved/MouseScrolledEvent`.
- **Params/builder**: resources are built from fluent `*Params` structs passed to static `Create()` factories: `Texture::Create(TextureParams().SetWidth(512)...)`.
- **Bindables**: `Texture`, `GraphicsBuffer`, `Sampler` implement `IBindableShaderResource` for slot binding in a `RenderPass`.
- **Events**: `EventDispatcher dispatcher(event); dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(OnResize));`
- **Backend hiding** (hard requirement): EnTT, Box2D, Jolt, ImGui, miniaudio, NVRHI must never appear in a public header. Public APIs use abstract classes + static `Create()` + opaque handles (see `Physics3D`). GLM in public headers is fine.

## Scenes, ECS & physics

- `Scene` owns entities (EnTT, PIMPL'd) and lazily-created per-dimension physics worlds: `Physics2D` (Box2D) / `Physics3D` (Jolt), reached via `Scene::GetPhysics2D()/GetPhysics3D()`. A scene only pays for the dimension it uses.
- **Lifecycle**: `OnStart` → `OnUpdate` → `OnStop`, `IsRunning()`; `Clear()` also stops. `SceneManager` is the default driver: first `SetActiveScene` selects, later switches auto-run `OnStop`(out)+`OnStart`(in); `CreateScene` never activates. Scripts can request a switch via `RequestSceneTransition(name)` (drained by `SceneManager::OnUpdate` after the active scene updates).
- **Rendering**: `SceneManager::OnRender()` → engine-owned `SceneRenderer`, which reads the primary `CameraComponent` (ortho view from 2D `TransformComponent`, perspective from `Transform3DComponent`) + `DirectionalLightComponent`, draws the 3D pass then 2D as overlay. `Scene::RenderEntities/RenderEntities3D` stay public for custom passes.
- **Scripts**: `ScriptableEntity` — `OnCreate` → `OnStart` (before physics bake) → `OnUpdate` → `OnDestroy`. A controller script can build the whole world in `OnStart`; keeps game layers tiny (DungeonCrawler3D is the showcase).
- **Components**: 3D mirrors 2D — `Transform3DComponent` (pos, quat, scale), `MeshRendererComponent` (`Mesh*` + color + optional `Material*`), `RigidBody3DComponent`, `Box/Sphere/CapsuleCollider3DComponent` (collider size = fraction of transform scale). New built-in components must be registered via `DE_INSTANTIATE_COMPONENT` in `src/.../Entity.cpp` or clients can't use them.
- **Handles & the copy landmine**: physics components hold opaque runtime handles (`PhysicsBodyId2D/3D`; 3D's "none" sentinel is `k_InvalidBody3D` = 0xFFFFFFFF, **not 0**). Copies/clones must reset live handles to the sentinel — `Scene::DuplicateEntity` owns this; keep it covering every handle-carrying component.
- `Scene::OnUpdate` runs scripts, steps live worlds, writes simulated transforms back (2D → `TransformComponent`, 3D → `Transform3DComponent`).
- Shape is baked into the body at creation (no separate 3D shape handles). Camera picking: `Scene::ScreenPointToRay(screenPos, viewportSize)` + `Ray::IntersectGroundPlane` (perspective cameras only).

## Rendering notes

- Shaders are authored as GLSL, compiled to SPIR-V (ShaderC) and cross-compiled to HLSL/DXBC for D3D. `GLM_FORCE_DEPTH_ZERO_TO_ONE` is workspace-global: SPIR-V already emits [0,1] depth — never enable SPIRV-Cross `fixup_clipspace`. Depth targets must set `isShaderResource = false`.
- **Shader disk cache is keyed by shader NAME, not source** (`examples/<Name>/.cache/shaders/`). After editing inline shader source, delete the cache or you'll run stale bytecode.
- **Renderer2D**: auto-batching quads/circles/MSDF text; default 2000 quads per batch, overflow flushes (configurable via `ApplicationParams`).
- **Renderer3D**: CPU-transforms every submitted vertex every frame; `MaxVertices` (default 64k, configurable) **silently drops** overflow beyond a one-time WARN. `MeshRendererComponent.Mesh = nullptr` works as per-entity culling. Batches are grouped per `Material*` (null = built-in default).
- Custom material shader binding convention: **0** = scene UBO (ViewProjection + light, volatile, written each `BeginScene`), **1** = the material's own `SetUniform` params, **2+** = textures/samplers, interleaved. The binding set must match shader reflection exactly.
- Prefer `RGBA8_UNORM` for standard color textures.

## Third-party vendors

| Vendor | Role |
|---|---|
| glfw / glm / spdlog / stb | Windowing+input, math, logging, image loading |
| nvrhi | Graphics API abstraction (Vulkan / D3D11 / D3D12) |
| imgui | Debug UI (behind `Dingo::UI` facade + `Layer::OnUIRender`) |
| msdf-atlas-gen | Font MSDF atlas generation |
| assimp | Model loading (`Model::LoadFromFile` — static meshes only, no skinning) |
| entt / box2d / JoltPhysics | ECS / 2D physics / 3D physics backends (all hidden) |
| miniaudio | Audio backend (hidden behind the `Audio` interface; v0.5) |

## Failure contracts

- `Model::LoadFromFile` and `Font::Create` return `nullptr` on failure — asset paths are cwd-relative, so a wrong working directory fails loudly, not with a broken object.
- Physics per-body calls are no-ops (getters return identity) on invalid/stale handles.

## Roadmap

See [ROADMAP.md](ROADMAP.md) (v0.1 → v1.0) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (dependency-sequenced engine-gap backlog). v0.5.0 (merged) landed the audio engine (miniaudio backend + `AudioSource`/`AudioListener` + 3D positional) and gameplay-grade physics (capsule character controller, ray/shape casts) showcased by `examples/EchoVault`. In progress on branch `v0.5.1`: the input rework + gamepad support described under Key patterns.
