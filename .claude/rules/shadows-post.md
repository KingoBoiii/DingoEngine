---
paths:
  - "**/Shadow*"
  - "**/Post*"
  - "**/FullscreenPass*"
  - "**/Renderer3D_Shadow*.glsl"
  - "**/Shadows.glsl"
  - "**/ToneMapping.glsl"
  - "**/Fullscreen.glsl"
---

**Shadows and post**
- `ShadowData` is uploaded every `EndScene`, and before the first casting scene a 1x1 placeholder depth texture is bound.
- The tone map's slot 1 holds level 0, or a 1x1 black texture while bloom is off (switched only on a toggle, since it rebuilds the material's pipelines), times `Bloom.Intensity` (0 when off).
