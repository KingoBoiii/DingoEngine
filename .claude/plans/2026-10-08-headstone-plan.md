# Headstone requests — runtime VSync, audio buses, fog, translucent 3D materials

The four open `headstone` issues, all filed 2026-10-06 from the *Headstone* game:

- **#87** VSync can only be set when the window is created.
- **#95** Audio has no buses or mix groups.
- **#92** The lit shader has no fog.
- **#91** 3D materials can't be translucent: no depth-write switch and no transparent pass.

Anchors were read on `master` @ `e5f4749` (v0.8.3 merged). One phase per scheduled run, one commit per phase (the translucency work is two phases). Every phase keeps existing apps bit-identical unless they opt in.

## Overlap with the open v0.9 PR (#106)

#106 (`routine:skip`, not touched here) also changes `PipelineParams` (blend modes, depth compare, depth bias), appends shadow data to the scene UBO (`CameraData` / `Renderer3D_Lit.glsl`) and adds post/particle passes to `Renderer3D::EndScene`. Phases 3–5 therefore touch the same files. To keep the eventual merge small:

- Fog appends **after** whatever master's `CameraData` ends with; if #106 lands first, master is merged into this branch and the fog fields move after its fields (two `vec4`s, nothing else).
- Translucency only uses the existing `PipelineParams::DepthWrite` and the blend state every pipeline already has. No new `PipelineParams` blend field here: #106's blend modes are the place for `Additive`.

## Phase 1 — runtime VSync (#87)

**Facts**
- `WindowParams::VSync` (`include/DingoEngine/Windowing/Window.h:17`) is copied once into `SwapChainParams` (`src/DingoEngine/Core/Application.cpp:59-64`). `SwapChain::m_Params` is protected, no getter or setter (`include/DingoEngine/Graphics/SwapChain.h:87`).
- `Window` never sees the swap chain (`Window.h:60` only forward-declares `GraphicsContext`); `Application` owns both (`Application.h:171,173`). `SetFullscreen` (`Window.cpp:155-175`) reaches the swap chain through the resize event.
- The recreate is not in the backend: `Renderer::QueueResize` (`src/DingoEngine/Graphics/Renderer.cpp:224-235`) stores the request under `s_Data->Mutex` (`RendererData` fields at `:46-51`), and the render thread applies it in `RenderThreadLoop` after `Present` and before `AcquireNextImage` (`:260-273`); `BeginFrame` applies it on the main thread when no image is acquired (`:145-167`).
- D3D11/D3D12 read the flag every `Present` (`DirectX11SwapChain.cpp:116-121`, `DirectX12SwapChain.cpp:114-119`). Vulkan bakes it into the present mode at creation (`VulkanSwapChain.cpp:389`, `ChoosePresentMode` `:66-88`) and also reads it at the end of `Present` (`:320-323`, `waitIdle` when vsynced). `RecreateSwapChain()` (`:540-562`) returns false and keeps the old chain while minimized.
- Event handlers run inside `Window::Update`/`WaitEvents` (`Application.cpp:231-242`) while the render thread may be presenting, so writing the flag straight into the swap chain from the main thread races `Present` on all three backends.

**Changes**
1. `SwapChain`: `void SetVSyncFlag(bool vsync) { m_Params.VSync = vsync; }` (store only), `virtual void SetVSync(bool vsync)` (defaults to `SetVSyncFlag`: all D3D needs), `bool IsVSync() const`; all three called only from the points in step 2. `VulkanSwapChain::SetVSync` overrides: store, then `RecreateSwapChain()` (bumps `m_ResizeGeneration`, as a resize does; a false return while minimized is fine, the stored flag applies on restore).
2. `Renderer::QueueVSync(bool)` beside `QueueResize` (`Renderer.h:91-94`), with `HasPendingVSync`/`PendingVSync` in `RendererData`, read and cleared in the same locked blocks that take the pending resize (`RenderThreadLoop` and the `BeginFrame` no-image branch); the swap-chain calls run after the lock, as the resize does. When a resize is pending too, only the flag is stored (`SwapChain::SetVSyncFlag`, no recreate) and the resize's recreate picks it up, so the chain is never recreated twice.
3. `Window::SetVSync(bool)` / `IsVSync()`: store in `m_Params.VSync`, no-op if unchanged, forward to `Renderer::QueueVSync`. Safe from event handlers; takes effect after the next present.
4. F3 Engine tab: a `VSync` checkbox reading `Window::IsVSync()` (main thread; `SwapChain::IsVSync` is written on the render thread) in `WindowInfoSection()` (`src/DingoEngine/UI/ImGui/ImGuiDebugPanels.cpp:296-303`), like the hot-reload checkbox at `:533-535`.
5. Docs: a "VSync" section after "Fullscreen" in `docs/application-and-layers.md:53-66`, the `GetWindow()` row (`:119`). Limits to state: Vulkan "off" is Mailbox, else Immediate, else FIFO; D3D swap chains have no `ALLOW_TEARING` flag, so "off" may not tear windowed; ImGui viewport swap chains stay vsynced (`ImGuiLayer.cpp:159-163`); `RequestRestart` goes back to `WindowParams::VSync`. CLAUDE.md: one line that VSync goes through the resize queue.

