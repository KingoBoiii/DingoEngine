---
paths:
  - "**/Animator*"
  - "**/Skeleton*"
  - "**/AnimationClip*"
  - "**/Skinning.glsl"
  - "src/DingoEngine/Scene/Systems/Animation*"
---

**Animation**
- `AnimationSystem::Update` calls `Animator::ClearEventsThisFrame` for every animator it skips as disabled, so a poller never re-reads the frame it stopped on (v0.8.3, #81).
- Clip bindings are cached per `AnimationClip::GetId()` (never reused) and looked up by id, never pointed at, so an `Animator` copies safely.
