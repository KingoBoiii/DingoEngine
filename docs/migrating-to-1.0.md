# Migrating to 1.0

This guide is for a game built on DingoEngine v0.9.x that upgrades to 1.0. Every break between
v0.9.1 and v1.0.0 has an entry: what changed, why, and the code before and after. Version 1.0
is the last release that may break the API without a major version. From here on
[versioning](versioning.md) applies: a minor release adds, a patch release fixes, and a
deprecation lives at least one minor release.

The quickest way through: update the engine, build, and fix the compile errors in the order of
the sections below (the likeliest breaks come first). Then read
[Behaviour changes](#behaviour-changes), because a few changes compile and run but do something
different. Last, tick through the [Checklist](#checklist).

- [What 1.0 promises](#what-10-promises)
- [Changes that compile but behave differently](#changes-that-compile-but-behave-differently)
- [Entry point and Application](#entry-point-and-application)
- [Rendering](#rendering)
- [Scene and components](#scene-and-components)
- [Physics](#physics)
- [Audio](#audio)
- [Core utilities and build](#core-utilities-and-build)
- [Removals and deprecations](#removals-and-deprecations)
- [Behaviour changes](#behaviour-changes)
- [New in 1.0](#new-in-10)
- [Checklist](#checklist)

Each entry is a heading, so the page's table of contents lists them all. Code is C++20 and
every `Dingo::` name is shown without its namespace, as in the other guides.

## What 1.0 promises

Two pages describe the rules the 1.0 headers follow. Read them once; they explain why most of
the changes below look the way they do.

- [Versioning](versioning.md): SemVer from 1.0.0, a look or simulation change counts as a
  break, the promise is source compatibility rather than ABI, params structs are append-only.
- [API conventions](conventions.md): a factory returns a ready object or `nullptr`, `delete`
  frees everything, borrowed objects are never deleted, `Dingo::Internal` is not part of the
  API.

Several things were looked at and left alone, so they need no entry: params structs stay
aggregates built with designated initializers, `Renderer3D::SubmitMesh` keeps its argument
order, `Shader::Create` still never returns `nullptr` (check `IsValid()`), component pointers
still don't own what they point at, and physics still steps once per `Scene::OnUpdate`.

## Changes that compile but behave differently

Most breaks are compile errors, which the compiler finds for you. These six are not. Check
them by hand.

| Change | Where |
|---|---|
| A `Layer::OnUIRender` override without the `override` keyword is never called again. | [EnableUI, UIParams and OnUIRender become ImGui names](#enableui-uiparams-and-onuirender-become-imgui-names) |
| An index buffer made with `GraphicsBuffer::Create(params)` and no `SetIndexFormat` is 32-bit, not 16-bit. | [Index buffers default to 32-bit](#index-buffers-default-to-32-bit) |
| `DirectionalLightComponent::Ambient` defaults to 0, so a sun with no `AmbientLightComponent` lights its dark side less. | [Directional lights no longer carry an ambient](#directional-lights-no-longer-carry-an-ambient) |
| `PipelineParams::FrontCounterClockwise` defaults to `true`, so a pipeline that culls back faces culls the other triangles. | [PipelineParams winds counter-clockwise by default](#pipelineparams-winds-counter-clockwise-by-default) |
| The first `SceneManager::SetActiveScene` starts the scene. | [The first SetActiveScene starts the scene](#the-first-setactivescene-starts-the-scene) |
| `Dingo.log` is no longer written beside the executable. | [Dingo.log moved to the per-user folder](#dingolog-moved-to-the-per-user-folder) |

## Entry point and Application

### CreateApplication returns the application and the engine initializes it

**Why:** every game had to repeat the same `Initialize()` call, which belongs to the engine's
start-up.

The engine now calls `Application::Initialize()` after `CreateApplication` returns.
`Initialize()` is public and idempotent, so the old code keeps working. Delete the call.

```cpp
// before (0.9)
Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
    ApplicationParams params{ .CommandLineArgs = args };
    MyApp* app = new MyApp(params);
    app->Initialize();
    return app;
}

// after (1.0)
Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
    ApplicationParams params{ .CommandLineArgs = args };
    return new MyApp(params);
}
```

`EntryPoint.h` now only defines `main` (and `WinMain` for Distribution builds on Windows),
which call `Dingo::Internal::Main`. A game that wrote its own `main` around `Dingo::Main` calls
`Dingo::Internal::Main` instead, or better, lets `EntryPoint.h` do it.

### Application::Destroy is private

**Why:** the destructor already called it, so a game that called both deleted its layers twice.

`delete` the application (the engine does) and don't call `Destroy()`. GPU teardown belongs in
`Layer::OnDetach`, which still runs while the renderer is up.

```cpp
// before (0.9)
app->Destroy();
delete app;

// after (1.0)
delete app;
```

### Restarting on another graphics API

**Why:** a restart only worked if `CreateApplication` consumed it, and only the test app did.

`Application::HasPendingRestart()` and `ConsumePendingRestart()` are gone. `RequestRestart(api)`
still closes the app and runs `CreateApplication` again, and the application that call returns
starts on `api` whatever its params say. `CreateApplication` therefore needs no restart code.

```cpp
// before (0.9)
GraphicsAPI api = GraphicsAPI::Vulkan;
if (Application::HasPendingRestart())
    api = Application::ConsumePendingRestart();
else if (auto value = args.Get("graphics"))
    api = (*value == "dx12") ? GraphicsAPI::DirectX12 : GraphicsAPI::Vulkan;

// after (1.0)
GraphicsAPI api = GraphicsAPI::Vulkan;
if (auto value = args.Get("graphics"))
    api = (*value == "dx12") ? GraphicsAPI::DirectX12 : GraphicsAPI::Vulkan;
```

### EnableUI, UIParams and OnUIRender become ImGui names

**Why:** "UI" is reserved for the game UI that DingoUI brings; these three are the Dear ImGui debug
layer. A field rename can't carry a deprecated alias through designated initializers, so it had
to happen before 1.0.

| 0.9 | 1.0 |
|---|---|
| `ApplicationParams::EnableUI` | `ApplicationParams::EnableImGui` |
| `ApplicationParams::UI` (a `UIParams`) | `ApplicationParams::ImGui` (an `ImGuiParams`) |
| `UIParams` in `UI/UIParams.h` | `ImGuiParams` in `UI/ImGuiParams.h` |
| `Layer::OnUIRender()` | `Layer::OnImGuiRender()` |

```cpp
// before (0.9)
ApplicationParams params{
    .CommandLineArgs = args,
    .EnableUI = true,
    .UI = { .EnableDocking = true, .EnableViewports = false },
};
class DebugLayer : public Layer
{
    void OnUIRender() override { UI::Text("hello"); }
};

// after (1.0)
ApplicationParams params{
    .CommandLineArgs = args,
    .EnableImGui = true,
    .ImGui = { .EnableDocking = true, .EnableViewports = false },
};
class DebugLayer : public Layer
{
    void OnImGuiRender() override { UI::Text("hello"); }
};
```

The `ImGuiParams` fields keep their names and defaults. `UI::Begin`, `UI::Text` and the rest of
`UI/UI.h` are unchanged.

**Watch out:** an old `void OnUIRender()` without `override` still compiles. It is just a new
function nobody calls, so the layer silently stops drawing. Mark every layer function
`override`, and search the project for `OnUIRender`.

### The debug-panel functions are no longer public

**Why:** 18 functions had no caller outside the engine, and each was a promise to keep a debug
layout stable.

`UI::RendererStatsWindow`, `EngineStatsWindow`, `InputStatsWindow`, `AssetStatsWindow`,
`AnimationStatsWindow`, `ProfilerStatsWindow`, every `...Section()` function, `UI::DebugWindow`
and `UI::DebugTab`, together with `UI/DebugPanels.h`, are now engine-internal.

The built-in Debug window is unchanged: F3 to F9 open its tabs in every build that keeps
`ApplicationParams::EnableDebugOverlays` on. A game that embedded a panel in its own window draws
the numbers it wants from the same sources:

```cpp
// before (0.9)
void OnUIRender() override { UI::RendererStatsWindow(); }

// after (1.0)
void OnImGuiRender() override
{
    const Renderer3D::Statistics& stats = Application::Get().GetRenderer3D().GetStatistics();
    UI::Begin("Renderer");
    UI::Text("draws %u, meshes %u", stats.DrawCalls, stats.SubmittedMeshes);
    UI::End();
}
```

Other sources: `Renderer2D::GetStatistics()`, `Application::GetFrameTimings()`,
`Renderer::GetGpuTimers()` and [profiling](profiling.md).

### WindowParams::Fullscreen becomes Mode and FullscreenMode

**Why:** fullscreen is now one of three modes (windowed, borderless, exclusive), and a bool
can't say which. See [window modes](application-and-layers.md#window-modes).

```cpp
// before (0.9)
.Window = { .Title = "My Game", .Fullscreen = true },

// after (1.0)
.Window = { .Title = "My Game", .Mode = WindowMode::BorderlessFullscreen },
```

`WindowMode::ExclusiveFullscreen` with `.FullscreenMode = { 1920, 1080, 144 }` switches the
monitor's display mode. `WindowParams::SetFullscreen(bool)` and `Window::SetFullscreen` still
work, and mean borderless fullscreen. The `WindowParams` setters (`SetTitle`, `SetWidth`...) now
return `WindowParams&`; before, they returned a copy, so `params.Window.SetTitle("x").SetWidth(800)`
set the width on a temporary. This is source-compatible.

### Engine-owned subsystems are private to the Application

**Why:** `Window`, `AssetManager`, `GraphicsContext` and the log had public constructors and
`Initialize`/`Shutdown` calls that a game could only break things with.

| You called (0.9) | In 1.0 |
|---|---|
| `Window::Initialize`, `Shutdown`, `Update`, `WaitEvents`, `SetEventCallback`, the constructor | Private. Receive events in `Layer::OnEvent`. |
| `Window::GetNativeWindowHandle()` (a `GLFWwindow*`) | Private. GLFW no longer reaches games; there is no replacement. |
| `AssetManager`'s constructor, `Initialize`, `Shutdown`, `Update` | Private. Use `Application::Get().GetAssetManager()`. |
| `GraphicsContext::Create`, `Initialize`, `Shutdown`, `RunGarbageCollection` | Private. Read `Application::Get().GetGraphicsContext()`. |
| `Log::Initialize()`, `Log::Shutdown()` | Engine-only. New: `Log::GetFilePath()` and `Log::Flush()`. |
| `CacheManager` (`Core/CacheManager.h`) | Gone from the public headers. For a folder of your own use `Platform::GetUserDataDir(name)`. |

The audio engine's version of this change is under [Audio](#audio-engine-lifecycle-is-private).

```cpp
// before (0.9): a harness with its own asset manager
AssetManager assets(AssetManagerParams{}, &audio);
assets.Initialize();
// ...
assets.Shutdown();

// after (1.0)
AssetManager& assets = Application::Get().GetAssetManager();
```

### PopLayer and PopOverlay are gone

**Why:** nothing called them, and `LayerStack` deleted its layers without clearing its list.

Layers live from `PushLayer`/`PushOverlay` until the application ends. To switch a layer off,
keep a flag in the layer and return early from `OnUpdate`. `LayerStack` itself is now
`Internal::LayerStack`.

### ApplicationParams gained members before Window

**Why:** `Name`, `LogDirectory`, `SaveDirectory` and `SettingsSlot` sit next to
`CommandLineArgs`, where they read best.

Designated initializers (`.CommandLineArgs = args, .Window = {...}`) are unaffected as long as
they keep the struct's order, which they did. Positional initialization (`ApplicationParams{ args,
{...} }`) does not compile. From 1.0 on, new members are only added at the end
([versioning](versioning.md#rules-that-keep-minor-releases-compatible)). `Renderer3DCapabilities`
and `MaterialParams` also gained members in the middle; the same advice applies.

## Rendering

### SubMesh::MeshData and Mat become Mesh and Material

**Why:** every other type names a mesh `Mesh` and a material `Material`.

```cpp
// before (0.9)
for (const SubMesh& sub : model->GetSubMeshes())
    renderer.SubmitMesh(sub.MeshData, transform, color, sub.Mat);

// after (1.0)
for (const SubMesh& sub : model->GetSubMeshes())
    renderer.SubmitMesh(sub.Mesh, transform, color, sub.Material);
```

`SubMesh` also has a new `BaseColor` (glTF's base colour factor). Nothing applies it for you;
pass it as the mesh colour if you want it.

### Upload timing is a BufferUpload

**Why:** a bare `true` or `false` told nobody whether the write happened at once or with the
frame.

`BufferUpload::Direct` writes in a command list of its own, for data written once.
`BufferUpload::Deferred` records the write in the frame's list ahead of the draws that read it.

| 0.9 | 1.0 |
|---|---|
| `CreateVertexBuffer(size, data, true, name)` | `CreateVertexBuffer(size, data, BufferUpload::Direct, name)` |
| `GraphicsBufferParams::DirectUpload` (`bool`, default `false`) | `GraphicsBufferParams::Upload` (default `BufferUpload::Deferred`) |
| `GraphicsBufferParams::SetDirectUpload(bool)` | `GraphicsBufferParams::SetUpload(BufferUpload)` |

```cpp
// before (0.9)
GraphicsBuffer* vb = GraphicsBuffer::CreateVertexBuffer(sizeof(quad), quad, true, "Quad");
auto params = GraphicsBufferParams().SetType(BufferType::VertexBuffer).SetByteSize(size)
    .SetDirectUpload(false);

// after (1.0)
GraphicsBuffer* vb = GraphicsBuffer::CreateVertexBuffer(sizeof(quad), quad,
    BufferUpload::Direct, "Quad");
auto params = GraphicsBufferParams().SetType(BufferType::VertexBuffer).SetByteSize(size)
    .SetUpload(BufferUpload::Deferred);
```

`CreateVertexBuffer` and `CreateIndexBuffer` still default to `Direct`.

### Index buffers take an IndexFormat

**Why:** `GraphicsFormat` was only ever an index type, and `CreateIndexBuffer` hid a 16-bit
default behind four other arguments.

`GraphicsFormat` is now `IndexFormat { Uint16, Uint32 }` (`Enums/IndexFormat.h`), and
`CreateIndexBuffer(size, format, data, upload, name)` takes it second, with no default. A call
that relied on the 16-bit default now says so.

```cpp
// before (0.9)
const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
auto* ib16 = GraphicsBuffer::CreateIndexBuffer(sizeof(indices), indices, true, "Quad IB");
auto* ib32 = GraphicsBuffer::CreateIndexBuffer(bytes, nullptr, true, "Mesh IB",
    GraphicsFormat::Uint32);

// after (1.0)
const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
auto* ib16 = GraphicsBuffer::CreateIndexBuffer(sizeof(indices), IndexFormat::Uint16, indices,
    BufferUpload::Direct, "Quad IB");
auto* ib32 = GraphicsBuffer::CreateIndexBuffer(bytes, IndexFormat::Uint32, nullptr,
    BufferUpload::Direct, "Mesh IB");
```

`GraphicsBufferParams::Format`/`SetFormat` are now `IndexFormat`/`SetIndexFormat`, and
`GraphicsBuffer::GetFormat()` is `GetIndexFormat()`. `Renderer::DrawIndexed` and
`Renderer::GetIndexCount` size an index buffer by its `IndexFormat`.

An index buffer made through `GraphicsBuffer::Create(params)` is a different, silent case: see
[Index buffers default to 32-bit](#index-buffers-default-to-32-bit).

### Sampler filters are a SamplerFilter

**Why:** `MinFilter = true` doesn't say whether true is nearest or linear.

```cpp
// before (0.9)
Sampler* pixelArt = Sampler::Create(SamplerParams()
    .SetMinFilter(false).SetMagFilter(false).SetMipFilter(false));

// after (1.0)
Sampler* pixelArt = Sampler::Create(SamplerParams().SetFilter(SamplerFilter::Nearest));
```

`SamplerFilter { Nearest, Linear }` is the type of `SamplerParams::MinFilter`, `MagFilter` and
`MipFilter`, and of the three per-filter setters. `SetFilter` sets all three. The defaults are
still linear. A read such as `if (params.MagFilter)` becomes
`params.MagFilter == SamplerFilter::Linear`.

### MaterialParams names match PipelineParams

**Why:** `MaterialParams::Blend` sat beside `SetBlendMode` and `PipelineParams::BlendMode`.

| 0.9 | 1.0 |
|---|---|
| `MaterialParams::Blend` | `MaterialParams::BlendMode` |
| `MaterialParams::DepthFunction` | `MaterialParams::DepthCompare` |

The setters (`SetBlendMode`, `SetDepthCompare`) were already right and are unchanged. Only a
designated initializer or a read of the field needs editing.

```cpp
// before (0.9)
MaterialParams params{ .Shader = shader, .Blend = BlendMode::Additive };
bool additive = material->GetParams().Blend == BlendMode::Additive;

// after (1.0)
MaterialParams params{ .Shader = shader, .BlendMode = BlendMode::Additive };
bool additive = material->GetParams().BlendMode == BlendMode::Additive;
```

### PipelineParams winds counter-clockwise by default

**Why:** `MaterialParams` already defaulted `FrontCounterClockwise` to `true` (the winding of
`Mesh::CreateBox`, OBJ and glTF), and `PipelineParams` disagreed.

This one compiles. A `Pipeline` you build yourself with `CullMode::Back` (the default) and no
`SetFrontCounterClockwise` call now treats counter-clockwise triangles as front faces. If your
triangles are wound clockwise they vanish. Either set the winding or wind the mesh the other way:

```cpp
// before (0.9): clockwise fronts by default
Pipeline* pipeline = Pipeline::Create(PipelineParams()
    .SetShader(shader).SetVertexLayout(layout).SetFramebuffer(target));

// after (1.0): the same pipeline, explicitly
Pipeline* pipeline = Pipeline::Create(PipelineParams()
    .SetShader(shader).SetVertexLayout(layout).SetFramebuffer(target)
    .SetFrontCounterClockwise(false));
```

A pipeline with `CullMode::None` (a fullscreen quad, most 2D) is not affected. Materials and
`Renderer2D` are not affected either; `Renderer2D` sets its own winding.

### Texture::Upload takes only the pixels

**Why:** the second argument was forwarded as the row pitch, while `GraphicsBuffer::Upload(data,
size)` takes a byte count, so the same-looking call meant two things.

`Texture::Upload(const void* data)` uploads the whole image: `Width x Height` pixels of the
texture's format, tightly packed, first row first.

```cpp
// before (0.9)
texture->Upload(pixels, width * 4);

// after (1.0)
texture->Upload(pixels);
```

### delete frees everything, and Initialize is protected

**Why:** release differed by type (`DestroyAndDelete`, plain `delete`, `Shutdown` then `delete`),
and a plain `delete` of a `Framebuffer` leaked its attachments. One rule now covers every type,
written in [conventions](conventions.md#lifetime-who-frees-what).

- A factory returns a ready object or `nullptr`. `delete` frees the object and its GPU
  resources.
- `Destroy()` (and `Renderer2D::Shutdown()`, `Renderer3D::Shutdown()`) stays public as an early
  release. The object must still be deleted; deleting it afterwards is safe.
- `DestroyAndDelete(ptr)` is now just delete-and-null.
- `GraphicsBuffer`'s destructor is public, so `delete buffer` works.
- `Initialize()` is protected on every resource and private on `Renderer2D`, `Renderer3D` and
  `Font`. `Create` already ran it, and a second call leaked.

```cpp
// before (0.9)
Sampler* sampler = Sampler::Create(params);
sampler->Initialize();       // redundant, and leaked the first GPU object
framebuffer->Destroy();      // without this a plain delete leaked the attachments
delete framebuffer;

// after (1.0)
Sampler* sampler = Sampler::Create(params);
delete framebuffer;
```

Free GPU objects in `Layer::OnDetach`. A lit material is deleted before the `Renderer3D` that
made it. `Font` can only be made with `Font::Create`.

### Factories are [[nodiscard]]

**Why:** ignoring a factory's result leaks the object.

Every raw factory (`Create*`, `LoadFromFile`, `CreateLitMaterial`, `Renderer2D::Create`,
`Renderer3D::Create`), `Renderer3D::CreateParticleEmitter` and `CreateStaticBatch`,
`AudioEngine::LoadClip` and `CreateBus`, `AABB::Transformed` and `Frustum::FromViewProjection`
now warn when the result is dropped (an error under `/WX` or `-Werror`).

```cpp
// before (0.9): compiled, leaked
Texture::CreateFromFile("textures/grass.png");

// after (1.0): keep it (or hand it to the AssetManager, which owns what it loads)
Texture* grass = Texture::CreateFromFile("textures/grass.png");
```

`StaticBatch::Add` and `SceneManager::CreateScene` are deliberately not `[[nodiscard]]`.

### Renderer, Material and CommandList plumbing is private

**Why:** these calls drive a frame the engine already drives. A game calling them desynchronized
the renderer's binding caches.

| You used (0.9) | In 1.0 |
|---|---|
| `Renderer::BeginFrame`, `EndFrame`, `SkipFrame`, `Begin`, `Close`, `Execute` | Private. `Application::Run` owns the frame. `Renderer::IsFrameSkipped()` stays public. |
| `Renderer::GetCommandList()`, `TryGetRecordingCommandList()` | Private. Record through `Renderer::Draw`, `DrawIndexed`, `Upload`, `Clear` and `Dispatch`. `CommandList` is `Internal`. |
| `Renderer::QueueResize`, `QueueVSync` | Private. Use `Window::SetVSync` and the window's resize. |
| `Renderer::IsSwapChainFramebuffer`, `GetSwapChainResizeGeneration` | Private. Re-read `Renderer::GetSwapChainFramebuffer()`, or key caches on `Framebuffer::GetId()`. |
| `Application::GetSwapChain()`, `Graphics/SwapChain.h` | Gone. `SwapChain` is `Internal`. Use `Renderer::GetSwapChainFramebuffer()`. |
| `Material::SetSceneUniformBuffer`, `SetSkinUniformBuffer`, `SetShadowResources`, `GetOrCreateRenderPass`, `GetUniformBuffer`... | Private. `Renderer3D` drives them. The `Material::k_*Name` block names stay public. |
| `#include <DingoEngine/Graphics/CommandList.h>` | Gone. `Viewport` moved to `Graphics/Viewport.h`, which `DingoEngine.h` includes. |
| `Enums/ShaderType.h` | Gone. No public function took it. |
| `Texture::NativeEquals`, `PostProcessStack::Shutdown`, `Font::GetMSDFData` | Private. |

The old `Begin`/`Close` tests in the test app were commented out for the same reason: they
opened a command list inside the frame the engine had already opened.

## Scene and components

### Directional lights no longer carry an ambient

**Why:** `DirectionalLightComponent::Ambient` (the engine's original single knob) is deprecated
in favour of `AmbientLightComponent`. Deprecating the member while leaving its 0.35 default
would have warned only the games that had already stopped using it, so the default moved to 0.

A scene with a `DirectionalLightComponent` and no `AmbientLightComponent` gets darker: the side
turned away from the sun loses the 0.35 ambient, and so does every face between. A face squarely
lit is unchanged, since the old light was scaled by `1 - Ambient` to make room for it. To keep
the old picture, put the ambient on its own component and scale the sun by `1 - a`.

```cpp
// before (0.9): Ambient defaulted to 0.35
Entity sun = scene.CreateEntity("Sun");
sun.AddComponent<DirectionalLightComponent>();

// after (1.0): the same numbers
Entity sun = scene.CreateEntity("Sun");
sun.AddComponent<DirectionalLightComponent>().Intensity = 1.0f - 0.35f;
sun.AddComponent<AmbientLightComponent>(AmbientLightComponent(glm::vec3(1.0f), 0.35f));
```

For a light with `Intensity` `i` and `Ambient` `a`, use `Intensity = i * (1 - a)` and an
`AmbientLightComponent` of white at `a`. A game that already set `light.Ambient = 0` to use
`AmbientLightComponent` only needs to delete that line, since touching `Ambient` now warns
(removed in 2.0). A scene with no light component at all still gets the engine's default light
with its 0.35 look. See [lighting](lighting.md#scene-scoped-lighting-and-the-default-light).

### Entity: names, duplicates and missing components

**Why:** `Entity` had holes: adding a second component asserted inside EnTT, a null entity
crashed, and `IDComponent` and `TagComponent` could be added and removed, which broke the
entity.

- `AddComponent<IDComponent>`, `AddComponent<TagComponent>` and their `RemoveComponent` no
  longer link. Rename with `Entity::SetName(std::string_view)` (an empty name becomes
  `"Entity"`). Reading either component with `GetComponent` or `HasComponent` still works.
- `AddComponent<T>` on an entity that has `T` replaces it and logs a warning. Before it was an
  EnTT assert.
- `GetComponent<T>` asserts with the type's name when the entity has none. Check `HasComponent<T>()`
  first when you aren't sure.
- `HasComponent` is false and `RemoveComponent` does nothing on a null or destroyed entity.
  `GetUUID()` and `GetName()` return `0` and `""`.

```cpp
// before (0.9)
entity.GetComponent<TagComponent>().Tag = "Crate";

// after (1.0)
entity.SetName("Crate");
```

### Scene camera and light lookups

**Why:** the camera lookups returned through out-parameters and two of them were engine-internal.

| 0.9 | 1.0 |
|---|---|
| `bool Scene::GetPrimaryCameraEntity(Entity& out)` | `Entity Scene::GetPrimaryCamera()`: the first active primary camera, else the first active one, else a null `Entity` |
| `Scene::GetRenderCameras(...)` | Private |
| `Scene::GetFirstDirectionalLightEntity(Entity&)` | Removed (no callers). Keep the sun's `Entity` when you create it. |
| `Scene::HasPendingSceneTransition`, `GetPendingSceneTransition`, `ClearPendingSceneTransition` | Private. `RequestSceneTransition(name)` stays. |

```cpp
// before (0.9)
Entity camera;
if (scene.GetPrimaryCameraEntity(camera))
    UseCamera(camera);

// after (1.0)
if (Entity camera = scene.GetPrimaryCamera())
    UseCamera(camera);
```

The 1.0 lookup skips inactive entities (see
[Active and inactive entities](scenes-and-ecs.md#active-and-inactive-entities-v10)).

## Physics

### BoxCollider2DComponent Size becomes HalfExtents

**Why:** `Size` was a half extent, as a fraction of `TransformComponent::Size`, but it shared a
name with `TransformComponent::Size`, which is the full extent. The rename matches the 3D
colliders.

The meaning did not change, so **do not halve your values**. The default `{ 0.5, 0.5 }` still
covers exactly the entity's quad, and the same numbers give the same collider.

```cpp
// before (0.9)
auto& box = entity.AddComponent<BoxCollider2DComponent>();
box.Size = { 0.5f, 0.25f };          // half extents, as a fraction of the entity's size

// after (1.0)
auto& box = entity.AddComponent<BoxCollider2DComponent>();
box.HalfExtents = { 0.5f, 0.25f };   // the same collider
```

See [2D physics](physics-2d.md#boxcollider2dcomponent--circlecollider2dcomponent).

### Scene 2D getters carry a 2D suffix

**Why:** `GetGravity()` and `GetLinearVelocity()` sat beside `GetGravity3D()` and
`SetLinearVelocity` overloads for both dimensions, and read as the default.

```cpp
// before (0.9)
glm::vec2 gravity = scene.GetGravity();
float speed = glm::length(scene.GetLinearVelocity(bird));

// after (1.0)
glm::vec2 gravity = scene.GetGravity2D();
float speed = glm::length(scene.GetLinearVelocity2D(bird));
```

`SetGravity` and `SetLinearVelocity` keep their vec2/vec3 overloads. The components keep their
bare 2D names (`RigidBody2DComponent`, `TransformComponent`); only `Entity` and `Scene` calls
that would clash with a 3D one carry the suffix ([conventions](conventions.md#shapes)).

### Physics2D and Physics3D lose As<T>

**Why:** it downcast to a backend type (Box2D, Jolt) that a game can't name.

`Physics2D::As<T>()` and `Physics3D::As<T>()` are removed. They had no callers. Use the abstract
interface; the backend never reaches a game. The 2D "no body" value now has a name:
`k_InvalidBody2D` (0, as before), beside `k_InvalidBody3D`.

```cpp
// before (0.9)
if (scene.GetRuntimeBody2D(entity) == 0) { /* no body */ }

// after (1.0)
if (scene.GetRuntimeBody2D(entity) == k_InvalidBody2D) { /* no body */ }
```

The old comparison with `0` still compiles and still works; the named value is only clearer.

## Audio

### AudioSoundId is a strong type

**Why:** with a plain `uint32_t`, `SetVolume(volume, sound)` with the arguments swapped compiled.

`AudioSoundId` is now `enum class AudioSoundId : std::uint32_t`, and `k_InvalidSound` is still
`0xFFFFFFFF`. Comparing and storing it works as before; mixing it with integers does not.

```cpp
// before (0.9)
AudioSoundId hum = audio.Play(clip, params);
audio.SetVolume(0.5f, hum);                 // compiled (and did nothing useful)
uint32_t raw = hum;

// after (1.0)
AudioSoundId hum = audio.Play(clip, params);
audio.SetVolume(hum, 0.5f);
uint32_t raw = static_cast<uint32_t>(hum);
```

`AudioBusId` was already a strong type.

### Audio engine lifecycle is private

**Why:** the `Application` owns the one audio engine, and a game that created or shut down a
second one broke the first.

`AudioEngine::Create`, `Initialize`, `Shutdown` and `Update` are private (the application drives
them), and `AudioEngine::As<T>()` is removed. Use `Application::Get().GetAudioEngine()`. Release a
clip before the engine shuts down (the `AssetManager` does this for the clips it owns).

```cpp
// before (0.9)
AudioEngine* audio = AudioEngine::Create();
audio->Initialize();

// after (1.0)
AudioEngine& audio = Application::Get().GetAudioEngine();
```

`LoadClip` and `CreateBus` are `[[nodiscard]]`.

## Core utilities and build

### BIT, EVENT_CLASS and the assert macros

**Why:** `BIT(x)` leaked into every game's global names with no parentheses around `x`, and the
asserts expanded to `{ ... }`, which breaks `if (a) DE_ASSERT(b); else ...`.

| 0.9 | 1.0 |
|---|---|
| `BIT(n)` | `DE_BIT(n)` (also parenthesizes `n`) |
| `EVENT_CLASS_TYPE(x)`, `EVENT_CLASS_CATEGORY(x)` | `DE_EVENT_CLASS_TYPE(x)`, `DE_EVENT_CLASS_CATEGORY(x)` |
| `DE_EXPAND_MACRO`, `DE_STRINGIFY_MACRO` | Removed (unused) |
| `DE_ASSERT`, `DE_CORE_ASSERT`, `DE_VERIFY`... as `{ ... }` | `do { ... } while (0)`; compiled out as `((void)0)` |

```cpp
// before (0.9)
enum Flags : uint32_t { Solid = BIT(0), Wet = BIT(1) };

// after (1.0)
enum Flags : uint32_t { Solid = DE_BIT(0), Wet = DE_BIT(1) };
```

`DE_BIND_EVENT_FN` is unchanged. A game that wraps the asserts in its own macros needs no change.

### Events: scoped categories and IsRepeat

**Why:** the unscoped `EventCategory` injected `None` and `EventCategoryInput` and friends into
`Dingo`, and `KeyPressedEvent::GetRepeatCount()` was only ever 0 or 1.

`EventCategory` is `enum class EventCategory : uint32_t` with `|` and `&` (so a mask such as
`EventCategory::Mouse | EventCategory::Input` now compiles; it did not before). The enumerators lost
their prefix, `EventCategoryApplication` (no event used it) is gone, and
`Event::GetCategoryFlags()` returns an `EventCategory`. `MouseButtonEvent` now also reports
`MouseButton`. `Event::IsInCategory` is `const`. `KeyPressedEvent` carries `bool isRepeat`.

```cpp
// before (0.9)
if (e.IsInCategory(EventCategoryMouse)) { /* ... */ }
bool OnKeyPressed(KeyPressedEvent& e)
{
    if (e.GetRepeatCount() > 0)
        return false;
    return Fire();
}

// after (1.0)
if (e.IsInCategory(EventCategory::Mouse)) { /* ... */ }
bool OnKeyPressed(KeyPressedEvent& e)
{
    if (e.IsRepeat())
        return false;
    return Fire();
}
```

### The Button alias is removed

**Why:** the unused `Button` alias for `MouseButton` made `Button("Play")` ambiguous with
`UI::Button` under two `using namespace` directives.

```cpp
// before (0.9)
Button which = Button::Left;

// after (1.0)
MouseButton which = MouseButton::Left;
```

### UUID(uint64_t) is explicit

**Why:** `AssetHandle h = 0;` and `assets.GetTexture(5)` compiled, and silently named an
asset that doesn't exist. `AssetHandle` is a `UUID`.

```cpp
// before (0.9)
AssetHandle none = 0;
Texture* texture = assets.GetTexture(5);
UUID id = 42;

// after (1.0)
AssetHandle none = k_InvalidAsset;      // or AssetHandle{}
Texture* texture = assets.GetTexture(AssetHandle(5));
UUID id(42);
```

A default-constructed `UUID` is still the null id 0, and `k_InvalidAsset` is now `constexpr`.

### FileSystem reads images into ImageData

**Why:** `ReadImage` returned an owning raw pointer you had to hand back to `FreeImage`, through
three out-parameters.

`FileSystem::ReadImage(path, flipVertically, forceRGBA)` returns `std::optional<ImageData>`
(`Width`, `Height`, `Channels`, `Pixels` as a `std::vector<uint8_t>`). `nullopt` means it failed
(logged). The raw readers moved to `Internal`, and three others went with them:

| 0.9 | 1.0 |
|---|---|
| `ReadImage(path, &w, &h, &c, flip, rgba)` | `ReadImage(path, flip, rgba)`, returns `optional<ImageData>` |
| `ReadImageFromMemory`, `FreeImage` | Removed. There is no public in-memory decoder; `Texture::CreateFromFile` and the `AssetManager` read through the same virtual file system. |
| `ReadTextFile(path)` | Removed. Use `ReadFileBytes` as below. |
| `GetFileName(path)` | Removed (dead). Use `path.filename()` and `FileSystem::PathToUtf8`. |

```cpp
// before (0.9)
uint32_t w = 0, h = 0, c = 0;
if (const uint8_t* data = FileSystem::ReadImage(path, &w, &h, &c, true, true))
{
    Use(data, w, h);
    FileSystem::FreeImage(data);
}
std::string text = FileSystem::ReadTextFile("config/rules.txt");

// after (1.0)
if (std::optional<ImageData> image = FileSystem::ReadImage(path, true, true))
    Use(image->Pixels.data(), image->Width, image->Height);
std::string text;
if (auto bytes = FileSystem::ReadFileBytes("config/rules.txt"))
    text.assign(bytes->begin(), bytes->end());
```

`WriteImage` and `ReadFileBytes` are unchanged.

### Asset metadata and the registry

**Why:** `AssetMetadata` exposed the hot-reload poll's bookkeeping (file times), and
`GetRegistry()` handed out the live map, which froze its type.

`AssetMetadata` keeps `Handle`, `Type`, `FilePath` and `State`. `AbsolutePath`, `LastWriteTime`
and `PendingWriteTime` moved to `Internal::AssetRecord`. `AssetManager::GetRegistry()` is
`GetHandles()`, a `std::vector<AssetHandle>` in no particular order; `GetMetadata(handle)` and
`GetState(handle)` answer the rest.

```cpp
// before (0.9)
for (const auto& [handle, metadata] : assets.GetRegistry())
    List(metadata.FilePath);

// after (1.0)
for (AssetHandle handle : assets.GetHandles())
    if (const AssetMetadata* metadata = assets.GetMetadata(handle))
        List(metadata->FilePath);
```

### Log.h no longer includes Windows.h, and asserts show their message

**Why:** `Log.h` pulled in `<Windows.h>` in Debug and Distribution but not Release, so a game
could build in Release and fail in Debug, and `min`/`max`/`near`/`far` leaked into its code.

A game that used a Windows type or call without including `<Windows.h>` itself now has to.
Include it with `WIN32_LEAN_AND_MEAN` and `NOMINMAX` defined. A failed assert's message box
(Windows, Debug and Distribution) now shows the real message; the plain-string form used to say
"No message". The `DE_ASSERT_MESSAGE_BOX` macro is gone.

### Version macros are SemVer, and BuildInfo.h is generated

**Why:** CI wrote the commit count into `DE_ENGINE_VERSION_PATCH`, so 1.0.0 would have read
1.0.503, and local builds read a checked-in 0.1.0.

`VERSION` at the repository root holds `major.minor.patch`. Premake writes the git-ignored
`include/DingoEngine/BuildInfo.h` from it on every generate, with `DE_ENGINE_VERSION_MAJOR`,
`MINOR`, `PATCH` and `BUILD`. `DE_ENGINE_VERSION_PATCH` is now the release's patch number, and
the commit count is `DE_ENGINE_VERSION_BUILD` alone.

```cpp
// before (0.9): "patch" was a build counter in CI builds
DE_INFO("Engine {}.{}.{}", DE_ENGINE_VERSION_MAJOR, DE_ENGINE_VERSION_MINOR,
    DE_ENGINE_VERSION_PATCH);

// after (1.0)
DE_INFO("Engine {}.{}.{} (build {})", DE_ENGINE_VERSION_MAJOR, DE_ENGINE_VERSION_MINOR,
    DE_ENGINE_VERSION_PATCH, DE_ENGINE_VERSION_BUILD);
```

`Application::GetEngineVersion()` returns the packed version and `GetEngineBuildNumber()` the
build number. See [versioning](versioning.md#version-numbers-in-the-build).

### Building the engine from source

**Why:** 1.0 adds two header-only vendors, nlohmann/json (for [saves](saves-and-settings.md))
and doctest (for the unit tests).

If you build the engine from source (Option A in [getting started](getting-started.md)):

```bash
git pull
git submodule update --init --recursive    # vendor/json and vendor/doctest are new
./Generate-Linux.sh                        # or premake5 vs2026 on Windows: regenerates BuildInfo.h
```

Re-running premake is required, because `BuildInfo.h` is no longer in the repository. A build
system that doesn't use premake has to write that header itself (four `#define`s). Nothing a game
links changed. A release package needs none of this.

### exeMetadata gives a game's executable a name and an icon (optional)

**Why:** a Windows executable had no version resource or icon of its own, and a Linux one had no
desktop entry.

This is new, not a break, and you can skip it. Add one call to the game's `premake5.lua`, with
`icon.ico` and `icon.png` beside it:

```lua
copyAssimpRuntime()
exeMetadata {
    Name = "My Game",                 -- the only required field
    Description = "One line about it.",
    Version = "1.2.0",                -- defaults to the engine's version
    Company = "My Studio",
    Copyright = "Copyright (c) 2026 My Studio",
}
embedPackKey()
```

If you adopt it, drop a hand-made `.rc` that holds the icon (named `GLFW_ICON`) or the version
resource, since `exeMetadata` writes both. See
[Name, icon and version](distribution.md#5-name-icon-and-version).

### Dead headers: Memory.h, IO.h and CacheManager.h

**Why:** nothing called them.

`Core/Memory.h` (a commented-out tracker), `Core/IO.h` (`IO::WriteFileAtomic`, no callers) and
`Core/CacheManager.h` are removed from the public headers. For atomic writes use
[`SaveData`](saves-and-settings.md#files), which writes a temporary file, then renames it, and
keeps a `.bak`. For a per-user folder use `Platform::GetUserDataDir(name)`.

## Removals and deprecations

Removed names fail to compile. Deprecated names compile and warn with `[[deprecated]]` and the
replacement in the message; they keep working through all of 1.x and go in 2.0.

### GraphicsParams::FramesInFlight and NativeWindowHandle

**Why:** `FramesInFlight` was never read (the Vulkan backend hard-codes 2), and the window
handle was an input the engine fills in itself.

```cpp
// before (0.9)
.Graphics = { .GraphicsAPI = api, .FramesInFlight = 3 },

// after (1.0)
.Graphics = { .GraphicsAPI = api },
```

Every example in 0.9 set `.FramesInFlight = 3`, so this is the break most games meet first.
`GraphicsParams` now ends in `std::optional<bool> VulkanValidation`; see
[Vulkan validation](#vulkan-validation-is-off-in-release-unless-asked-for).

### TextureFormat::RGB is gone and RGBA is an alias

**Why:** `RGB` made an RGBA8 texture but uploaded it at 3 bytes a pixel, which garbled the
image, and `RGBA` was a second name for `RGBA8_UNORM`.

```cpp
// before (0.9): 3 bytes a pixel
Texture::CreateFromData(w, h, rgbPixels, TextureFormat::RGB);

// after (1.0): expand to 4 bytes (alpha 255) first
Texture::CreateFromData(w, h, rgbaPixels, TextureFormat::RGBA8_UNORM);
```

`TextureFormat::RGBA` still compiles, as `RGBA8_UNORM`. `CreateFromData`'s default format is now
`RGBA8_UNORM`. A `switch` over `TextureFormat` that had both `case RGBA:` and `case RGBA8_UNORM:`
now has a duplicate case.

### CullMode::BackAndFront is gone

**Why:** the name said "cull both faces", the value was `None`.

```cpp
// before (0.9)
.SetCullMode(CullMode::BackAndFront)

// after (1.0)
.SetCullMode(CullMode::None)
```

### GraphicsAPI::Headless is gone

**Why:** it was enumerator 0, so a value-initialized `GraphicsAPI{}` was `Headless`, which
`GraphicsContext::Create` always rejected with a fatal error.

`GraphicsAPI` is now `Vulkan`, `DirectX11`, `DirectX12`, and `GraphicsAPI{}` is `Vulkan`. Every
value shifted down by one, so a game that stored the enum as an integer (in a settings file, say)
reads the wrong API. Store its name instead.

### Renderer2D::GetOutput and ClearColor

**Why:** `Renderer2D::GetOutput()` always returned null (the 2D renderer draws into the swap
chain, whose image is not a `Texture`) and `Renderer2DParams::ClearColor` was written and never
read.

```cpp
// before (0.9): the field was written and never read
.Renderer2D = { .ClearColor = { 0.1f, 0.1f, 0.12f, 1.0f } },

// after (1.0): clear where you draw
renderer.Clear({ 0.1f, 0.1f, 0.12f, 1.0f });
```

For a texture you can sample, draw into a framebuffer
([render targets](rendering-2d.md#render-targets)).

### TextureParams::WrapMode and TextureWrapMode

**Why:** the field did nothing; address modes live on the sampler.

```cpp
// before (0.9)
TextureParams().SetWidth(64).SetHeight(64).SetWrapMode(TextureWrapMode::Repeat)

// after (1.0): wrap on the sampler
Sampler::Create(SamplerParams().SetAddressMode(SamplerAddressMode::Repeat))
```

### DirectionalLightComponent::Ambient (deprecated)

Deprecated, with a 0 default. This one changes a picture; it has its own entry:
[Directional lights no longer carry an ambient](#directional-lights-no-longer-carry-an-ambient).

### Renderer3D::SetDirectionalLight (deprecated)

**Why:** it replaced the default light, but any `SubmitLight` or `SetAmbientLight` call in a
scene switches that default off, so it worked only by accident.

```cpp
// before (0.9)
renderer.SetDirectionalLight({ -0.4f, -1.0f, -0.35f }, 0.35f);

// after (1.0): set it at startup ...
ApplicationParams params{
    .CommandLineArgs = args,
    .Renderer3D = { .LightDirection = { -0.4f, -1.0f, -0.35f }, .Ambient = 0.35f },
};
// ... or submit the light in each scene, with its ambient on its own
DirectionalLight sun;
sun.Direction = { -0.4f, -1.0f, -0.35f };
sun.Intensity = 1.0f - 0.35f;
renderer.SubmitLight(sun);
renderer.SetAmbientLight(glm::vec3(1.0f), 0.35f);
```

The setter still works for a scene that submits no light of its own. See
[lighting](lighting.md#scene-scoped-lighting-and-the-default-light).

### The SkinData skinning block (deprecated)

**Why:** the skin palette arena (`DE_SKIN_PALETTE` in `Skinning.glsl`) replaces it, with one
upload a scene instead of one per draw. Built-in skinned meshes already use the arena.

Only a custom skinned shader that declares a `SkinData` uniform block is affected. It still
works, capped at 256 instances a frame, and the renderer warns once. Deprecated names:
`Renderer3D::k_MaxSkinDataBinding` (use `k_MaxSkinUniformBinding`), `Renderer3D::k_MaxSkinDataInstances` and `Material::k_SkinDataBlockName`. To move
a shader over, define `DE_SKIN_PALETTE` before including `Skinning.glsl` and use its blocks; see
[custom skinned shaders](animation.md#custom-skinned-shaders). The old `k_MaxSkinnedInstancesLimit`
is 4096 (it was 256).

A custom skinned shader that copied the `DE_SkinDraw` uniform block of the v1.0 pre-releases must
change it. `DE_SkinDraw` is now a push-constant block, so a draw's palette base reaches the shader
without a buffer write (on Vulkan that write ended the render pass of every skinned instance):

```glsl
// before
layout(std140, binding = 6) uniform DE_SkinDraw { uvec4 SkinDraw; };
// after (or include Skinning.glsl with DE_SKIN_PALETTE, which declares it)
layout(push_constant) uniform DE_SkinDraw { uvec4 SkinDraw; };
```

The shader no longer defines `DE_SKIN_DATA_BINDING` for `DE_SkinDraw`, and the material bindings it
freed can move up. The renderer owns the push constants: a material shader cannot declare another
push block, its stages all declare the same one, and it is at most 128 bytes. On D3D11 and D3D12 the
block takes the lowest constant-buffer register, b0 to b13, that no uniform block uses. The first
launch after updating recompiles every shader (the shader cache format moved to 3).

### Scene::OnPhysicsStart and OnPhysicsStop (deprecated)

**Why:** they started physics without the scripts' `OnStart`, the `PlayOnStart` sources or the
running flag, which `OnStart` does in the right order.

```cpp
// before (0.9)
scene.OnPhysicsStart();
// ...
scene.OnPhysicsStop();

// after (1.0)
scene.OnStart();
// ...
scene.OnStop();
```

### Old ToString functions, gamepad constants and GetUpdateInBackground (deprecated)

**Why:** one naming rule: `ToString(value)` for enum names, `k_` for constants,
`Is*` for bools ([conventions](conventions.md#shapes)).

| Deprecated (0.9) | Use (1.0) |
|---|---|
| `AssetTypeToString(type)` | `ToString(type)` |
| `AssetStateToString(state)` | `ToString(state)` |
| `WindowModeToString(mode)` | `ToString(mode)` |
| `MaxGamepads` | `k_MaxGamepads` |
| `GamepadButtonCount` | `k_GamepadButtonCount` |
| `GamepadAxisCount` | `k_GamepadAxisCount` |
| `Application::GetUpdateInBackground()` | `Application::IsUpdatingInBackground()` |

```cpp
// before (0.9)
for (uint32_t i = 0; i < MaxGamepads; ++i) { /* ... */ }
const char* name = AssetTypeToString(AssetType::Texture2D);

// after (1.0)
for (uint32_t i = 0; i < k_MaxGamepads; ++i) { /* ... */ }
const char* name = ToString(AssetType::Texture2D);
```

## Behaviour changes

These need no code change, but they change what a game does or logs. Under
[versioning](versioning.md) a change in how a game looks or simulates counts as a break from
1.0.0 on, so these are the last ones of their kind.

### Index buffers default to 32-bit

An index buffer made with `GraphicsBuffer::Create(params)` and no `SetIndexFormat` used to mean
16-bit indices. It is `IndexFormat::Uint32` now, like the meshes the engine makes. A buffer of
`uint16_t` indices drawn as 32-bit indices shows garbage or nothing. The `CreateIndexBuffer`
helper cannot hit this, since it asks for the format.

```cpp
// before (0.9): unset format meant 16-bit
auto* ib = GraphicsBuffer::Create(GraphicsBufferParams()
    .SetType(BufferType::IndexBuffer).SetByteSize(sizeof(indices)).SetInitialData(indices));

// after (1.0): say so
auto* ib = GraphicsBuffer::Create(GraphicsBufferParams()
    .SetType(BufferType::IndexBuffer).SetByteSize(sizeof(indices)).SetInitialData(indices)
    .SetIndexFormat(IndexFormat::Uint16));
```

### Dingo.log moved to the per-user folder

The log is now `GetUserDataDir(Name)/logs/Dingo.log` (`%LOCALAPPDATA%\<Name>\logs` on Windows,
`~/.local/share/<Name>/logs` on Linux), because a game installed under Program Files can't write
beside itself. `ApplicationParams::Name` picks the folder (empty is the executable's name). The
last five runs are kept as `Dingo.log` and `Dingo.1.log` to `Dingo.4.log`.

To keep the old place, pass `--log-here` (the working directory) or `--log-dir=<path>`, or set
`ApplicationParams::LogDirectory`. Crash dumps go to the same folder. A support script that
collects `Dingo.log` from beside the executable needs the new path (`Log::GetFilePath()` names
it). See [logs and crashes](application-and-layers.md#logs-and-crashes).

The engine installs its crash handler when the `Application` is constructed. A game with its own
handler for signals or unhandled exceptions installs it afterwards and then owns the report.

### Vulkan validation is off in Release unless asked for

The validation layer cost Release about 0.6 ms of render CPU a frame. Debug builds load it as
before. Release and Distribution load it only when asked:

```cpp
.Graphics = { .GraphicsAPI = GraphicsAPI::Vulkan, .VulkanValidation = true },
```

or `--vulkan-validation` on the command line (`--vulkan-validation=off` turns it off in Debug).
A Release test run that relied on validation errors to fail now needs the flag.

### DE_VERIFY in Distribution shows a message and exits

`DE_VERIFY` and `DE_CORE_VERIFY` check in every configuration. In a Distribution build a failed
one used to break into a debugger that a player doesn't have. It now calls `FatalError`: it logs
the message with its file and line, flushes the log, shows a message box (Windows), and exits with
code 1. Debug and Release still break in the debugger. `FatalError(message)` is public
(`Core/FatalError.h`) for your own unrecoverable errors.

### Instanced draws may differ by up to 2/255 from 0.9

Meshes repeated eight or more times with one material and shadow setting (and any
`MaterialParams::Instancing` material) are drawn as one instanced draw, positioned on the GPU
instead of on the CPU. The two transforms round differently, so a pixel can differ from 0.9 by
at most 2/255, in at most 0.1 % of the pixels. Instancing is on by default. To get the 0.9 picture
exactly, turn it off:

```cpp
.Renderer3D = { .Capabilities = { .Instancing = false } },
// or at run time: renderer.SetInstancingEnabled(false);
```

Draw calls and CPU time drop sharply with it on. Translucent and skinned meshes are never
instanced. See [instancing](scenes-and-ecs.md#instancing-v10).

### Meshes and sprites outside the view are not drawn

`Renderer3D` now culls meshes, shadow casters and skinned meshes against the camera's view, and
`Scene::RenderEntities` skips sprites and circles wholly outside the 2D view (text is never
culled). The picture is identical, with one exception: a custom **vertex shader that moves
vertices off the mesh's bounds** (wind, waves, outlines) loses meshes whose rest bounds are out of
view. Turn culling off for that material:

```cpp
MaterialParams params{ .Shader = windShader, .FrustumCulled = false };
```

A mesh with `MeshRendererComponent::CullDistance` set is also skipped past that distance; the
default `0` never culls. `Renderer3D::SetCullingEnabled(false)` and
`Renderer2D::SetCullingEnabled(false)` turn culling off for checking. See
[culling](scenes-and-ecs.md#culling-v10).

### A Static entity that moves is rebaked and warned about

`MeshRendererComponent::Static` is new. A static mesh is baked into GPU buffers once and not
transformed again. If its entity moves anyway, the engine rebakes that cell of the scene and logs
a warning once, so the mesh still draws in the right place; it just costs the rebake. Set
`Static` only on meshes that don't move. See
[static geometry](scenes-and-ecs.md#static-geometry-v10).

### The first SetActiveScene starts the scene

**Why:** the first `SetActiveScene` only selected the scene, and later switches started the new
one. Games that never called `OnStart` themselves (SpaceInvaders, AngryBirds) never started their
first scene, so its `PlayOnStart` audio and physics did not run.

Every `SetActiveScene` now calls `Scene::OnStart()` (a no-op on a scene that is running). It runs
each active script's `OnStart`, then creates the physics worlds and bodies from the entities that
exist at that moment, then starts the `PlayOnStart` sources. Build the scene's entities first.
An explicit `OnStart()` after it is harmless.

```cpp
// before (0.9): the first activation only selected the scene
m_SceneManager.SetActiveScene("Level");
BuildLevel(*m_Scene);
m_Scene->OnStart();

// after (1.0): build, then activate
BuildLevel(*m_Scene);
m_SceneManager.SetActiveScene("Level");
```

A game that activated the scene first and built it afterwards, and relied on never starting it,
now gets physics and audio it did not have. Bodies for entities spawned after the start are
made as the [2D](physics-2d.md#spawning-bodies-at-runtime) and
[3D](physics-3d.md) guides describe.

### A second AddScript runs the first script's OnDestroy

An entity holds one script. Before, a second `AddScript` deleted the first without calling its
`OnDestroy`, and a script that replaced itself was freed in the middle of its own call. Now
`AddScript` calls the old script's `OnDestroy`, replaces it, and logs a warning.
`Entity::RemoveScript()` detaches a script the same way. A script detached from inside one of its
own callbacks is freed as soon as no script callback is on the stack.

```cpp
entity.AddScript<EnemyScript>();
entity.AddScript<BossScript>();     // EnemyScript::OnDestroy runs, then BossScript replaces it
```

A game that kept two scripts on one entity by accident loses the first. Put the second on a child
entity, or merge them.

### Component edits at runtime apply when the runtime object is rebuilt

A scene reads the settings components (`RigidBody3DComponent`, `CharacterController3DComponent`,
`AudioSourceComponent`...) when it builds the runtime object, at `OnStart` and when an entity
becomes active again. Two things changed to make that consistent:

- Changing `RigidBody3DComponent::Type` on a running body no longer changes how the write-back
  treats it. The body keeps the type it was built with.
- Removing a rigid body, character controller or audio source component now releases its runtime
  object. It used to leave it alive.

To apply an edit, rebuild the entity's runtime objects with `SetActive`:

```cpp
entity.GetComponent<RigidBody3DComponent>().Type = BodyType3D::Kinematic;
entity.SetActive(false);
entity.SetActive(true);     // the body is rebuilt at the current transform, at rest
```

`Scene::DestroyEntity` and `Scene::CreateRigidBody` also ignore an `Entity` that belongs to
another scene; before, they used its handle in their own registry.

### Arial became Inter in the examples

The examples and the test app ship `Inter-Bold.ttf` (and the test app `Inter-Regular.ttf`) in
place of `arialbd.ttf`: Arial is Microsoft's, and Inter's licence allows redistribution. A game
that copied an example's `fonts/arialbd.ttf` into its own assets keeps working, since its copy
is its own. A game that loads the file from an example's folder, or that starts from an example
as a template and updates it, must supply a font. Inter covers 254 of the 263 code points the
atlas bakes (Arial covered 221). It is a narrower, rounder face, so text lays out slightly
differently. `LICENSE-Inter.txt` sits at each assets root and travels with the packages.

```cpp
// before (0.9)
m_Font = Font::Create("fonts/arialbd.ttf");

// after (1.0): your own font file, here Inter
m_Font = Font::Create("fonts/Inter-Bold.ttf");
```

### Smaller changes

- `MaxSkinnedInstances` defaults to 1024 (it was 64), at most 4096. A scene with more than 64
  animated characters used to drop the rest with a warning.
- `Renderer3DCapabilities::MaxVertices` and `MaxIndices` bound one draw. A scene's draws share
  one upload arena that grows to the busiest frame, so a scene is no longer cut into pooled
  buffers.
- Meshes that meet at exactly equal depth can resolve to the other mesh than in 0.9, because
  instanced and static meshes draw in a different order from the CPU batches.
- `Scene::GetLightVisibility` answers 1 for an inactive light, as for any light not drawn.
- A glTF model whose animated nodes sit outside its armature now orders the skeleton's bones
  before its rigid-mesh nodes. `Skeleton::GetSkinJointCount()` is unchanged, but a game that
  cached joint indices should look joints up by name.
- `Scene::SetFixedTimeStep` and `ScriptableEntity::OnFixedUpdate` are new. Physics still steps
  once per `OnUpdate` with the frame's delta, so a game's simulation does not change.

## New in 1.0

Not breaks, but worth a look while you are here:

- [Saves and settings](saves-and-settings.md): `SaveData`, `SaveSlots` and `Settings`.
- [Window modes](application-and-layers.md#window-modes): borderless and exclusive fullscreen,
  and `Window::GetDisplayModes()` for an options screen.
- [Logs and crashes](application-and-layers.md#logs-and-crashes): rotated logs, crash reports
  and minidumps, `FatalError`.
- [Culling](scenes-and-ecs.md#culling-v10), [instancing](scenes-and-ecs.md#instancing-v10) and
  [static geometry](scenes-and-ecs.md#static-geometry-v10): fewer draw calls for big scenes.
- [Surface overrides](lighting.md#surface-overrides-v10): per-mesh emissive and roughness
  without a material each.
- [Active and inactive entities](scenes-and-ecs.md#active-and-inactive-entities-v10):
  `Entity::SetActive`.
- [Audio](audio.md): buses, attenuation and the new `AudioSoundId`.
- [Versioning](versioning.md) and [API conventions](conventions.md).

## Checklist

Tick these off in order. A build after each group shows what is left.

**Entry point and Application**

- [ ] `CreateApplication` ends in `return new MyApp(params);`, with no `Initialize()` call.
- [ ] No call to `Application::Destroy`, `HasPendingRestart` or `ConsumePendingRestart`.
- [ ] `EnableUI`, `UI` and `OnUIRender` renamed to `EnableImGui`, `ImGui` and `OnImGuiRender`,
      and every layer function is marked `override`.
- [ ] No use of `UI::*StatsWindow`, `UI::*Section` or `UI::DebugWindow`.
- [ ] `WindowParams::Fullscreen` replaced by `Mode`.
- [ ] No call to `Window`, `AssetManager`, `GraphicsContext`, `Log` or `CacheManager`
      lifecycle functions, to `Window::GetNativeWindowHandle` or to `PopLayer`/`PopOverlay`.
- [ ] `.FramesInFlight` removed from `GraphicsParams`.

**Rendering**

- [ ] `SubMesh::MeshData` and `Mat` renamed to `Mesh` and `Material`.
- [ ] `BufferUpload` replaces every `bool directUpload`, `DirectUpload` and `SetDirectUpload`.
- [ ] `CreateIndexBuffer` calls pass an `IndexFormat`; `GetFormat()` is `GetIndexFormat()`.
- [ ] Every index buffer made with `GraphicsBuffer::Create(params)` sets `SetIndexFormat`.
- [ ] Sampler filters use `SamplerFilter`.
- [ ] `MaterialParams::Blend` and `DepthFunction` renamed to `BlendMode` and `DepthCompare`.
- [ ] Hand-made `Pipeline`s that cull back faces state `SetFrontCounterClockwise`.
- [ ] `Texture::Upload` takes one argument.
- [ ] No `Initialize()` calls on resources, and no `Destroy()` before `delete` that you only
      needed for `Framebuffer`.
- [ ] Factory results are kept (no `[[nodiscard]]` warnings).
- [ ] No use of `Renderer::Begin`, `GetCommandList`, `Application::GetSwapChain`, or
      `CommandList.h`.
- [ ] Custom vertex shaders that move vertices set `FrustumCulled = false`.

**Scene, physics, audio**

- [ ] Every `DirectionalLightComponent` that relied on the 0.35 ambient has an
      `AmbientLightComponent` and a scaled `Intensity`; nothing sets `Ambient` any more.
- [ ] `SetName` instead of adding or removing `TagComponent` or `IDComponent`.
- [ ] `GetPrimaryCamera()` replaces `GetPrimaryCameraEntity(Entity&)`.
- [ ] `BoxCollider2DComponent::Size` renamed to `HalfExtents` (values unchanged).
- [ ] `GetGravity2D` and `GetLinearVelocity2D`.
- [ ] `AudioSoundId` is not mixed with integers; no `AudioEngine::Create` or `As<T>`.
- [ ] Scenes are built before `SetActiveScene`.
- [ ] No entity relies on a second `AddScript` keeping the first script.

**Core utilities and build**

- [ ] `DE_BIT`, `DE_EVENT_CLASS_TYPE` and `DE_EVENT_CLASS_CATEGORY`.
- [ ] `EventCategory::Mouse` and the rest, `IsRepeat()`, `MouseButton` instead of `Button`.
- [ ] `UUID(42)` and `k_InvalidAsset` instead of integer conversions.
- [ ] `FileSystem::ReadImage` returns `ImageData`; no `ReadTextFile`, `ReadImageFromMemory`
      or `FreeImage`.
- [ ] `AssetManager::GetHandles()` replaces `GetRegistry()`.
- [ ] `<Windows.h>` is included where Windows types are used.
- [ ] Engine built from source: submodules updated and premake re-run.
- [ ] Optional: `exeMetadata` in the game's `premake5.lua`.

**Deprecations (can wait for 2.0, but they warn)**

- [ ] `Renderer3D::SetDirectionalLight`, the `SkinData` block, `Scene::OnPhysicsStart` and
      `OnPhysicsStop`, the old `*ToString` names, the gamepad constants and
      `GetUpdateInBackground` moved to their replacements.

**Run it**

- [ ] The log is found in the new folder (or `--log-here` is used).
- [ ] A Release run that needs Vulkan validation passes `--vulkan-validation`.
- [ ] Lit scenes look as before: check the sun's dark side, instanced repeats and text in the
      new font.
- [ ] Physics and `PlayOnStart` audio in the first scene behave as intended.