**Verify** (sandbox: code review only). GPU machine: Breakout3D or the test app, toggle the F3 checkbox on Vulkan, DX11, DX12 and watch the F3 frame time go from ~16.7 ms to uncapped and back; toggle while minimized and restore; toggle during a window drag-resize.

## Phase 2 — audio buses (#95)

**Facts**
- Public API: `include/DingoEngine/Audio/AudioEngine.h` (master volume `:107-108`), ids in `AudioTypes.h:18-19` (`AudioSoundId`, slot + 16-bit generation), `SoundPlayParams` `:43-57`.
- Every sound is created by `StartInstance` with `pGroup = nullptr` (`src/DingoEngine/Audio/MiniAudio/MiniAudioEngine.cpp:209`), i.e. straight into the engine output. `SoundSlot`/`MiniAudioData` in `MiniAudioData.h:30-41`. `Shutdown` releases every slot (`:115-119`) then `ma_engine_uninit` (`:122`). `Update` reaps sounds `ma_sound_at_end` (`:133-150`).
- miniaudio 0.11.25: `ma_sound_group` is a typedef of `ma_sound` (`vendor/miniaudio/miniaudio.h:11273-11274`), so the existing `struct ma_sound;` forward declaration covers it. Groups start in the started state (`:77064`); `ma_sound_group_stop` stops pulling, freezing children. `ma_node_uninit` (`:74706-74729`) calls `ma_node_detach_full` (`:74790-74826`), which detaches every input: uninitialising a group with live children leaves them attached to nothing, silent and never `at_end`, so never reaped.
- `AudioSourceComponent` (`include/DingoEngine/Scene/Components.h:511-526`) → `AudioSync::PlaySource` (`src/DingoEngine/Scene/Systems/AudioSync.cpp:95-124`) copies it into `SoundPlayParams` (`:108-117`).
- Game-side workarounds: Candlewick mutes by zeroing the master volume (`examples/Candlewick/src/Audio.cpp:108-111`); Marionette refuses to play while muted (`examples/Marionette/src/Audio.cpp:16,43,53`). There is no audio doc page and no audio test.

