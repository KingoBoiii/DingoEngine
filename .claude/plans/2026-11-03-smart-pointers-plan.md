**Status**: open

# Smart pointers for owned resources

## Context

Public factories return raw owning pointers, and every owner frees them by hand. `docs/conventions.md` ("Lifetime: who frees what") states the rule: the caller owns what a factory returns, and `delete`, a `std::unique_ptr` or `DestroyAndDelete(ptr)` (`Renderer.h:47`) frees it. Each destructor already frees its GPU objects, and the backend keeps whatever an in-flight frame still uses. So `std::unique_ptr` works today; the API just doesn't hand one out.

**What that costs.** Leaks and double frees are possible wherever a pointer is freed by hand, and a `T*` doesn't say whether it is owned or borrowed. Today that is written down only in the conventions table.

**Size** (2026-11-03, v1.2.0):
- 22 public raw factories: `Texture::Create`/`CreateFromFile`/`CreateFromData`, `Shader::Create`/`CreateFromFile`, `Model::LoadFromFile` (×2), `Material::Create` (×2), `Mesh`, `Sampler`, `Framebuffer`, `GraphicsBuffer`, `Pipeline`, `RenderPass`, `ComputePass`, `Font`, `ParticleEffect`, `Renderer3D::CreateLitMaterial`. `AudioEngine::Create` and `GraphicsContext::Create` are engine-owned through `Application`.
- Engine: 92 `DestroyAndDelete` calls plus 66 `delete`/`->Destroy()` sites in 20 files. The largest are `MiniAudioEngine.cpp` (20), `Application.cpp` (10), `AssetManager.cpp` (7), `Renderer2D.h` and `Renderer3D.cpp`.
- Examples and the test app: 208 `DestroyAndDelete` calls in 37 files.

**Already smart**: `Renderer3D::CreateParticleEmitter` and `CreateStaticBatch` return `std::shared_ptr`, and the engine uses `unique_ptr`/`shared_ptr` internally in about 130 places.

**Decisions (proposed, waiting for the owner):**
- D1. Use `std::unique_ptr<T>` directly, with no `Scope<T>`/`Ref<T>` alias. `conventions.md` already names it.
- D2. **A raw pointer always means borrowed.** Anything that owns uses `unique_ptr`. Borrowed results (`AssetManager::Get*`, `Model`'s meshes, `Renderer::GetWhiteTexture()`, and the rest of the conventions table) stay raw.
- D3. Phases 1 and 2 change no public API and can ship in 1.x. Only phase 3 changes factory return types, so it lands in **2.0**, the same way the existing "removed in 2.0" deprecations (`AssetTypes.h:64`) are handled. *Alternative: new `CreateUnique` names in 1.x. Rejected: two names per factory, then a rename in 2.0.*
- D4. No new `shared_ptr`. NVRHI already reference-counts the GPU objects, and shared ownership on top would hide when they get freed. GPU teardown order matters (it happens in `Layer::OnDetach`, before the device goes).
- D5. No new showcase example. Phase 2 converts every example, and they become the pattern.

---

## Phase 1: engine internals (no public change)

1. Change every owning raw member in `src/` and in the private members of public headers (for example `Renderer2D.h:296-313`) to `std::unique_ptr`. Replace `DestroyAndDelete`/`delete` with `.reset()` or let destruction handle it.
2. **Where teardown order matters, keep it written out.** Members are destroyed in reverse declaration order *after* the destructor body. Wherever the GPU side has to go before the device or the renderer, call `.reset()` explicitly in the same place the `delete` is today: `Renderer3D::Shutdown`, `Renderer2D::Shutdown`, `Application`'s teardown (`GraphicsContext` last), `ImGuiLayer::OnDetach`, and the swap chains' `Destroy()`.
3. `MiniAudioEngine.cpp` and `MiniAudioVfs.cpp`: C-struct allocations (`ma_*`) get a `unique_ptr` with a custom deleter only where it's simpler. Otherwise leave them and record why.
4. `AssetManager` storage: entries own through `unique_ptr`, and `Get*` keeps returning raw (D2).
5. Ownership comments that only say "owned" or "borrowed" are deleted. The type says it now.

**Risk**: teardown order. This is engine-internal lifetime, so it gets an `engine-reviewer` pass.
**Verify**: Linux Debug, every project. Run the test app's renderer, assets, audio and scene cases. Then run one example to its end and look for `LOG ERROR` or `VUID-` at shutdown. Needs a GPU: a full start/quit cycle on Vulkan, DX11 and DX12 with validation on, plus a window resize and an asset hot-reload.

## Phase 2: examples and the test app (no public change)

1. Replace every `DestroyAndDelete(m_X)` member pattern with `std::unique_ptr<T> m_X{ T::Create(...) }` (208 calls, 37 files). `OnDetach` keeps an explicit `.reset()` only where order matters (a lit material before `Renderer3D`, a `ParticleEffect` after its emitters: `conventions.md`, "Order").
2. Calls that pass a resource on use `.get()`. Borrowed pointers stay raw.
3. `docs/conventions.md`: make `std::unique_ptr` the recommended way to hold a factory result, and turn `DestroyAndDelete` into a note.

**Verify**: Linux Debug, every project. Run each changed example's `--check` and the test app's cases that changed. Needs a GPU: the examples' `--check` runs on the three backends (the `pr-gpu-test` Examples jobs).

## Phase 3: factories return `std::unique_ptr` (2.0, breaking)

1. All 22 factories return `std::unique_ptr<T>` and stay `[[nodiscard]]`. `Shader::Create`'s "never null with a device" rule stays as it is.
2. `AudioEngine::Create` and `GraphicsContext::Create` follow, since `Application` holds them.
3. `DestroyAndDelete`: mark it `[[deprecated("hold a std::unique_ptr; removed in 2.0")]]` in the last 1.x release, and remove it in 2.0.
4. Docs: `conventions.md`, every `docs/*.md` code sample that calls a factory, and a new `docs/migrating-to-2.0.md` section with the one-line change (`T* x = T::Create()` → `auto x = T::Create()`, with `.get()` where it's passed on). Update the `///` docs on each factory; Doxygen must stay warning-free.
5. Phase 2 has already converted the examples and the test app, so here they only lose their now-redundant `std::unique_ptr<T>{ ... }` wrappers.

**Verify**: Linux Debug, every project, and the Doxygen check. Needs a GPU: the same as phase 2.

---

## Left out

- **Handles instead of pointers** for GPU resources. That's a bigger redesign. `AssetManager` already uses handles for the assets it manages.
- **An intrusive `Ref<T>`** (Hazel-style). Ruled out by D4.
- **Borrowed-pointer wrappers** (`observer_ptr`, `not_null`). D2 makes raw mean borrowed, which is enough.
- **Leak detection.** That's the memory stats plan (`2026-10-14-memory-stats-plan.md`). Its per-category counts are a good before/after check for phase 1.

"As built" notes are added here per phase as the work lands.
