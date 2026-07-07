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

## Loading

```cpp
// Synchronous - ready when the call returns:
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

**Ownership**: the manager owns everything it loads and frees it all at
shutdown (or on `Unload`/`Remove`). Never `Destroy()`/`delete` a managed asset.
Typed getters return borrowed pointers — `AudioClip` is the exception and is
shared (`std::shared_ptr`), matching how `AudioSourceComponent` holds clips.

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
- Query the lifecycle with `GetState(handle)`:
  `Unloaded -> Queued/Loading -> Ready | Failed`.

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

Only file-backed shaders can hot-reload (`Shader::CreateFromSource` inline
shaders have no file to watch). Models, fonts and audio clips are not watched.

Hot-reload is a development feature: it costs a timestamp poll every
`HotReloadInterval` seconds — leave `EnableHotReload = false` in shipping
builds.

## API summary

| Call | Effect |
|---|---|
| `Import(path)` | Register only (no load). `k_InvalidAsset` for unknown extensions. |
| `Load(path)` | Register + load now. |
| `LoadAsync(path)` | Register + load in the background. |
| `Reload(handle)` | Synchronous reload of a registered asset. |
| `Unload(handle)` | Free the object, keep the registration. |
| `Remove(handle)` | Free + forget. |
| `GetTexture/GetShader/GetModel/GetFont/GetAudioClip(handle)` | Typed access; `nullptr`/empty until `Ready`. |
| `Get<T>(handle)` | Template form of the above (not for `AudioClip`). |
| `IsReady/GetState/GetMetadata(handle)`, `FindByPath(path)` | Queries. |
| `GetRegisteredCount/GetLoadedCount/GetPendingCount()` | Stats (debug/loading UI). |

The showcase for all of this is
[examples/ArenaShooter](../examples/ArenaShooter/) — a wave-based top-down
shooter that async-loads everything behind a progress bar and demonstrates
live shader + texture editing.
