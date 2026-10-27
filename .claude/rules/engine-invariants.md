---
paths:
  - "src/DingoEngine/Scene/**"
  - "include/DingoEngine/**"
  - "src/DingoEngine/Asset/**"
  - "src/DingoEngine/UI/**"
  - "src/DingoEngine/Core/**"
  - "src/DingoEngine/Physics/**"
---

**Engine invariants**
- A new built-in component is registered with `DE_INSTANTIATE_COMPONENT` in `src/DingoEngine/Scene/Entity.cpp`, or clients can't use it, and added to `Scene::DuplicateSubtree`'s `CopyComponentIfExists` list (`Scene.cpp`), or duplicates silently drop it. Engine-internal components (`RuntimeComponents.h`, `HierarchyComponent`, `SocketComponent`) are never registered there.
- DingoUI internals (`src/DingoEngine/UI/DingoUI/`) never appear in a public header; `DingoUI.h` holds the params structs and the `DingoUI::` calls only. A texture or font atlas freed while DingoUI draws it goes through the `NvrhiTexture` destroy hook (`DingoUITextureHook.h`), so a new texture-destroying path must call it. `DingoUI.glsl` is an embedded shader (`s_EmbeddedShaders`).
- Public components hold settings only. Never put a live handle (body, controller, sound) on one; those live in `Scene/Systems/RuntimeComponents.h`.
- Caches and lookups key on never-reused ids (`Mesh::GetId()`, `Material::GetId()`, `Framebuffer::GetId()`, `Texture::GetId()`, `Sampler::GetId()`, `Skeleton::GetId()`, `AnimationClip::GetId()`), never on addresses: a freed object's address gets reused.
- The 3D "no body" handle is `k_InvalidBody3D` (0xFFFFFFFF), not 0.
- Scripts never step a character controller and never keep a `CharacterController3D*` across `OnStop`/`OnStart`: controllers are rebuilt on `OnStart`.
- The `AssetManager` owns what it loads: never `Destroy()` or `delete` a managed asset.
- `Application::OnDestroy()` stays in `Run()`'s exit path, never in `Destroy()`, where virtual dispatch no longer reaches the override. GPU teardown goes in `Layer::OnDetach`.
- Any new code that reads a transform as world space goes through `HierarchySystem`.
- A factory returns a ready object or `nullptr`, and `delete` frees everything; engine-only surface is private or under `Internal` (docs/conventions.md). SemVer from 1.0.0 (docs/versioning.md): an API addition waits for a minor release, a removal or rename for a major one.
