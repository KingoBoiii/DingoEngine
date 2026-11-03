---
paths:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.inl"
---

**Code conventions**

| Thing | Convention |
|---|---|
| Namespace | `Dingo` (everything) |
| Members | `m_Name` (instance), `s_Name` (static), `k_Name` or `UPPER_SNAKE_CASE` constants, `UPPER_SNAKE_CASE` macros |
| Backend classes | `Vulkan*`, `DirectX12*`, `Nvrhi*`, `Win*` |
| Log macros | engine `DE_CORE_INFO/WARN/ERROR/ASSERT`; client `DE_INFO/...` |
| Event binding | `DE_BIND_EVENT_FN(fn)`; bit flags via `DE_BIT(x)` |
| Resource creation | fluent `*Params` structs passed to a static `Create()`: `Texture::Create(TextureParams().SetWidth(512)...)` |
| PCH | `depch.h` / `depch.cpp` |

- On-screen text is UTF-8. Only the engine builds with `/utf-8`, so non-ASCII literals in the test app and examples are hex-escaped.
- Prefer `RGBA8_UNORM` for ordinary colour textures.

**C++ and headers** (MSVC and GCC both have to accept it)
- The vendor libraries that never appear in a public header are listed in `.claude/rules/public-api.md`.
- `DE_CORE_ASSERT(cond, msg)` takes a plain string only; format arguments don't compile. `DE_CORE_WARN/ERROR` do take them.
- `<Windows.h>` leaks `min`, `max`, `near` and `far` into test code and public headers. Write `(std::min)(a, b)`, `(std::max)(...)`, `(std::numeric_limits<T>::max)()`, and never name anything `near` or `far`.
- In a public header, a member named after its own type spells the type qualified: `Dingo::Mesh* Mesh`. GCC rejects `Mesh* Mesh`.
- Paths become text only through `Internal::PathToUtf8`/`PathToGenericUtf8`/`PathFromUtf8` (`Core/PathText.h`). Never call `path.string()`/`generic_string()` in the engine, or build a `path` from a `std::string`: on Windows they go through the ANSI code page and throw on a name it can't map. Games and the test app follow the same rule through the public `FileSystem::PathToUtf8`/`PathToGenericUtf8`/`PathFromUtf8`.
