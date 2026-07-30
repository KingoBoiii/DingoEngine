# Asset Pipeline (v0.6)

The `AssetManager` is the engine's central registry and owner of file-backed
assets: textures, shaders, models, fonts, and audio clips. It replaces ad-hoc
calls to the raw `Create*()` factories with **stable UUID handles**, **path
deduplication**, **background loading**, and **hot-reload** of shaders and
textures during development.

The raw factories (`Texture::CreateFromFile`, `Model::LoadFromFile`,
`Font::Create`, `Shader::CreateFromFile`, `AudioEngine::LoadClip`) still exist
and are the right tool for *unmanaged* resources — render targets, generated
data, one-off internals. Everything a game loads from disk should go through
the manager.

## Setup

The manager is engine-owned; every `Application` has one. Configure it through
`ApplicationParams::Assets`:

```cpp
ApplicationParams params;
params.Assets.RootDirectory = "assets";   // default
params.Assets.EnableHotReload = true;     // development builds only
params.Assets.HotReloadInterval = 0.5f;   // seconds between file polls
```

Every asset path you pass to the manager is **relative to the root directory**.
A relative root is resolved against the working directory once at startup, so a
game can anchor it to the executable's directory (or a CLI argument) instead of
depending on where it was launched from — the classic cwd-relative asset trap.
If the root does not exist at startup, a warning is logged immediately.

Access it anywhere:

```cpp
AssetManager& assets = Application::Get().GetAssetManager();
```

## Handles

Assets are identified by `AssetHandle` — a random 64-bit UUID assigned when a
path is first registered. Handles are stable for the lifetime of the manager
and survive unload/reload cycles (including hot-reload), so game code can hold
handles instead of tracking raw pointers.

- `k_InvalidAsset` is the null handle; test with `IsValidAssetHandle(h)`.
- The same path always maps to the same handle: loading
  `"sprites/player.png"` twice returns the same handle *and the same loaded
  object* — no duplicate file reads, no duplicate GPU textures.
- Paths are matched by their normalized spelling, so an absolute path under the
  asset root, its relative form, and (on Windows, where the filesystem ignores
  case) `Sprites/Player.png` all name the same registration.

## Loading

```cpp
// Synchronous - Ready when the call returns:
AssetHandle tex  = assets.Load("sprites/player.png");
AssetHandle font = assets.Load("fonts/arialbd.ttf");

Texture* texture = assets.GetTexture(tex);      // or assets.Get<Texture>(tex)
Font*    hud     = assets.GetFont(font);
std::shared_ptr<AudioClip> clip = assets.GetAudioClip(assets.Load("audio/shoot.wav"));
```

Asset types are inferred from the file extension:

| Type | Extensions |
|---|---|
| `Texture2D` | `.png` `.jpg` `.jpeg` `.tga` `.bmp` |
| `Shader` | `.glsl` `.shader` |
| `Model` | `.obj` `.gltf` `.glb` `.fbx` |
| `Font` | `.ttf` `.otf` |
| `AudioClip` | `.wav` `.ogg` `.mp3` |

`Load` is synchronous with one caveat: if the same path already has a
`LoadAsync` in flight it does **not** block on it — the call returns the handle
and the background pass publishes as usual, so poll `IsReady(handle)` when you
mix the two for one asset.

**Ownership**: the manager owns everything it loads and frees it all at
shutdown (or on `Unload`/`Remove`). Never `Destroy()`/`delete` a managed asset.
Typed getters return borrowed pointers — `AudioClip` is the exception and is
shared (`std::shared_ptr`), matching how `AudioSourceComponent` holds clips.

**Threading**: the manager is main-thread-only — the thread that drives
`Application::Run`, where the per-frame pump finalizes background loads. The
only exceptions are `GetPendingCount()` and `GetReloadingCount()`, which read
atomics. Decoding happens on an internal worker thread, but that thread never
touches the registry or a loaded object, so calling a getter from your own job
thread races the pump.

**Failure contract**: a failed load logs an error and keeps the asset
registered with `State == Failed`; the typed getters return `nullptr`. A later
`Reload(handle)` — or a hot-reload after the file is fixed — recovers without
re-registering.

## Background loading

```cpp
// Kick everything off at startup - returns immediately:
m_Player  = assets.LoadAsync("sprites/player.png");
m_Music   = assets.LoadAsync("audio/theme.ogg");
m_Duck    = assets.LoadAsync("models/duck.gltf");

// Each frame, until done:
if (assets.GetPendingCount() == 0) { /* everything Ready - start the game */ }
float progress = 1.0f - (float)assets.GetPendingCount() / (float)totalQueued;
```

