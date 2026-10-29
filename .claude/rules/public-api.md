---
paths:
  - "include/DingoEngine/**"
  - "docs/**"
---

**Public API**
- EnTT, Box2D, Jolt, ImGui, miniaudio and NVRHI never appear in a public header.
- Public APIs use abstract classes, a static `Create()` and opaque handles (see `Physics3D`). GLM in public headers is fine.
- The one exception to CLAUDE.md's comment rule (no code comments unless absolutely necessary): every public declaration in `include/DingoEngine/` gets a `///` line saying what it promises (decision D7 in [the v1.0 plan](../plans/2026-10-08-v1.0-plan.md), line 819; `docs/conventions.md` "Documentation"). The `API reference` workflow fails a PR that leaves one out.
- Docs travel with the code: a public API change updates its `docs/*.md` page in the same PR.
- A change to `include/` or a guide runs `mkdir -p build/docs && doxygen docs/Doxyfile` and leaves `build/docs/warnings.txt` empty.
