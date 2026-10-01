# DingoEngine — Claude Context

## What this project is

DingoEngine is a C++20 game engine built on a graphics abstraction layer (NVRHI) targeting Vulkan (primary), DirectX 11, and DirectX 12. User code lives in `Layer` subclasses pushed onto an `Application`-owned `LayerStack`; games are usually thin layers driving Scene/ECS scripts.

## Build system

- **Tool**: Premake5. Regenerate with `./vendor/premake/bin/premake5.exe vs2026` from the repo root (do NOT use `Generate-Windows.bat` from a non-interactive shell — it ends in `PAUSE`).
- **Build**: MSBuild on `DingoEngine.slnx` (or a single example's `.vcxproj`), `/p:Configuration=Debug /p:Platform=x64`. Configs: `Debug`, `Debug-ASan`, `Release`, `Distribution`. Requires the `VULKAN_SDK` env var. Workspace `startproject` is `Dingo-TestFramework`.
- **Output**: engine = static lib; examples = executables. Run examples with cwd = the example's source dir so relative `assets/...` paths resolve.
- **New executable checklist**: any project linking the engine must call `copyAssimpRuntime()` (shared helper in the root `premake5.lua`) from its own `premake5.lua`, or it dies with `STATUS_DLL_NOT_FOUND` (`0xC0000135`) before `main` — in Debug-ASan it also copies the ASan runtime DLL. Also add it to `.github/workflows` release packaging and the `.vscode` build/launch entries.
- **Debug-ASan** links and runs (v0.6.3) but is not built by CI. The Vulkan SDK's shaderc/SPIRV prebuilts are non-ASan, so the configuration defines `_DISABLE_STRING_ANNOTATION` / `_DISABLE_VECTOR_ANNOTATION` workspace-wide; dropping them brings back ~500 `LNK2038 annotate_*` mismatches. The linker needs no `-fsanitize` flag (cl embeds `/INFERASANLIBS`).
- **Version**: `VERSION` holds `major.minor`; CI appends the commit count and overwrites `include/DingoEngine/BuildInfo.h` (`DE_ENGINE_VERSION_*`, read by `Application::GetEngineVersion/GetEngineBuildNumber`). The checked-in `BuildInfo.h` is a `0.1.0` placeholder and `VERSION` trails the release branch — neither is a reliable "what am I on" source; the branch/tag is.
- **PCH**: `depch.h` / `depch.cpp`.

## Project structure

```
include/DingoEngine/       Public API headers (Core, Graphics, Events, Windowing,
                           Physics/2D, Physics/3D, Scene, UI, Audio, Asset)
src/DingoEngine/           Implementations, incl. backend-only code:
  Graphics/NVRHI/          NVRHI wrappers (Vulkan/, DirectX11/, DirectX12/)
  Graphics/Shaders/        engine GLSL (Renderer3D_Lit.glsl), embedded into the lib
  Physics/2D/Box2D/        the only box2d.h includer
  Physics/3D/JoltPhysics/  the only Jolt includer
  Scene/SceneData.h        the scene PIMPL; EnTT stays inside src/Scene/
  Scene/Systems/           ScriptSystem, PhysicsSync, AudioSync, CameraUtils,
                           LightSystem — the systems Scene delegates to
                           (engine-internal)
  Asset/AssetManagerData.h the AssetManager PIMPL — worker thread, mutexes and
                           the asset maps live here, not in the public header
vendor/                    Third-party submodules — NEVER modify vendor code
examples/                  FlappyBird, Breakout3D, SpaceInvaders (Scene/ECS showcase),
                           AngryBirds (2D physics), DungeonCrawler (top-down 2D slice),
                           DungeonCrawler3D (ECS-integrated 3D, procedural dungeons,
                           script-driven, custom materials), EchoVault (v0.5 showcase:
                           character controller, casts, 3D positional audio, gamepad;
                           v0.7: orbs and sentries carry point lights),
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
- On-screen text is **UTF-8** (v0.6.2): `DrawText`/`GetStringWidth` share the decoder in `src/DingoEngine/Graphics/Utf8.h`, and invalid bytes read as Latin-1. The MSDF atlas bakes Latin-1, printable General Punctuation (U+2010–U+2027, U+2030–U+205E) and U+20AC; anything else draws `?`. Changing the charset must bump `k_FontAtlasCacheFormatVersion` in `Font.cpp`. Only the engine project builds with `/utf-8`, so non-ASCII literals in the test app/examples are hex-escaped.

## Comments — keep them to a minimum

Do not write code comments unless absolutely necessary. A comment must earn its place by stating a load-bearing "why" — a constraint, invariant, or non-obvious consequence the code cannot express itself. Never narrate what the next line does, never leave change-tracking or review commentary ("added X", "fixed Y"), and apply the same restraint to scripts, YAML, shaders, and example code. When in doubt, leave it out.

## Key patterns

- **Entry point**: implement `CreateApplication()` returning a heap-allocated `Application*`; `EntryPoint.h` owns `main()`.
- **Layers**: override `OnAttach/OnDetach/OnUpdate(dt)/OnUIRender` (UI only if ImGui enabled).
- **Teardown**: `Application::OnDestroy()` runs once when `Run()`'s loop exits, with everything still live (v0.6.2 — before that it was called from `~Application()` → `Destroy()`, where virtual dispatch had already stripped the derived class, so overrides never ran; never move it back into `Destroy()`). GPU-resource teardown still belongs in `Layer::OnDetach`, which the engine deliberately runs while the renderer is still answerable: `Renderer::Shutdown()` parks the render thread → layers detach → `Renderer::Destroy()`. So `OnDetach` may still ask for the white texture, a sampler or the swap-chain framebuffer. Managed assets are freed after the layers that borrow them, and audio after that (an `AudioClip` must not outlive the engine that decoded it).
- **Debug window**: one tabbed ImGui window, on unless `ApplicationParams.EnableDebugOverlays = false` (it forces the ImGui backend up even in a UI-less Distribution build). **F3** = Engine, **F4** = Renderer (v0.7: incl. the light budget), **F5** = Input, **F6** = Assets.
- **Input** (reworked v0.5.1): frame-coherent snapshot with standard semantics — `Is...Pressed` = edge ("just pressed"), `Is...Down` = held, plus `Released`/`Up` — uniform across keys, mouse buttons, and gamepad buttons (pre-0.5.1 code had `Pressed`/`Down` inverted). Gamepads: `GamepadButton`/`GamepadAxis` codes, `GetGamepadLeftStick/RightStick` (deadzone-filtered), triggers remapped to [0,1], up to 16 pads, `GamepadConnected/DisconnectedEvent`. Mouse adds `GetMouseDelta`/`GetMouseScrollDelta` + `MouseMoved/MouseScrolledEvent`. v0.6.2: `SetCursorMode(Normal/Hidden/Locked)` (raw motion while Locked; `GetMouseDelta` reads zero for 2 frames after a mode change or refocus, because GLFW warps the cursor), `IsAnyKey/MouseButton{Pressed,Down}`, `WindowFocusEvent` + `Window::IsFocused()`. Input reaches the GLFW window through a private `Input::AttachWindow` that `Window` calls. ImGui's GLFW backend would reset a Hidden cursor every frame, so `ImGuiLayer::Begin` sets `NoMouseCursorChange` while the mode isn't Normal.
- **Params/builder**: resources are built from fluent `*Params` structs passed to static `Create()` factories: `Texture::Create(TextureParams().SetWidth(512)...)`.
- **Assets** (v0.6): `Application::Get().GetAssetManager()` — UUID `AssetHandle`s (a default-constructed `UUID`/`AssetHandle` is 0 = `k_InvalidAsset` since v0.6.3; fresh ids come from `UUID::Generate()`), path dedup against a configurable asset root (`ApplicationParams.Assets`), `Load`/`LoadAsync` (textures+audio decode on a worker thread, GPU publish in the main-thread pump; shader/model/font async requests amortize one-per-frame on the main thread), typed `Get*` returning nullptr until the object exists, failure keeps the registration (`State == Failed`). A **texture** hot-reload moves the asset to `State == Reloading`, **not** back to `Loading`, while the new pixels decode on the worker: the object stays alive, `Get*` keeps returning the same live pointer and `IsReady()` stays true, so a game gating draws on `IsReady()` never blinks a sprite out while an artist saves. (A shader hot-reload recompiles synchronously inside the poll and never leaves `Ready`.) `GetPendingCount()` counts `LoadAsync` requests only (reloads are counted separately by `GetReloadingCount()`), so it stays usable as a loading-screen gate. Opt-in hot-reload polls timestamps and reloads textures/shaders IN PLACE: `Texture::Reinitialize` swaps contents inside the same object; `Shader::Reload` recompiles past the name-keyed disk cache, bumps `GetGeneration()`, and pipelines/render passes lazily rebuild at bind time (`NvrhiCommandList::SetPipeline`/`SetRenderPass`). A shader compile error during reload keeps the old program (no assert). `AssetManager::Reload` uses the same in-place path for textures/shaders (other types are recreated, invalidating pointers); `Unload` always frees the object, so it invalidates pointers games cache — that's why the debug panel offers Reload but no Unload. The manager OWNS what it loads — never `Destroy()`/`delete` a managed asset; raw factories remain for unmanaged resources. The F6 tab is built from `UI::AssetSummarySection`/`AssetRegistrySection`. See docs/asset-pipeline.md.
- **Audio** (v0.5): `Application::Get().GetAudioEngine()` — `LoadClip` once, then `Play` (returns an `AudioSoundId` for Stop/Pause/volume/pitch/loop/position) or `PlayOneShot` (fire-and-forget) per instance. Positional audio via the `glm::vec3` overloads + `SetListenerPosition`. In the ECS: `AudioSourceComponent` / `AudioListenerComponent`. v0.6.2: distance falloff via `SoundAttenuation` (per sound in `SoundPlayParams::Attenuation`, live via `SetAttenuation`, engine-wide via `SetDefaultAttenuation`). Its defaults equal miniaudio's own (`MaxDistance` = float max), so behaviour is unchanged until a game opts in.
- **Bindables**: `Texture`, `GraphicsBuffer`, `Sampler` implement `IBindableShaderResource` for slot binding in a `RenderPass`.
- **Events**: `EventDispatcher dispatcher(event); dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(OnResize));`
- **Backend hiding** (hard requirement): EnTT, Box2D, Jolt, ImGui, miniaudio, NVRHI must never appear in a public header. Public APIs use abstract classes + static `Create()` + opaque handles (see `Physics3D`). GLM in public headers is fine.

## Scenes, ECS & physics

- `Scene` owns entities (EnTT, PIMPL'd) and lazily-created per-dimension physics worlds: `Physics2D` (Box2D) / `Physics3D` (Jolt), reached via `Scene::GetPhysics2D()/GetPhysics3D()`. A scene only pays for the dimension it uses.
- **Lifecycle**: `OnStart` → `OnUpdate` → `OnStop`, `IsRunning()`; `Clear()` also stops. `SceneManager` is the default driver: first `SetActiveScene` selects, later switches auto-run `OnStop`(out)+`OnStart`(in); `CreateScene` never activates. Scripts can request a switch via `RequestSceneTransition(name)` (drained by `SceneManager::OnUpdate` after the active scene updates).
- **Rendering**: `SceneManager::OnRender()` → engine-owned `SceneRenderer`, which reads the primary `CameraComponent` (ortho view from 2D `TransformComponent`, perspective from `Transform3DComponent`), submits the scene's lights through `Scene::SubmitLights`, and draws the 3D pass then 2D as overlay. `Scene::RenderEntities/RenderEntities3D` stay public for custom passes.
- **Scripts**: `ScriptableEntity` — `OnCreate` → `OnStart` (before physics bake) → `OnUpdate` → `OnDestroy`. A controller script can build the whole world in `OnStart`; keeps game layers tiny (DungeonCrawler3D is the showcase). Calling `DestroyEntity` from inside a script's `OnDestroy` is safe: `DetachScript` unregisters the script *before* running `OnDestroy`, and `Clear`/`StartScripts`/`OnUpdate`/`ForEachScript` all walk handle snapshots rather than the live script map.
- **Components**: 3D mirrors 2D — `Transform3DComponent` (pos, quat, scale), `MeshRendererComponent` (`Mesh*` + color + optional `Material*`), `RigidBody3DComponent`, `Box/Sphere/CapsuleCollider3DComponent` (collider size = fraction of transform scale), `MeshCollider3DComponent` (v0.6.2: the mesh at the FULL transform scale, like the renderer; null `Mesh` = the entity's `MeshRendererComponent::Mesh`). New built-in components must be registered via `DE_INSTANTIATE_COMPONENT` in `src/.../Entity.cpp` or clients can't use them.
- **Lights** (v0.7): `PointLightComponent`, `SpotLightComponent` (position from `Transform3DComponent`, spot aim = `Rotation * Direction`; skipped without a transform; `Enabled`), `AmbientLightComponent`, and `DirectionalLightComponent` + `Color`/`Intensity` (its legacy `Ambient` adds white ambient AND scales the light by `1 - Ambient`, keeping the old `a + (1-a)·N·L` look; up to 4 count, each adding its `Ambient`). `SceneRenderer` calls `Scene::SubmitLights` (→ `Internal::LightSystem`); a custom 3D pass calls it itself. **Default-light rule**: only a registry with NO light component gets a default `DirectionalLightComponent` (a disabled one still counts), and ambient is always set, so an all-lights-off scene goes dark. Registered in `DE_INSTANTIATE_COMPONENT`; no runtime handles. See docs/lighting.md.
- **Character controllers**: `CharacterController3DComponent` — `Scene::OnUpdate` calls `controller->Update(dt)` itself, so a script only *sets* velocity/direction; never step it by hand. Controllers are rebuilt on `OnStart`, so scripts must not cache a `CharacterController3D*` across an `OnStop`/`OnStart` cycle (use-after-free).
- **Runtime handles** (v0.6.3): public components hold settings only. An entity's live physics body (+ 2D shape ids), character-controller slot and playing sound are engine-internal EnTT components in `src/DingoEngine/Scene/Systems/RuntimeComponents.h`, present exactly while the thing they name is alive (emplaced on create, removed on destroy, `registry.clear<T>()` when the world stops). So copying, assigning or duplicating a component can never alias a body — never add a handle field back onto a public component, and never add the runtime structs to `DE_INSTANTIATE_COMPONENT`. Clients read handles via `Scene::GetRuntimeBody2D/3D`, `GetRuntimeSound`, `GetCharacterController`; 3D's "none" sentinel is `k_InvalidBody3D` = 0xFFFFFFFF, **not 0**.
- `Scene::OnUpdate` runs scripts, steps live worlds, writes simulated transforms back (2D → `TransformComponent`, 3D → `Transform3DComponent`). The 3D step takes `ceil(dt * 60 - 0.1)` Jolt collision steps, clamped to [1, 4] (v0.6.2; it was always 1, which let bodies tunnel through mesh colliders at 30 fps).
- Shape is baked into the body at creation (no separate 3D shape handles). Mesh/ConvexHull shapes are cached per world by `Mesh::GetId()` — never by `Mesh*`, whose address a freed mesh can hand to a new one — and each body wraps the shared shape in its own `ScaledShape`. Triangle meshes are Static/Kinematic only (Dynamic falls back to the hull) and one-sided; fast bodies against them need `ContinuousCollision` (Jolt `LinearCast`). Camera picking: `Scene::ScreenPointToRay(screenPos, viewportSize)` + `Ray::IntersectGroundPlane` (perspective cameras only).

## Rendering notes

- Shaders are authored as GLSL, compiled to SPIR-V (ShaderC) and cross-compiled to HLSL/DXBC for D3D. `GLM_FORCE_DEPTH_ZERO_TO_ONE` is workspace-global: SPIR-V already emits [0,1] depth — never enable SPIRV-Cross `fixup_clipspace`. Depth targets must set `isShaderResource = false`.
- **Shader disk cache** (`.cache/shaders/` beside the executable since v0.6.2 — `CacheManager` falls back to `GetUserDataDir("DingoEngine")/cache/<exe stem>` when that is read-only; it used to follow the cwd) is validated at load: each `.spv`/`.dxbc` carries a header with a source hash (+ entry point/shader model) and a format version, so edited inline or file shaders recompile automatically — no manual cache clearing. Bump `k_ShaderCacheFormatVersion` in `NvrhiShader.cpp` when compile options or the shader toolchain change.
- **Renderer2D**: auto-batching quads/circles/MSDF text; default 2000 quads per batch, overflow flushes (configurable via `ApplicationParams`).
- **Renderer3D**: CPU-transforms every submitted vertex every frame. Batches are grouped per `Material*` (null = built-in default); `MaxVertices`/`MaxIndices` (default 64k/96k, configurable) cap one batch = one draw, and a material that outgrows it spills into another chunk (v0.6.3). Only a single mesh bigger than a whole batch is dropped (warn once, `Statistics::DroppedMeshes`). `MeshRendererComponent::Visible = false` is per-entity culling. A batch whose material drew nothing for 300 scenes in a row (`k_MaxIdleBatchScenes`) is released.
- **Lighting** (v0.7): `Renderer3D::SubmitLight(Directional/Point/Spot)` + `SetAmbientLight` are scene-scoped (cleared at each `EndScene`). A scene that submits nothing gets `Renderer3DParams`' default light (`SetDirectionalLight` replaces it), pixel-identical to pre-v0.7; any submitted light, even an ignored one, switches it off. ≤ 4 directional; point + spot share `GetLocalLightBudget()` (`MaxLocalLights`, capped at 32): frustum-culled, then ranked with ties to submission order (still scenes never flicker), warn once on overflow. Point/spot lights with intensity/range ≤ 0 or non-finite values are ignored; directional lights only reject non-finite ones. No tone mapping until v0.9: lights clip (K17) and pop at the budget edge (K16).
- **Lit shader file** (v0.7): `src/DingoEngine/Graphics/Shaders/Renderer3D_Lit.glsl`, embedded into `DingoEngine.lib` by a premake rule running `scripts/embed.lua`; Release/Distribution always use that copy. Debug/Debug-ASan load it from `DE_ENGINE_SHADER_DIR` (silently using the embedded copy if the file is missing) and hot-reload it via `Internal::WatchUnmanagedShader`, only while the AssetManager's opt-in `EnableHotReload` is on. **Pitfall**: an OLD Debug exe runs the CURRENT .glsl; hide the file to A/B old code.
- **Lit materials** (v0.7): `Renderer3D::CreateLitMaterial(MaterialParams)` (lit shader, `CullMode::None`) with per-MATERIAL `Emissive*`, `Roughness` (0.5) and `Specular` (0 = off); `GetDefaultMaterial()` is shared by every mesh without its own. Texture/sampler **slot 0 only** (anything else warns once and makes the binding set invalid); alpha comes from the mesh colour only (lit draws are unsorted and write depth). `EndScene` rewrites each drawn lit material's binding 1 every frame; any `Renderer3D` may draw one; the caller deletes it before the creating renderer shuts down. A custom shader brings its own emissive/specular. `Material::SetTexture/SetSampler` rebuild the cached passes when a slot's pointer changes; the check is by pointer, so clear a slot before reusing a freed texture's address (K18).
- **Swap-chain render pass** (`VulkanFramebuffer::CreateRenderPass`): a frame is many instances of it — NVRHI ends the pass on every buffer write — so colour is `eLoad`/`eStore` and depth `eLoad`/`eStore` (v0.6.3). Never go back to `eNone`/`eDontCare`: the HUD blends over the 3D world and later batches depth-test against earlier ones. The clears run outside the pass.
- Custom material shader binding convention: **0** = scene UBO, **1** = the material's own `SetUniform` params, **2+** = textures/samplers, interleaved. The binding set must match shader reflection exactly. The scene UBO (`Renderer3D::CameraData`, std140, volatile, uploaded at the top of `EndScene`) starts with a frozen 96-byte prefix (`ViewProjection`, `LightDirection`, `Ambient`) that custom shaders may declare alone; the lights follow, mirrored by `CameraData` in `Renderer3D_Lit.glsl`. Only ever append. The prefix carries only the first directional light. A binding reflected by more than one stage enters the layout once (`NvrhiShader::CreateBindingLayoutHandle`).
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

- `Model::LoadFromFile`, `Font::Create` and (v0.6) `Texture::CreateFromFile` return `nullptr` on failure, so a wrong path fails loudly, not with a broken object. Since v0.6.3 every raw file factory (those three, `Shader::Create`/`CreateFromFile`, `AudioEngine::LoadClip`) resolves a relative path through `Internal::ResolveRawAssetPath` (`src/DingoEngine/Asset/AssetPath.h`): under the asset root if the file exists there, else unchanged, i.e. cwd-relative. The `AssetManager` layers its own contract on top: failed loads stay registered with `State == Failed` and `Get*` returns nullptr.
- Physics per-body calls are no-ops (getters return identity) on invalid/stale handles.

## Docs & reviews

- `docs/` — `getting-started`, `application-and-layers`, `scenes-and-ecs`, `rendering-2d`, `physics-2d`, `physics-3d`, `asset-pipeline`, `lighting`. Keep the relevant one in step when changing a public API.
- `.claude/reviews/` — dated code-review reports, findings keyed `B*`/`O*`/`R*` with a per-finding fix status. The current one is `2026-07-29-v0.6.0-review.md`: every Critical and High item is fixed, one commit each, recorded with how it was verified. Medium and below are being worked through, so take the *fix-status section*, not the summary sentence, as the state — and confirm against `git log` before believing either. Read it before touching the asset/hot-reload path; it also records why a passing "before" run in an A/B repro is suspect.

## Roadmap

See [ROADMAP.md](ROADMAP.md) (v0.1 → v1.0) and [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) (dependency-sequenced engine-gap backlog).

- **v0.5.1** (merged): input rework + gamepad support.
- **v0.6.0** (merged, `86abb61`): the asset pipeline described under Key patterns — `AssetManager`, async loading, in-place texture/shader hot-reload, source-hash-validated shader cache, the F6 Assets panel — showcased by `examples/ArenaShooter` and the test app's Asset Manager Test (`--test=asset`), then a full review pass (see above).
- **v0.6.1**: `Renderer3D` passes mesh UVs to custom materials as `a_TexCoord`. **v0.6.2**: game-workaround cleanup (cursor modes, focus event, any-input queries, executable-relative paths, 3D audio attenuation — see ROADMAP) plus mesh colliders — `ColliderShape3D::Mesh`/`ConvexHull`, `MeshCollider3DComponent`, opt-in `ContinuousCollision`, test app's Mesh Collider Test (`--test=collider`). **v0.6.3**: the known-bug sweep — KNOWN-BUGS K4, K5, K7, K8, K9, K12, K13 fixed, one commit each; K10 (GLM in public headers) and K11 (device migration across GPUs) deliberately deferred.
- **v0.7.0** (engine work done on the branch, P0–P6 of `.claude/plans/2026-09-26-v0.7-lighting-plan.md`, whose "As built" notes override its design; not merged or tagged yet): the lighting above, the Lighting Test (`--test=light`), DungeonCrawler3D's opt-in `--night`, and EchoVault's orbs and sentries as lit emissive materials carrying point lights.
- **Next**: finish v0.7 — the review pass (P7, `.claude/reviews/<date>-v0.7.0-review.md`, Critical/High fixed one commit each) and the *Candlewick* example (P8; needs the new-executable checklist).
- **Then**: v0.8 **animation & character fidelity** (parent-child transform hierarchy + skinned meshes, clips, blend tree, animation events — honouring the promise v0.4.2 made and v0.5–v0.7 skipped), v0.9 **shadows, post-processing & VFX** (now purely visual), v1.0 **stability, performance & polish** (which absorbed the renderer throughput work: culling, instancing/static batching, material sharing).
- **Not in the version train**: **scripting** (was v0.7) and **networking/multiplayer** (was v0.8) both ship as out-of-band modules — see the "Modules" section at the end of `ROADMAP.md`. Online co-op is no longer part of the 1.0 launch.