- **Textures and audio clips** decode on a background loader thread; the GPU
  upload / publish happens on the main thread inside the engine's per-frame
  pump, so no game code ever races the renderer.
- **Shaders, models and fonts** load on the main thread instead (their loaders
  create GPU resources internally), amortized **one asset per frame** so a
  loading screen keeps animating between them.
- Typed getters return `nullptr` until the asset is `Ready` — draw a
  placeholder or a loading screen, and poll `IsReady(handle)` /
  `GetPendingCount()`.
- `GetPendingCount()` counts `LoadAsync` requests only. Hot-reloads are counted
  by `GetReloadingCount()` instead, so a mid-game reload cannot stall a loading
  bar gating on the former.
- Query the lifecycle with `GetState(handle)`:
  `Unloaded -> Queued/Loading -> Ready | Failed`, plus `Ready -> Reloading ->
  Ready` for an in-place reload (see below).

## Hot-reload (shaders & textures)

With `EnableHotReload` on, the manager polls the source files of every loaded
**texture** and **shader** and reloads them in place when they change on disk:

- **Textures** re-decode on the loader thread and swap their contents inside
  the *same* `Texture` object — every `Texture*` held by game code (and every
  cached binding set, via `NativeEquals`) keeps working, even if the new image
  has different dimensions.
- **Shaders** recompile from source, bypassing and rewriting the bytecode disk
  cache. On success the shader's *generation* is bumped and every `Pipeline` /
  `RenderPass` built from it lazily rebuilds on its next bind — no game code
  involved. On a **compile error** the previous program keeps running and the
  error is logged; fix the file and save again.

While a texture re-decodes, its state is `Reloading` rather than `Loading`: the
previously loaded object keeps serving, so `IsReady(handle)` stays **true** and
the typed getters keep returning the same live pointer. A save by an artist
never makes a sprite blink out for a few frames.

Only file-backed shaders can hot-reload (`Shader::CreateFromSource` inline
shaders have no file to watch). Models, fonts and audio clips are not watched.

Hot-reload is a development feature: it costs a timestamp poll every
`HotReloadInterval` seconds — leave `EnableHotReload = false` in shipping
builds.

`AssetManager::Reload(handle)` applies the same in-place refresh for textures and
shaders, so you can force a reload from your own code (or the debug panel below)
without invalidating anything. Every other type is destroyed and recreated by
`Reload`, which *does* invalidate borrowed pointers. It is a no-op while a load
or hot-reload of that asset is already in flight — cancelling one would only
throw away a decode that is about to publish the same file.

## The Assets debug panel

Press **F6** for the Assets tab of the engine's built-in Debug window (or embed
`UI::AssetSummarySection()` / `UI::AssetRegistrySection()` in your own window —
see [DebugPanels.h](../include/DingoEngine/UI/DebugPanels.h)). It shows the asset
root, live registered/loaded/in-flight counts with a load-progress bar, per-type
and per-state breakdowns, and every registration in a filterable table with its
state, type and source path.

It is also a control surface: toggle hot-reload for the session, press **Reload**
on a texture or shader row (in place, so it is safe while the game is running),
or retry everything that failed. **Reload** is disabled for models, fonts and
audio clips - reloading those destroys and recreates the object, which would
invalidate pointers game code is holding - and **"Reload all loaded"** skips
them for the same reason. There is deliberately no Unload button — unloading
frees the object while game code may still hold the pointer it was handed.

## API summary

| Call | Effect |
|---|---|
| `Import(path)` | Register only (no load). `k_InvalidAsset` for unknown extensions. |
| `Load(path)` | Register + load now (unless a background load is already in flight). |
| `LoadAsync(path)` | Register + load in the background. |
| `Reload(handle)` | Synchronous reload. Textures and shaders refresh **in place** (pointers stay valid); other types are recreated. No-op while a load is in flight. |
| `Unload(handle)` | Free the object, keep the registration. Invalidates borrowed pointers. |
| `Remove(handle)` | Free + forget. |
| `GetTexture/GetShader/GetModel/GetFont/GetAudioClip(handle)` | Typed access; `nullptr`/empty until the asset has a loaded object. An in-place reload keeps serving the live pointer. |
| `Get<T>(handle)` | Template form of the above (not for `AudioClip`). |
| `IsReady/GetState/GetMetadata(handle)`, `FindByPath(path)` | Queries. `IsReady` stays true across an in-place reload. |
| `GetRegisteredCount/GetLoadedCount/GetPendingCount/GetReloadingCount()` | Stats (debug/loading UI). |

The showcase for all of this is
[examples/ArenaShooter](../examples/ArenaShooter/) — a wave-based top-down
shooter that async-loads everything behind a progress bar and demonstrates
live shader + texture editing.
