---
paths:
  - "src/DingoEngine/UI/**"
  - "include/DingoEngine/UI/**"
---

**DingoUI**
- DingoUI internals (`src/DingoEngine/UI/DingoUI/`) never appear in a public header; `DingoUI.h` holds the params structs and the `DingoUI::` calls only. A texture or font atlas freed while DingoUI draws it goes through the `NvrhiTexture` destroy hook (`DingoUITextureHook.h`), so a new texture-destroying path must call it. `DingoUI.glsl` is an embedded shader (`s_EmbeddedShaders`).
- Engine hotkeys use `IsKeyPressedUnconsumed`; the DingoUI context's destructor and `Application::Destroy` clear the mask (Input statics survive `RequestRestart`).