**Changes**
1. `AudioTypes.h`: a strong `enum class AudioBusId : std::uint32_t` with `k_InvalidBus` (0xFFFFFFFF) and `k_MasterBus` (0). A plain `uint32_t` would make `PlayOneShot(clip, 1)` / `PlayOneShot(clip, 0.5)` ambiguous against the existing `PlayOneShot(clip, float)`. Ids use the sound packing (`(generation << 16) | index`, `MiniAudioEngine.cpp:28-37`), with bus slot 0 reserved for the root group and never freed, so no created bus can pack to `k_MasterBus`. A stale id is a no-op, as for sounds.
2. `AudioEngine` (pure virtual, implemented in `MiniAudioEngine`): `CreateBus(std::string_view name, AudioBusId parent = k_MasterBus)`, `DestroyBus`, `FindBus(name)`, `SetBusVolume`/`GetBusVolume`, `SetBusMuted`/`IsBusMuted`, `PauseBus`/`ResumeBus`/`IsBusPaused`, `StopBus` (releases every sound on the bus and its sub-buses).
3. Backend: `BusSlot { ma_sound* Group; uint32_t Parent; float Volume; bool Muted, Paused; std::string Name; uint16_t Generation; }` in `MiniAudioData`, plus one internal root group made in `Initialize` that master-routed sounds and top-level buses attach to, so `k_MasterBus` supports mute and pause too. `SetMasterVolume` stays on `ma_engine_set_volume` (unchanged behaviour); `SetBusVolume(k_MasterBus)` forwards to it.
4. Mute is separate from volume: the group volume is `Muted ? 0 : Volume`, so unmute restores the level. Pause is `ma_sound_group_stop`; documented consequences: `IsPlaying` still reports true for a sound on a paused bus, one-shots on it are reaped after resume, and a sound started on a paused bus stays silent until resume.
5. Routing: `SoundPlayParams::Bus = k_MasterBus`; `StartInstance` passes the bus group as `pGroup` and records the bus index in `SoundSlot`. A stale bus plays on master with a warn-once. `PlayOneShot(clip, AudioBusId bus, float volume = 1)` and `PlayOneShot(clip, const glm::vec3&, AudioBusId, float)` overloads, since one-shots are the main SFX path.
6. `DestroyBus` releases every sound routed to the bus, destroys its sub-buses leaf first, then `ma_sound_group_uninit`, `delete`, bump the generation. Shutdown: sounds, then buses children first (each `BusSlot` keeps a creation serial, since reused slots don't follow creation order; a child is always created after its parent, so descending serial is safe), then the root group, then `ma_engine_uninit`. `CreateBus` with a name already in use returns the existing bus and warns; with a stale parent it returns `k_InvalidBus` and logs an error.
7. ECS: `AudioSourceComponent::Bus = k_MasterBus`; `params.Bus = source.Bus;` in `AudioSync::PlaySource`. No `DE_INSTANTIATE_COMPONENT` change.
8. Test: an Audio Bus Test (`test/src/Tests/Audio/AudioBusTest.*`, `--test=bus`) with `[PASS]`/`[FAIL]` start-up checks in the Lighting Test's style: create/find/destroy, stale ids are no-ops, volume/mute/pause round-trip through the getters, `GetActiveSoundCount` drops after `StopBus`/`DestroyBus`, a destroyed parent takes its children. Plays `beep.wav` (already in the test assets) through an "SFX" bus with sliders for the ear check.
9. Docs: a new `docs/audio.md` (clips, `Play`/`PlayOneShot`, spatial audio and attenuation, buses), a row in `docs/README.md`, the Audio bullet in CLAUDE.md. Examples are not ported here (their workarounds keep working); noted as follow-ups.

**Verify** (sandbox: code review only; miniaudio's null backend could run the checks headless on a Linux build later). Windows: `--test=bus` all PASS; ear check that a bus volume change reaches a looping sound already playing (the Headstone complaint).

## Phase 3 — distance fog in the lit shader (#92)

**Facts**
- `Renderer3D_Lit.glsl`: the fragment stage already has `v_WorldPosition` (location 2) for both the static and `DE_SKINNED` vertex stages, and `CameraPosition` (`w = 1` world eye, `w = 0` orthographic direction, `:150`). Final colour at `:184-188`.
- `Renderer3D::CameraData` (`include/DingoEngine/Graphics/Renderer3D.h:259-279`) ends with `LocalLights[32]`; the block is 1808 bytes (static_asserts `:272-278`). Custom shaders declaring a prefix are unaffected by appended fields.
- Lights are scene-scoped: `SetAmbientLight` (`src/DingoEngine/Graphics/Renderer3D.cpp:701-706`), reset in `ClearSceneLights` (`:719-726`), resolved in `ResolveSceneLights` (`:728-805`), uploaded in `EndScene` (`:284-286`).
- ECS: `Scene::SubmitLights` (`src/DingoEngine/Scene/Scene.cpp:417-420`) → `Internal::LightSystem::SubmitLights` (`src/DingoEngine/Scene/Systems/LightSystem.cpp:50-112`, entity-order helper `:29-38`). The clear colour lives on the Scene (`Scene.h:272-273`, `m_ClearColor` `:311`); `SceneRenderer` reads it (`SceneRenderer.cpp:53-64`). `AmbientLightComponent` (`Components.h:180-190`) is the model for a new component; registration `Entity.cpp:388-413`, duplicate list `Scene.cpp:100-123`.

**Changes**
1. `Graphics/Light.h` (beside the light structs): `enum class FogMode { None, Linear, Exponential, ExponentialSquared }` and `struct Fog { FogMode Mode = Linear; glm::vec3 Color{0.5f}; float Start = 10, End = 50, Density = 0.05f, MaxOpacity = 1; }`.
2. `CameraData` appends `glm::vec4 FogColor` (rgb colour, a = max opacity, 0 = off) and `glm::vec4 FogParams` (start, end, density, mode) at 1808 and 1824; size 1840; static_asserts extended; the GLSL block mirrors it.
3. `Renderer3D::SetFog(const Fog&)` / `ClearFog()`: scene-scoped like the lights (reset in `ClearSceneLights`), non-finite values ignored, linear with `End <= Start` ignored with a warn-once. Does **not** set `m_SceneLightSubmitted`: a fogged scene keeps the default light. `Statistics::Fog` (bool) for the checks, set in `EndScene` (statistics are reset in `BeginSceneInternal`, `Renderer3D.cpp:225`).
4. Shader: after emissive, `if (FogParams.w > 0.5 && CameraPosition.w > 0.5)` compute the radial distance, the factor per mode, `× FogColor.a`, `mix` towards `FogColor.rgb`; alpha untouched. Mode 0 branches out, so fog-less scenes stay pixel-identical. Orthographic: no fog (documented limit).
5. ECS: `FogComponent { FogMode Mode; glm::vec3 Color; bool UseClearColor = true; float Start, End, Density, MaxOpacity; bool Enabled = true; }`, registered and duplicated. `LightSystem::SubmitLights` gets the clear colour from `Scene::SubmitLights`, takes the first enabled `FogComponent` in entity order (warn once on more), and calls `SetFog`. It never counts as a light for the default-light rule. No `Scene::SetFog`: one way in.
6. Test: Lighting Test steps — fog set → `Statistics::Fog`, cleared after `EndScene`, the default light survives `SetFog`, a `FogComponent` with `UseClearColor` takes the scene's clear colour; a pixel probe (the Render Target Test's `ReadPixels` pattern) of a lit quad past `End` equals the fog colour. `--lighting=fog` as a visual mode.
7. Docs: a "Fog" section in `docs/lighting.md` after "Scene-scoped lighting" (`:103`), the offset table and "1840 bytes" in "Custom material shaders" (`:318-336`), Limits (`:373`); a `FogComponent` row in `docs/scenes-and-ecs.md:69`; CLAUDE.md Lights and scene-UBO notes. No `k_ShaderCacheFormatVersion` bump: the source hash changes with the shader.

**Verify**: GLSL compiled with glslang and cross-compiled with SPIRV-Cross to HLSL SM 5.0/5.1 in the sandbox if the tools can be installed, static and `DE_SKINNED`. GPU machine: `--test=light` all PASS on three backends; 0 px against master for `--test=light` default and a frozen Candlewick/Marionette frame (no fog set).

## Phase 4 — translucent materials, part 1: material state and ranged draws (#91)

**Facts**
- Every pipeline already blends straight-alpha "over" (`src/DingoEngine/Graphics/NVRHI/NvrhiPipeline.cpp:80-92`); depth test/write come from `PipelineParams::DepthTest/DepthWrite` (`include/DingoEngine/Graphics/Pipeline.h:44-45`, setters `:86,92`; used at `NvrhiPipeline.cpp:98-105`).
- `Material::GetOrCreateRenderPass` builds the pipeline without depth settings (`src/DingoEngine/Graphics/Material.cpp:182-189`). `MaterialParams` (`include/DingoEngine/Graphics/Material.h:15-46`) has no blend or depth field. The skinned twin copies the source's params at creation (`Renderer3D.cpp:457-488`), and `CreateLitMaterial` (`:589-595`) only overrides shader and cull mode, so a new field reaches both.
- `CommandList::DrawIndexed(indexCount, instanceCount)` (`include/DingoEngine/Graphics/CommandList.h:58`) and `Renderer::DrawIndexed(Material*, ...)` (`Renderer.cpp:446-471`) draw from index 0. Each Renderer3D flush takes a whole pooled VB/IB pair sized for a batch (`Renderer3D.h:343-344`, ~3.4 MB), so a per-mesh sorted pass without ranged draws would allocate a pair per mesh.

**Changes**
1. `MaterialParams::Translucent` (`SetTranslucent(bool)`, default false) and `IsTranslucent()`. A translucent material's pipeline gets `SetDepthWrite(false)` (depth test stays on, so opaque geometry still hides it). Opaque pipelines are unchanged.
2. `CommandList::DrawIndexed(indexCount, instanceCount = 1, firstIndex = 0)` → NVRHI `DrawArguments::startIndexLocation`; a `Renderer::DrawIndexed(material, layout, vb, ib, indexCount, firstIndex)` overload. Today `indexCount = 0` means the whole buffer (`ResolveIndexCount`); with `firstIndex > 0` it means the rest of it. Existing calls are unchanged.
3. No Renderer3D behaviour change yet: a translucent material still batches in `m_DrawOrder`, it just doesn't write depth. Phase 5 adds the sort.

**Verify**: code review; GPU machine: `--test=batch`, `--test=mesh`, `--test=light` 0 px against master.

## Phase 5 — translucent materials, part 2: the sorted pass (#91)

**Facts**
- Batches: `std::unordered_map<Material*, MaterialBatch> m_Batches` (`Renderer3D.h:331`) drawn in `m_DrawOrder` (first-submission order, `:338`) by `EndScene` (`Renderer3D.cpp:293-345`); skinned draws follow (`DrawSkinnedSubmissions`, `:347`, body `:490-587`). `SubmitMesh` CPU-transforms into a chunk (`:808-901`). `CameraPosition` is always known at `EndScene` (`BeginSceneInternal`, `:215-223`). `Mesh` has no bounds.
- CLAUDE.md and `Renderer3D.h:160-161` document that a translucent static hides skinned meshes behind it, because skinned draws come last.

**Changes**
1. `SubmitMesh`: a translucent material (checked once per material per scene, `MaterialBatch::Translucent` beside `SkinnedOnly`) is not pushed to `m_DrawOrder`; its vertices are transformed into `m_TranslucentVertices/Indices` arenas and a `TranslucentSubmission { Material*, FirstVertex, VertexCount, FirstIndex, IndexCount, SortKey, Order }` is recorded. Sort key: the centroid of the transformed vertices (summed in the existing loop): squared distance to the eye for perspective, `-dot(centroid, CameraPosition.xyz)` for orthographic. The same oversize drop applies.
2. `SubmitSkinnedMesh` with a translucent material records a skinned submission keyed by instance (sort key from the instance transform's origin), keeping an instance's translucent submeshes adjacent. An all-translucent instance uploads `SkinData` once, as today. A mixed instance (opaque body, translucent visor) uploads in both passes: the skin buffer's `MaxWritesPerFrame` (`Renderer3D.cpp:416-422`, sized from `GetSkinnedInstanceBudget()`) doubles so the second upload is never dropped on Vulkan, and the instance budget is decided once, at the opaque pass, for both halves, so a character is drawn whole or not at all.
3. `EndScene`: opaque static batches, opaque skinned, then the translucent list `std::stable_sort`ed far to near (ties by `Order`, so still scenes never flicker). The walk packs consecutive same-material records into as few full-size chunks as needed (uploaded once each) and issues one ranged `DrawIndexed` per material run; a skinned record draws through the factored-out per-submission body of `DrawSkinnedSubmissions`. Lit checks (`BindsPastSlotZero`, `PrepareLitMaterial`) run once per translucent material.
4. `Statistics::TranslucentMeshes` / `TranslucentDraws` (also counted in `SubmittedMeshes` / `DrawCalls`).
5. Tests: Renderer3D Batch Test checks (three meshes, one translucent material, any order → `TranslucentDraws == 1`; alternating two materials by depth → 3); a pixel probe of two overlapping translucent quads submitted front-first equals back-to-front "over"; a translucent quad in front of an opaque one blends. `--mesh-alpha=<a>` in the Mesh 3D Test as a visual mode.
6. Docs: a "Translucent materials" section (blend over, depth write off, sorted per mesh by centroid; limits: no per-triangle sort, intersecting or large overlapping meshes can show order errors, `Additive` waits for #106's blend modes); CLAUDE.md Renderer3D / Lit materials / GPU skinning notes (`lit draws are unsorted and write depth` no longer holds for translucent materials); `Renderer3D.h:70-72,160-161,178-187` comments; the lit shader comment at `Renderer3D_Lit.glsl:186-187`.

**Verify**: code review; GPU machine: `--test=batch` PASS, `--test=mesh --mesh-alpha=0.5`, `--test=anim --anim-skeleton` (bones visible inside a translucent Fox), 0 px on the opaque regression set on Vulkan, DX11, DX12.

## Questions decided without the owner (overrule in the PR)

- **Plan first vs code now:** four features across swap chain, audio, shader UBO and the 3D batcher, none buildable in the sandbox → plan as Phase 0, one feature per later run.
- **VSync path:** through `Renderer::QueueVSync` and the render thread's resize point, not a direct `SwapChain::SetVSync` from `Window` (races `Present`).
- **Bus destroy:** stops its sounds and destroys its sub-buses, rather than re-routing them to the parent.
- **Master bus:** an internal root group, so mute/pause work on master too; `SetMasterVolume` keeps its current backend call.
- **Bus id type:** a strong `enum class AudioBusId`, not `uint32_t`, so the new `PlayOneShot` overloads can't be ambiguous with the `float` volume ones.
- **Stale bus on play:** plays on master with a warn-once, rather than failing the play.
- **Fog API:** `Renderer3D::SetFog` + `FogComponent` only (no `Scene::SetFog`); radial distance; no fog with an orthographic camera.
- **Translucency API:** one `MaterialParams::Translucent` flag over the existing blend state; `Additive` left to #106's `PipelineParams` blend modes. Sort per mesh by transformed centroid.
- **Phase order:** smallest and least overlapping with #106 first (VSync, audio), then fog, then translucency.
