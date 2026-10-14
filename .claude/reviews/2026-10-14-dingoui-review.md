# Code review — v1.0 DingoUI (game UI, U0–U8)

**Date:** 2026-10-14 · **Branch:** `claude/practical-keller-3idpy4` at `d7f9db3` (71 files, +12490/−206 against the plan's base `6cc2dd8`) · **Scope:** everything the game-UI milestone added, U0–U8, as `git diff 6cc2dd8..HEAD`: shared text layout, `Scene::WorldToScreenPoint`, the Input consume mask and ImGui capture, the DingoUI context, batcher and shader, layout, widgets, styles and 9-slice, focus navigation, tweens and root transitions, world roots, the F9 tab and stress mode.

**Method:** one fresh reviewer who wrote none of the code, reading against CLAUDE.md's rules and the plan (`.claude/plans/2026-10-08-v1.0-game-ui-plan.md`, §2, §7 and the "As built" notes, which win). Each finding is marked **CONFIRMED** (code path traced end to end) or **PLAUSIBLE** (needs a runtime repro). Runs on this box (Linux, Mesa llvmpipe under Xvfb): Debug build of the engine, test app and unit tests (no warnings); `--test=dingoui` 448 pass, 0 `[FAIL]`, no `VUID-` or `LOG ERROR`; `Dingo-UnitTests` 153/153; Doxygen with an empty `build/docs/warnings.txt`; a hash microbenchmark (g++ `-O2`) for [F1](#f1). Nothing was built with MSVC or run on a GPU, D3D11 or D3D12, and nothing was run under ASan.

Items the plan's "As built" notes already leave open (U9's metrics cache idea, no GPU numbers, overlay only with the tab open) are not repeated, except where a finding gives them a file and a fix.

---

## Fix status {#fix-status}

