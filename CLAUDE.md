# DingoEngine — Claude Context

## What this project is

DingoEngine is a C++20 game engine built on a graphics abstraction layer (NVRHI) targeting Vulkan (primary), DirectX 11, and DirectX 12. User code lives in `Layer` subclasses pushed onto an `Application`-owned `LayerStack`; games are usually thin layers driving Scene/ECS scripts.

## Build system

- **Tool**: Premake5. Regenerate with `./vendor/premake/bin/premake5.exe vs2026` from the repo root (do NOT use `Generate-Windows.bat` from a non-interactive shell — it ends in `PAUSE`).
- **Build**: MSBuild on `DingoEngine.slnx` (or a single example's `.vcxproj`), `/p:Configuration=Debug /p:Platform=x64`. Configs: `Debug`, `Debug-ASan`, `Release`, `Distribution`. Requires the `VULKAN_SDK` env var. Workspace `startproject` is `Dingo-TestFramework`.
- **Output**: engine = static lib; examples = executables. Run examples with cwd = the example's source dir so relative `assets/...` paths resolve.
- **New executable checklist**: any project linking the engine must call `copyAssimpRuntime()` (shared helper in the root `premake5.lua`) from its own `premake5.lua`, or it dies with `STATUS_DLL_NOT_FOUND` (`0xC0000135`) before `main`. Also add it to `.github/workflows` release packaging and the `.vscode` build/launch entries.
- **Debug-ASan** is not built by CI and does not link as configured: the Vulkan SDK prebuilts are non-ASan, so it additionally needs `_DISABLE_STRING_ANNOTATION` / `_DISABLE_VECTOR_ANNOTATION` (absent from premake today), plus the ASan runtime DLL beside the exe — a second cause of `0xC0000135`.
- **Version**: `VERSION` holds `major.minor`; CI appends the commit count and overwrites `include/DingoEngine/BuildInfo.h` (`DE_ENGINE_VERSION_*`, read by `Application::GetEngineVersion/GetEngineBuildNumber`). The checked-in `BuildInfo.h` is a `0.1.0` placeholder and `VERSION` trails the release branch — neither is a reliable "what am I on" source; the branch/tag is.
- **PCH**: `depch.h` / `depch.cpp`.

## Project structure

```
include/DingoEngine/       Public API headers (Core, Graphics, Events, Windowing,
                           Physics/2D, Physics/3D, Scene, UI, Audio, Asset)
src/DingoEngine/           Implementations, incl. backend-only code:
  Graphics/NVRHI/          NVRHI wrappers (Vulkan/, DirectX11/, DirectX12/)
  Physics/2D/Box2D/        the only box2d.h includer
  Physics/3D/JoltPhysics/  the only Jolt includer
  Scene/SceneData.h        the scene PIMPL; EnTT stays inside src/Scene/
  Scene/Systems/           ScriptSystem, PhysicsSync, AudioSync, CameraUtils —
                           the systems Scene delegates to (engine-internal)
  Asset/AssetManagerData.h the AssetManager PIMPL — worker thread, mutexes and
                           the asset maps live here, not in the public header
vendor/                    Third-party submodules — NEVER modify vendor code
examples/                  FlappyBird, Breakout3D, SpaceInvaders (Scene/ECS showcase),
                           AngryBirds (2D physics), DungeonCrawler (top-down 2D slice),
                           DungeonCrawler3D (ECS-integrated 3D, procedural dungeons,
                           script-driven, custom materials), EchoVault (v0.5 showcase:
                           character controller, casts, 3D positional audio, gamepad),
                           ArenaShooter (v0.6 showcase: AssetManager, async load,
                           hot-reload)
test/                      Dingo-TestFramework — interactive graphics/renderer/asset
                           tests, one per feature under src/Tests/
docs/                      Per-feature docs (see Docs & reviews)
```

Vendor forks that upstream as CMake (e.g. box2d) carry their own `premake5.lua`, `include`d from the root workspace.

The test framework takes `--test=<name substring>` (case-insensitive) to boot straight into one case — `--test=asset`, `--test=Text` — otherwise it starts on the first.

Device and adapter selection lives in `Graphics/NVRHI/` (`VulkanGraphicsContext.cpp` for Vulkan).

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
- **Teardown** (gotcha): an `Application::OnDestroy()` override **never runs** — `Destroy()` is called from `~Application()`, where virtual dispatch has already stripped the derived class. Put app teardown in `Layer::OnDetach`, which the engine deliberately runs while the renderer is still answerable: `Renderer::Shutdown()` parks the render thread → layers detach → `Renderer::Destroy()`. So `OnDetach` may still ask for the white texture, a sampler or the swap-chain framebuffer. Managed assets are freed after the layers that borrow them, and audio after that (an `AudioClip` must not outlive the engine that decoded it).
- **Debug window**: one tabbed ImGui window, on unless `ApplicationParams.EnableDebugOverlays = false` (it forces the ImGui backend up even in a UI-less Distribution build). **F3** = Engine, **F4** = Renderer, **F5** = Input, **F6** = Assets.
- **Input** (reworked v0.5.1): frame-coherent snapshot with standard semantics — `Is...Pressed` = edge ("just pressed"), `Is...Down` = held, plus `Released`/`Up` — uniform across keys, mouse buttons, and gamepad buttons (pre-0.5.1 code had `Pressed`/`Down` inverted). Gamepads: `GamepadButton`/`GamepadAxis` codes, `GetGamepadLeftStick/RightStick` (deadzone-filtered), triggers remapped to [0,1], up to 16 pads, `GamepadConnected/DisconnectedEvent`. Mouse adds `GetMouseDelta`/`GetMouseScrollDelta` + `MouseMoved/MouseScrolledEvent`.
- **Params/builder**: resources are built from fluent `*Params` structs passed to static `Create()` factories: `Texture::Create(TextureParams().SetWidth(512)...)`.
- **Assets** (v0.6): `Application::Get().GetAssetManager()` — UUID `AssetHandle`s, path dedup against a configurable asset root (`ApplicationParams.Assets`), `Load`/`LoadAsync` (textures+audio decode on a worker thread, GPU publish in the main-thread pump; shader/model/font async requests amortize one-per-frame on the main thread), typed `Get*` returning nullptr until the object exists, failure keeps the registration (`State == Failed`). A **texture** hot-reload moves the asset to `State == Reloading`, **not** back to `Loading`, while the new pixels decode on the worker: the object stays alive, `Get*` keeps returning the same live pointer and `IsReady()` stays true, so a game gating draws on `IsReady()` never blinks a sprite out while an artist saves. (A shader hot-reload recompiles synchronously inside the poll and never leaves `Ready`.) `GetPendingCount()` counts `LoadAsync` requests only (reloads are counted separately by `GetReloadingCount()`), so it stays usable as a loading-screen gate. Opt-in hot-reload polls timestamps and reloads textures/shaders IN PLACE: `Texture::Reinitialize` swaps contents inside the same object; `Shader::Reload` recompiles past the name-keyed disk cache, bumps `GetGeneration()`, and pipelines/render passes lazily rebuild at bind time (`NvrhiCommandList::SetPipeline`/`SetRenderPass`). A shader compile error during reload keeps the old program (no assert). `AssetManager::Reload` uses the same in-place path for textures/shaders (other types are recreated, invalidating pointers); `Unload` always frees the object, so it invalidates pointers games cache — that's why the debug panel offers Reload but no Unload. The manager OWNS what it loads — never `Destroy()`/`delete` a managed asset; raw factories remain for unmanaged resources. The F6 tab is built from `UI::AssetSummarySection`/`AssetRegistrySection`. See docs/asset-pipeline.md.
- **Audio** (v0.5): `Application::Get().GetAudioEngine()` — `LoadClip` once, then `Play` (returns an `AudioSoundId` for Stop/Pause/volume/pitch/loop/position) or `PlayOneShot` (fire-and-forget) per instance. Positional audio via the `glm::vec3` overloads + `SetListenerPosition`. In the ECS: `AudioSourceComponent` / `AudioListenerComponent`.
- **Bindables**: `Texture`, `GraphicsBuffer`, `Sampler` implement `IBindableShaderResource` for slot binding in a `RenderPass`.
- **Events**: `EventDispatcher dispatcher(event); dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(OnResize));`
- **Backend hiding** (hard requirement): EnTT, Box2D, Jolt, ImGui, miniaudio, NVRHI must never appear in a public header. Public APIs use abstract classes + static `Create()` + opaque handles (see `Physics3D`). GLM in public headers is fine.

## Scenes, ECS & physics

- `Scene` owns entities (EnTT, PIMPL'd) and lazily-created per-dimension physics worlds: `Physics2D` (Box2D) / `Physics3D` (Jolt), reached via `Scene::GetPhysics2D()/GetPhysics3D()`. A scene only pays for the dimension it uses.
- **Lifecycle**: `OnStart` → `OnUpdate` → `OnStop`, `IsRunning()`; `Clear()` also stops. `SceneManager` is the default driver: first `SetActiveScene` selects, later switches auto-run `OnStop`(out)+`OnStart`(in); `CreateScene` never activates. Scripts can request a switch via `RequestSceneTransition(name)` (drained by `SceneManager::OnUpdate` after the active scene updates).
- **Rendering**: `SceneManager::OnRender()` → engine-owned `SceneRenderer`, which reads the primary `CameraComponent` (ortho view from 2D `TransformComponent`, perspective from `Transform3DComponent`) + `DirectionalLightComponent`, draws the 3D pass then 2D as overlay. `Scene::RenderEntities/RenderEntities3D` stay public for custom passes.
- **Scripts**: `ScriptableEntity` — `OnCreate` → `OnStart` (before physics bake) → `OnUpdate` → `OnDestroy`. A controller script can build the whole world in `OnStart`; keeps game layers tiny (DungeonCrawler3D is the showcase). Calling `DestroyEntity` from inside a script's `OnDestroy` is safe: `DetachScript` unregisters the script *before* running `OnDestroy`, and `Clear`/`StartScripts`/`OnUpdate`/`ForEachScript` all walk handle snapshots rather than the live script map.
- **Components**: 3D mirrors 2D — `Transform3DComponent` (pos, quat, scale), `MeshRendererComponent` (`Mesh*` + color + optional `Material*`), `RigidBody3DComponent`, `Box/Sphere/CapsuleCollider3DComponent` (collider size = fraction of transform scale). New built-in components must be registered via `DE_INSTANTIATE_COMPONENT` in `src/.../Entity.cpp` or clients can't use them.
- **Character controllers**: `CharacterController3DComponent` — `Scene::OnUpdate` calls `controller->Update(dt)` itself, so a script only *sets* velocity/direction; never step it by hand. Controllers are rebuilt on `OnStart`, so scripts must not cache a `CharacterController3D*` across an `OnStop`/`OnStart` cycle (use-after-free).
- **Handles & the copy landmine**: physics components hold opaque runtime handles (`PhysicsBodyId2D/3D`; 3D's "none" sentinel is `k_InvalidBody3D` = 0xFFFFFFFF, **not 0**), and `CharacterController3DComponent` an index (`k_InvalidControllerIndex`). Copies/clones must reset live handles to their sentinel — `Scene::DuplicateEntity` owns this; keep it covering every handle-carrying component.
- `Scene::OnUpdate` runs scripts, steps live worlds, writes simulated transforms back (2D → `TransformComponent`, 3D → `Transform3DComponent`).
- Shape is baked into the body at creation (no separate 3D shape handles). Camera picking: `Scene::ScreenPointToRay(screenPos, viewportSize)` + `Ray::IntersectGroundPlane` (perspective cameras only).

## Rendering notes

- Shaders are authored as GLSL, compiled to SPIR-V (ShaderC) and cross-compiled to HLSL/DXBC for D3D. `GLM_FORCE_DEPTH_ZERO_TO_ONE` is workspace-global: SPIR-V already emits [0,1] depth — never enable SPIRV-Cross `fixup_clipspace`. Depth targets must set `isShaderResource = false`.
- **Shader disk cache** (`examples/<Name>/.cache/shaders/`) is validated at load: each `.spv`/`.dxbc` carries a header with a source hash (+ entry point/shader model) and a format version, so edited inline or file shaders recompile automatically — no manual cache clearing. Bump `k_ShaderCacheFormatVersion` in `NvrhiShader.cpp` when compile options or the shader toolchain change.
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

- `Model::LoadFromFile`, `Font::Create` and (v0.6) `Texture::CreateFromFile` return `nullptr` on failure — asset paths are cwd-relative, so a wrong working directory fails loudly, not with a broken object. The `AssetManager` layers its own contract on top: failed loads stay registered with `State == Failed` and `Get*` returns nullptr.
- Physics per-body calls are no-ops (getters return identity) on invalid/stale handles.

## Docs & reviews

- `docs/` — `getting-started`, `application-and-layers`, `scenes-and-ecs`, `rendering-2d`, `physics-2d`, `physics-3d`, `asset-pipeline`. Keep the relevant one in step when changing a public API.
- `.claude/reviews/` — dated code-review reports, findings keyed `B*`/`O*`/`R*` with a per-finding fix status. The current one is `2026-07-29-v0.6.0-review.md`: every Critical and High item is fixed, one commit each, recorded with how it was verified. Medium and below are being worked through, so take the *fix-status section*, not the summary sentence, as the state — and confirm against `git log` before believing either. Read it before touching the asset/hot-reload path; it also records why a passing "before" run in an A/B repro is suspect.

## Roadmap

See [ROADMAP.md](ROADMAP.md) (v0.1 → v1.0) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (dependency-sequenced engine-gap backlog).

- **v0.5.1** (merged): input rework + gamepad support.
- **v0.6.0** (merged, `86abb61`): the asset pipeline described under Key patterns — `AssetManager`, async loading, in-place texture/shader hot-reload, source-hash-validated shader cache, the F6 Assets panel — showcased by `examples/ArenaShooter` and the test app's Asset Manager Test (`--test=asset`), then a full review pass (see above).
- **Next**: v0.7 **lighting & shading** — point/spot lights on a capped forward N-light budget, colour + intensity on the directional light, a specular term, and the lit shader moved out of the `Renderer3D.cpp` string literal onto the hot-reloadable file path.
- **Then**: v0.8 **animation & character fidelity** (parent-child transform hierarchy + skinned meshes, clips, blend tree, animation events — honouring the promise v0.4.2 made and v0.5–v0.7 skipped), v0.9 **shadows, post-processing & VFX** (now purely visual), v1.0 **stability, performance & polish** (which absorbed the renderer throughput work: culling, instancing/static batching, material sharing).
- **Not in the version train**: **scripting** (was v0.7) and **networking/multiplayer** (was v0.8) both ship as out-of-band modules — see the "Modules" section at the end of `ROADMAP.md`. Online co-op is no longer part of the 1.0 launch.
