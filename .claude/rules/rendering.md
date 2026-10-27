---
paths:
  - "src/DingoEngine/Graphics/**"
  - "include/DingoEngine/Graphics/**"
---

**Rendering**
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` is workspace-wide: never enable SPIRV-Cross `fixup_clipspace`.
- Bump `k_ShaderCacheFormatVersion` (`NvrhiShader.cpp`) when shader compile options or the toolchain change, and `k_FontAtlasCacheFormatVersion` (`Font.cpp`) when the font charset changes.
- A new engine shader joins `s_EmbeddedShaders` in `EngineShaders.cpp`, or Release builds can't find it.
- The scene UBO (`Renderer3D::CameraData`, mirrored in `Renderer3D_Lit.glsl`) is append-only; custom shaders rely on its first 96 bytes. Material bindings: 0 = scene UBO, 1 = the material's `SetUniform` block, 2+ = textures/samplers, and the binding set must match the shader's reflection exactly.
- A volatile uniform buffer is written in every frame that binds it. If it isn't, Vulkan reads stale data and D3D12 removes the device.
- The swap-chain render pass stays `eLoad`/`eStore` for colour and depth. The Vulkan acquire wait is queued just before the drawing commands execute, never at acquire. Never call `SwapChain::SetVSync` from the main thread while the render thread runs.
- An old Debug executable runs the *current* `Renderer3D_Lit.glsl` from disk; hide the file to A/B old code.