| Finding | Commit | Verified how |
|---|---|---|
| [U1](#u1) | fixed (commit pending) | `Internal::DingoUIOnTextureDestroyed` (`DingoUITextureHook.h`), called from `NvrhiTexture::Destroy` and its destructor, so a font's atlas is covered: an exiting root whose kept commands draw the texture is erased, a still-declared one's `Kept` is emptied (dropped if it then exits), and the batcher drops its cached upload when a batch binds it. No-op without a context. `UIDrawList::AddText` now skips a font with no atlas, so every kept font is freed through the hook. `--test=dingoui` "Free:" checks (font freed after the last declared frame, texture freed mid-exit, next frame rebuilds); not run under ASan. `docs/game-ui.md` says freeing mid-exit is safe and ends the exit. |
| [F1](#f1) | fixed (commit pending) | `UIDrawList::Hash` mixes 8-byte words: one header word (kind, flags, text length), one id word, each command's floats as one block, text in 8-byte chunks with a sized tail. New `UIRenderStatistics::HashMs`, shown in the F9 DingoUI tab. Existing cache-hit/upload checks in `--test=dingoui` and the 153 unit tests pass; not re-benchmarked. |
| [N1](#n1) | fixed (commit pending) | `UIRootParams::Navigable` is off by default; only a `Modal` root or `SetNavigable(true)` joins focus navigation (`DingoUILayout.cpp` `BeginRootScaled`). The press that reveals the ring is not consumed, and Back is consumed only while the ring shows or a modal is up. HUD recipe in `docs/game-ui.md`. `--test=dingoui` stages: HUD root takes no arrows/Back/Space, the reveal press stays with the game, a modal takes Escape; the menu stages call `SetNavigable(true)`. |
| [N2](#n2) | fixed (commit pending) | `ReadNavInput` records the keys and pad buttons actually down (or pressed) this frame in `UINavInput`; `ConsumeNavInput` consumes only those. The stick is not consumed (documented; games gate their own read with `DingoUI::IsCapturingInput()`). Test-app stage: a held Space stays with the game; unit case for a single direction key. |
| [F2](#f2) | fixed (commit pending) | `UITextMetricsCache` (`DingoUITextCache.h`): two generations keyed on atlas id, size, max width and text hash, `string_view` lookup, no allocation on a hit. 4 unit cases, plus a stress check that the still menu measures no text anew from its fourth frame. Release `--dingoui=stress`, menu500 (3 runs): DingoUIMs 0.160 / 0.180 / 0.179 (HEAD with F1: 0.196-0.200; before F1: 0.216-0.250), layout 0.087-0.100 (HEAD 0.111-0.113); heavy 3.4-4.0 ms (before 4.0-5.1). Still above the 0.1 ms menu budget. |
| [F3](#f3) | fixed (commit pending) | `UIElement::State` holds the `UIWidgetState*` found at declare time, so `EndFrame` and `BeginWidgetBox` do not look it up again; the default-seen and declared-id scratch containers are members reused every frame. |
| [A1](#a1) | fixed (commit pending) | `DingoUIParams::DefaultFont` removed; `DingoUI::SetDefaultFont` is the only way. Test app, docs and header comments updated; no example used the field. |
| [D1](#d1) | fixed (commit pending) | `docs/game-ui.md` names `Input::GetMouseScrollDelta` and no longer mentions `UIInputState::DeltaTime`. |
| [I1](#i1) | fixed (commit pending) | Docs corrected, `LayerStack` unchanged (owner's call). `Application::Initialize` pushes the DingoUI and ImGui layers last, so only overlays pushed after start-up sit above them; `docs/application-and-layers.md` and `.claude/reference/engine-patterns.md` say so. |

---

## Triage summary

| ID | Severity | Area | Finding | Status |
|---|---|---|---|---|
| [U1](#u1) | High | Lifetime / renderer | An exiting root's kept commands dereference textures and fonts the game has already freed | fixed |
| [N1](#n1) | Medium | Input core | With default params, one focusable HUD widget takes the arrows, Space, Enter, Escape, Tab and the pad's D-pad/A/B from the game | fixed |
| [F1](#f1) | Medium | Performance / renderer | The vertex-cache hash runs byte by byte every drawn frame, outside every phase timer | fixed |
| [F2](#f2) | Medium | Performance | Every text is re-measured through msdf's maps every frame, with a string copy per measure | fixed |
| [F3](#f3) | Low | Performance | A second state lookup per widget and two heap allocations every frame | fixed |
| [N2](#n2) | Low | Input core | Nav consumption takes every key bound to an action, not the one pressed; the stick is never consumed | fixed |
| [I1](#i1) | Low | Input core / docs | Game overlays sit above the ImGui and DingoUI layers in event order, against the docs | fixed |
| [A1](#a1) | Low | API | `DingoUIParams::DefaultFont` can't be used: no `Font` can exist before the `Application` | fixed |
| [D1](#d1) | Low | Docs | `docs/game-ui.md` names `Input::GetScroll` (doesn't exist) and the internal `UIInputState` | fixed |

Checked and found sound (no finding):
- **Consume mask** (`Input.cpp` `Update`, ~:228-253): a consumed key, button or pad button reads as up from press to release, the release frame included, and the next press after it is visible. `IsAny*` honour it. Engine F3–F9 keys use `IsKeyPressedUnconsumed`. `Resume` and `RequestRestart` paths leave no stuck bits (`Application.cpp:225-231`, the context's destructor).
- **Games without DingoUI**: an empty tree has no hit, no blocking rect and no nav candidates (`ResolveNavigation` returns at :251-259 before any consumption), and the batcher is never called on an empty list (`DingoUIContext.cpp:696`). Hit-testing only counts elements that draw (`DingoUILayout.cpp:82-85`), so a transparent full-screen layout root blocks nothing.
- **Renderer**: `DingoUI.glsl` declares bindings 0 (UBO), 1 (`texture2D[32]`), 2 and 3 (samplers), and the batcher sets exactly those, with white in every unused slot (`DingoUIBatcher.cpp:127-144`). The volatile UBO is written in every frame that draws (:165). The cache key covers the format key, target size, every texture's never-reused id, 9-slice texture size and the text bytes; a shader reload rebuilds the pipeline and drops the cache (`GetTargetPipeline`); skipped frames record nothing (`IsFrameSkipped`, and `Renderer::Upload` is a no-op then); grown buffers retire for 8 frames. The texture-array indexing is the same `nonuniformEXT` pattern Renderer2D's quad shader already ships on D3D11/D3D12. The shader is in `s_EmbeddedShaders`. Headless never creates a context and every `DingoUI::` call is null-guarded.
- **Destroy order**: `UnwatchUnmanagedShader` after the AssetManager is deleted only touches a static vector, as Renderer3D's shaders already do. `UIRootTransitions` holds entries in an `unordered_map`, so the `Entry*` list is stable across the frame.
- **Text layout**: `Renderer2D::DrawText` and `Font::GetStringWidth` give the same output as before for callers that don't set `MaxWidth`, apart from the documented missing-glyph fix.
- **Public headers**: no vendor types, `(std::numeric_limits<float>::max)()`, appended aggregate fields only (`ApplicationParams::DingoUI`, `FrameTimings::DingoUIMs`, `Renderer2DTextParams::MaxWidth`), Doxygen clean.

---

## High

### U1 — An exiting root's kept commands dereference textures and fonts the game has already freed {#u1}

`DingoUIContext.cpp:733-741, 771`, `DingoUITween.h:62-69`, `DingoUIDrawList.h:184-185`, `DingoUIDrawList.cpp:271, 279, 283, 371`, `DingoUIBatcher.cpp:139-144`, `NvrhiRenderPass.cpp:81`. CONFIRMED (traced; not run under ASan).

**What happened:**
- A root with a `UITransition` keeps its last drawn commands in `Entry::Kept` (`std::swap(entry->Kept, m_RootScratch)`, :771). The commands hold raw `Dingo::Texture* Image` and `const Dingo::Font* TextFont` pointers.
- When the game stops declaring the root, `AdvanceExiting` keeps the entry for the exit duration and `emitExiting` (:733-741) appends `Kept` to every frame's draw list until it ends.
- `UIDrawList::Hash` then calls `command.Image->GetId()` (:271, :279) and `command.TextFont->GetAtlasTexture()` (:283) every frame. On a cache miss, `Build` dereferences the font again (:371) and the batcher binds the raw `Texture*` (`DingoUIBatcher.cpp:139-144`), which `NvrhiRenderPass::RefreshReloadedTextures` dereferences at draw time (`NvrhiRenderPass.cpp:81`).
- The docs promise this is safe: "The texture must stay alive until the frame has been drawn" (`docs/game-ui.md:98`), and for the default font, "unset it before you destroy it" (:42). Closing a menu that has a transition and unloading its textures or font in the same frame (a level change, say) reads freed memory for the whole exit (0.3 s in the test app).
- **Repro in the test app:** run `--test=dingoui` until the live page shows. Its `live_fade` root is open by default (`DingoUITest.h:155`) and has `Fade(0.3f)` with text in `m_Font` (`DingoUITest.cpp:2692-2697`). Then click any other test in the Tests panel. `TestLayer` runs `Cleanup()` (`TestLayer.cpp:248-254`), which destroys `m_Font`, `m_ImageTexture` and `m_SliceTexture` (`DingoUITest.cpp:181-187`) but leaves the context's transitions alone. The next `EndFrame` exits `live_fade` and hashes its text through the freed `Font`.

**Fix:** add an engine-internal hook, `Internal::DingoUIOnTextureDestroyed(const Texture*)`, called from texture destruction while a context exists. It erases every `UIRootTransitions` entry whose `Kept` references the texture, as an image or as a text command's atlas, and clears the batcher's `m_HasUpload`.
- A font's atlas is destroyed in `Font::Destroy`, so this one hook covers fonts too.
- Other kept fonts are still alive when the hook runs: any font freed earlier already dropped its entries.
- Exiting entries are few, so the scan is cheap.
- Add a check in the transition stage that frees a texture on a root's last declared frame and draws the exit.
- Leave `docs/game-ui.md:98` as it is once the hook lands.

---

## Medium

### N1 — With default params, one focusable HUD widget takes the arrows, Space, Enter, Escape, Tab and the pad's D-pad/A/B from the game {#n1}

`DingoUI.h:471, 478, 648`, `DingoUIContext.cpp:245-246, 354-378`, `DingoUINav.cpp:194-218`, `docs/game-ui.md:237-239`. CONFIRMED (traced).

**What happened:**
- A screen root defaults to `BlocksInput = true` and `Navigable = true`, and a widget to `Focusable = true`. So one HUD button (pause, an inventory slot) makes its root a nav candidate, and `ResolveNavigation` gets past its early return.
- Every press of a bound key sets `fired.*`, and `ConsumeNavInput` runs whatever the focus state (:378). That includes the press that only reveals the focus ring (:357-358), and Back with no widget focused (:354). A consumed key reads as up until it is released.
- What a keyboard-and-mouse game with a HUD pause button sees, all on default params:
  - the arrow keys (movement) read as up for as long as they're held;
  - the first Space (jump) shows the ring, and the second one clicks the focused HUD button (:362, `Clicked`);
  - Escape never reaches `Input::IsKeyPressed`;
  - the pad's D-pad, A and B are taken the same way.
- Moving the mouse hides the ring again (:245-246), so the cycle starts over.
- The docs say a HUD avoids this with `BlocksInput(false)` (:237-239). But then a click on the HUD's button also reaches the game, because pointer consumption only happens over blocking roots. The only correct HUD is `BlocksInput(true)` plus `SetNavigable(false)`, which is neither the default nor documented.

**Fix** (the owner's call: it changes U5's design): navigation should take keys only from a UI that is acting as a menu.
- Default `Navigable` to on only for `Modal` roots, or for roots that hold a `DefaultFocus` widget or a `SetFocus` target. Menus opt in.
- Consume nothing on the press that reveals the ring, and consume Back only while focus is visible or a modal is up.
- Document the HUD recipe in `docs/game-ui.md` under "Input".

### F1 — The vertex-cache hash runs byte by byte every drawn frame, outside every phase timer {#f1}

`DingoUIDrawList.cpp:17-37, 252-296`, `DingoUIBatcher.cpp:97`. CONFIRMED by benchmark; the share of the 0.24 ms is PLAUSIBLE.

**What happened:**
- `UIDrawList::Hash` runs on every drawn frame, cache hit or not. It feeds every byte through an FNV multiply chain, and every float field is its own 4-byte `Bytes` call.
- It runs before `BuildMs`, `UploadMs` and `DrawMs` start, so the F9 tab and U8's table never show it. U8's Release menu500 run gives `DingoUIMs` 0.240 ms against a phase sum of 0.177 ms (begin 0.029, layout 0.115, emit 0.022, draw 0.011), leaving 0.063 ms untimed.
- On this box, byte-wise FNV over 80 KB takes 0.118 ms and an 8-byte-word mix takes 0.022 ms (5.4×). The 0.063 ms gap fits the hash at that byte rate.

**Fix:** mix 8-byte words (`memcpy` into a `uint64_t`, `h = (h ^ w) * 0x9E3779B97F4A7C15; h ^= h >> 32`). Hash each command's plain fields as a block of words, and the text in 8-byte chunks with a byte tail. Keep the seed (format key and size). Time the hash too (a `HashMs`, or inside `BuildMs`) so the tab accounts for the whole frame.

### F2 — Every text is re-measured through msdf's maps every frame, with a string copy per measure {#f2}

`DingoUIContext.cpp:37-44`, `DingoUILayout.cpp:383-389, 391-406, 417, 482-484`, `vendor/msdf-atlas-gen/msdf-atlas-gen/FontGeometry.cpp:184-218`. CONFIRMED (traced).

**What happened:**
- Layout is the largest phase (0.115 ms of the Release menu500 frame), and text measurement dominates it (plan, U8 as built).
- Each frame measures every text at least once (`FitAxis`, :417), wrapping text again at its resolved width (:484), and a wrapping `Fit` text inside a row once per word for its min-content width (:402).
- Each measure copies the `string_view` into `m_MeasureScratch` (:42), because `Font::MeasureText` takes a `const std::string&`. It then walks `TextLayout`, which costs about four `std::map` lookups per character in msdf-atlas-gen: `getGlyph` twice, plus the kerning find.
- A still menu measures the same strings at the same sizes every frame.

**Fix:**
- Cache inside `m_Measure`, keyed on (atlas texture `GetId()`, which is never reused; the size's bits; `maxWidth`'s bits; a 64-bit hash of the text) and returning the `vec2`.
- Keep two generations, current and previous, swapped in `EndFrame`, so an entry unused for a frame is dropped.
- Give the internal `TextLayout` path a `string_view` entry so the copy goes away.
- Re-measure against §2.5 after this and [F1](#f1).
- If the budget still misses, the plan's other option is to skip layout when the declaration hash is unchanged.

---

## Low

### F3 — A second state lookup per widget and two heap allocations every frame {#f3}

`DingoUIContext.cpp:672-681, 317, 723`. CONFIRMED.

**What happened:**
- After layout, `EndFrame` does a second `m_States.Find` for every interactive or scroll element to store `LastRect` (:672-681), although `Interact` already held the state pointer.
- `ResolveNavigation` builds a fresh `std::unordered_set` of default-focus ids (:317) on every frame that has nav candidates.
- `BuildDrawList` allocates a `declared` vector (:723) every frame.

**Fix:**
- Store the `UIWidgetState*` on the `UIElement` in `Interact`. The pointers stay valid until `Evict`, which runs after the loop.
- Make `defaults` and `declared` members that are cleared rather than rebuilt.

### N2 — Nav consumption takes every key bound to an action, not the one pressed; the stick is never consumed {#n2}

`DingoUINav.cpp:194-218`. CONFIRMED.

**What happened:**
- When Accept fires, all of `AcceptKeys` is consumed (Enter, Space and KP Enter by default). So pressing Enter while holding Space (a held jump, a held throttle) makes Space read as up until it's released.
- A keyboard direction also consumes the pad's D-pad button on `fired.Gamepad`.
- A left-stick direction moves focus, but the axis is never consumed, so the game's character walks while focus moves.

**Fix:**
- Consume only the keys and buttons that are down this frame. `ReadNavInput` already knows which source fired, so record it in `UINavInput`.
- For the stick, document that `GetGamepadLeftStick` is not masked, or have a menu root report `IsCapturingInput` so games gate on it.

### I1 — Game overlays sit above the ImGui and DingoUI layers in event order, against the docs {#i1}

`LayerStack.cpp:30-33`, `Application.cpp:166-173`, `docs/application-and-layers.md:412-413, 422-423`, `.claude/reference/engine-patterns.md:14`. CONFIRMED.

**What happened:**
- `PushOverlay` appends, and events go top-down from the end. An overlay a game pushes in its constructor, which runs after `Application`'s, therefore sits above `DingoUILayer` and `ImGuiLayer`.
- Its `OnEvent` sees a press over an ImGui window or a blocking DingoUI panel before either can mark it `Handled`.
- The docs say ImGui handles the press "before it reaches your layers" and that DingoUI "sits under ImGui and over your overlays"; the reference file says the same.
- Polled input is still protected, because the consume mask is applied before `OnUpdate`.

**Fix:** have `LayerStack` keep the engine overlays on top: count them, and insert a later `PushOverlay` below them. Alternatively, correct both docs.

### A1 — `DingoUIParams::DefaultFont` can't be used: no `Font` can exist before the `Application` {#a1}

`DingoUI.h:234-239`, `docs/game-ui.md:41-42`, `Application.cpp:136`. CONFIRMED.

**What happened:**
- The context copies `m_Params.DingoUI` inside `Application`'s constructor (:136).
- A `Font` needs the graphics device: `Font::Initialize` creates its atlas through `Texture::Create` and `NvrhiTexture`. So a font can't exist before the `Application` does.
- Setting `params.DingoUI.DefaultFont` from the game's constructor body is too late. The field is always null in practice.
- At 1.0 the field becomes permanent API (removing it then takes a major release).

**Fix:** remove the field before 1.0 and keep `DingoUI::SetDefaultFont`, or document it as reserved. Update `docs/game-ui.md:41-42` to match.

### D1 — `docs/game-ui.md` names `Input::GetScroll` (doesn't exist) and the internal `UIInputState` {#d1}

`docs/game-ui.md:220, 283`. CONFIRMED.

**What happened:**
- :220 says consumed scroll makes "`Input::GetScroll`" read as nothing. The function is `Input::GetMouseScrollDelta`.
- :283 points readers at `UIInputState::DeltaTime`, which is an `Internal` type.

**Fix:**
- :220: write `Input::GetMouseScrollDelta`.
- :283: say "the real frame time (the `OnUpdate` delta)".

## Final review of the fixes and U10 (2026-10-14, Opus high, `f6c3a2a..d1c4bfb`)

No High findings. Fix status:

| # | Sev | Finding | Status |
|---|---|---|---|
| R1 | Med | Marionette declared the HUD once per scene step (8 per frame in tournament runs) | fixed `fbd94a4`: `Hud::Update` simulates per step, `Hud::Declare` runs once per frame from `MarionetteLayer` |
| R2 | Med | An arena launch declared UI from `OnStart`, spending the "declare from OnUpdate" warning | fixed `fbd94a4`: nothing declares outside `OnUpdate` |
| R3 | Low | Dead `Overlay::AnyInputPressed`/`IsMenuKey` | fixed `fbd94a4` |
| R4 | Low | Title/end screens: a refocus click could press Play; pad/keyboard needs one press to reveal the ring | refocus guard fixed `fbd94a4`; the reveal press is the engine's design (U9 N1), kept |
| R5 | Low | HUD roots scaled with match 0.5 while the layout assumes height | fixed `fbd94a4`: `Overlay::Scale()` matches height |
| R6 | Low | `IsBackPressed` false on a navigable root with no focusable widget | fixed (engine commit after `fbd94a4`) |
| R7 | Low | A modal root that does not block input got no navigation but still consumed Back | fixed: modal implies `BlocksInput` |
| R8 | Low | Text-metrics key had one hash | fixed: key adds the length and an FNV-1a hash |
| R9 | Low | `NvrhiTexture.cpp` includes a DingoUI header (layering) | kept: one internal hook, no public surface; a generic texture-destroyed callback list is worth it only when a second listener appears |
| R10 | Low | A held direction reaches the game while the ring is hidden | intended and documented in `docs/game-ui.md` (games gate on `IsCapturingInput`) |
