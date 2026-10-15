# API conventions

The rules every public header follows, so a type's lifetime and shape never has to be looked up
type by type.

## Lifetime: who frees what

**A factory returns a ready object or `nullptr`, and `delete` frees it.**

- `Create`, `CreateFromFile`, `CreateFromData`, `LoadFromFile` and `Renderer3D::CreateLitMaterial`
  hand back an object that is already initialized. The ones that read a file return `nullptr` when
  it can't be read (the error is logged). They are `[[nodiscard]]`: dropping the result leaks it.
- The caller owns what a factory returns. `delete` (or a `std::unique_ptr`) frees everything the
  object holds, its GPU objects included; `DestroyAndDelete(ptr)` deletes and nulls the pointer in
  one call. `Destroy()` (and `Renderer2D::Shutdown()` or `Renderer3D::Shutdown()`) only frees the
  GPU side early; the object must still be deleted, and deleting it afterwards is safe.
- GPU objects may be deleted at any time: the backend keeps what an in-flight frame still uses until
  the GPU is done with it. Free them in `Layer::OnDetach`, which runs while the renderer is still up.
- `Renderer3D::CreateParticleEmitter` and `CreateStaticBatch` return a `std::shared_ptr`; the last
  reference frees it, and it may outlive the renderer.

**Borrowed objects are never deleted.** Anything handed out by something that owns it:

| Owner | Borrowed objects |
|---|---|
| `Application` | `GetWindow()`, `GetRenderer2D()`, `GetRenderer3D()`, `GetAssetManager()`, `GetAudioEngine()` |
| `AssetManager` | every `Get*` result (textures, shaders, models, fonts, clips) |
| `Model` | its meshes, materials, diffuse textures, skeleton and clips |
| `Renderer` | `GetWhiteTexture()`, the shared samplers, `GetSwapChainFramebuffer()`, `GetPostProcessStack()` |
| `Renderer3D` | `GetDefaultMaterial()`, `GetBoxMesh()` |
| `SceneManager` | the scenes `CreateScene` returns |
| `Scene` | entities, scripts, animators, particle emitters, character controllers, physics worlds |

**One exception:** with a graphics device, `Shader::Create` never returns `nullptr`. A shader that
fails to compile comes back invalid (`IsValid()` is false), because the asset manager and
hot-reload keep an object in place while its file is fixed. Check `IsValid()` instead. Without a
graphics device (a headless app) every GPU factory, `Shader::Create` included, returns `nullptr`
with an error.

**Order:** a lit material is deleted before the `Renderer3D` that made it, an `AudioClip` released
before the engine shuts down, a `ParticleEffect` deleted after its emitters.

## Components hold no ownership

Component fields that point at a resource (`Texture*`, `Font*`, `Mesh*`, `Material*`, `Model*`,
`ParticleEffect*`) never own it. The game or the `AssetManager` keeps the object alive while any
component points at it, and copying or duplicating a component copies the pointer. The one shared
owner is `AudioSourceComponent::Clip`, a `std::shared_ptr<AudioClip>`.

Live runtime objects (bodies, controllers, sounds, animators) are never component fields; read them
through `Scene` (`GetRuntimeBody3D`, `GetCharacterController`, `GetAnimator`...).

## Shapes

- **Params structs** are aggregates with default member initializers and fluent setters that return
  a reference (`TextureParams().SetWidth(512).SetHeight(512)`). Build them either way.
- **Enums** are `enum class`; a bool parameter that isn't obviously on/off is an enum
  (`BufferUpload`, `SamplerFilter`, `IndexFormat`).
- **Handles** are strong types (`AssetHandle`, `AudioSoundId`, `AudioBusId`) with a named invalid
  value (`k_InvalidAsset`, `k_InvalidSound`, `k_InvalidBody2D`, `k_InvalidBody3D`). Stale handles are
  no-ops, and getters return a neutral value for them.
- **Names:** `Get*`/`Set*` for state, `Is*`/`Has*` for bools, `k_` for constants, `ToString(value)`
  for enum names, `2D`/`3D` suffixes where a 2D and a 3D call would otherwise share a name (the
  components leave the 2D name bare, `Entity`'s world-transform calls leave the 3D one bare).
- **Engine-only declarations** live in `namespace Dingo::Internal` or under `src/`. Nothing a game
  needs is there, and none of it is covered by [versioning](versioning.md).

## Documentation

Every public declaration in `include/DingoEngine/` carries a `///` line (`///<` after a field)
saying what it promises beyond its signature: units and spaces, what a default means, who owns
what and for how long, which thread may call it, and what it returns when it fails. A run of
fluent setters shares one line in a `///@{ ... ///@}` group. Doxygen builds these into the API
reference, with these guides as its pages:

```bash
mkdir -p build/docs && doxygen docs/Doxyfile   # HTML in build/docs/html
```

The `API reference` workflow fails a pull request that leaves a declaration undocumented or a
guide's link unresolved, and publishes the reference to GitHub Pages from master.
