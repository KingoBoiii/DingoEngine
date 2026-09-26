# Linux Support

What it takes to move DingoEngine from **Windows-only** to **supported on Linux**: what breaks, where,
how to fix it, and how each fix was verified. Companion to [ROADMAP.md](ROADMAP.md), whose v1.0
milestone lists *"cross-platform validation (Linux + Vulkan)"*, and to [KNOWN-BUGS.md](KNOWN-BUGS.md).

- **Verified on 2026-09-26 against `master` @ `f8e3486` (v0.6.1)** on Ubuntu 24.04 with GCC 13.3, premake
  5.0.0-beta8 (`gmake`), Vulkan SDK components 1.4.357 (built from source, [Appendix A](#appendix-a)) and
  Mesa's llvmpipe (Vulkan 1.4) under Xvfb. Every claim carries a `file:line` anchor from that pass. Code
  drifts, so re-confirm before fixing.
- **It is close.** With Phase 1 (L1–L11) plus L12, L15 and F1 applied (the exact, tested diff is in
  [Appendix B](#appendix-b)), `premake5 gmake && make` builds **all 20 projects in all four
  configurations** (Debug, Debug-ASan, Release, Distribution). The test framework and **all eight
  examples** run, render and shut down cleanly, including under AddressSanitizer. None of it is on
  `master` yet.
- **A first attempt stalled.** On 2026-05-14, `4378ccd`, `2f6a991`, `5e6d382` and `f42842c` guarded the
  D3D and MSVC-only bits in the NVRHI and spdlog premake scripts, and `c81a9bc` added
  `-Wno-changes-meaning`. Then `4d73d8b` switched the CI job off (`.github/workflows/build-master.yml:122`,
  `if: 1 == 0`). Everything below is what that attempt didn't reach.

**Why Windows never saw any of this**

| On Windows… | …which hides |
|---|---|
| MSVC accepts an extra member qualifier, default member initialisers used by a nested default argument, and members named after their own type | [L5](#l5), [L9](#l9), [L10](#l10) |
| NTFS is case-insensitive | [L2](#l2), [L11](#l11), [L15](#l15) |
| MSBuild links a referenced static lib's own dependencies, and `lib.exe` merges them into `DingoEngine.lib` | [L4](#l4), [L20](#l20) |
| Windows drivers offer an RGBA8 surface format, so the swap chain and RGBA8 render targets agree | [L14](#l14) |
| The Vulkan SDK installer always includes the validation layer | [L13](#l13) |
| The MSVC linker presumably never pulls the conflicting spdlog objects in | [L12](#l12) |
| `Debug-ASan` doesn't link on Windows ([KNOWN-BUGS K5](KNOWN-BUGS.md)) | [F1](#f1) |

**Kind**: **Build** stops compile or link. **Run** builds, then breaks or misbehaves. **Hygiene** works,
but leaves something wrong behind. **Ship** isn't needed to build, but is needed to call Linux
*supported*.
**Effort** (the [ROADMAP-BACKLOG.md](ROADMAP-BACKLOG.md) scale): **S** ≤ 1 day · **M** 2–4 days ·
**L** 1–2 weeks.

---

## Checklist

### Phase 1 — compile and link

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L1](#l1) | Windows-only library names | Build | `premake5.lua:110-124,193` | S | Appendix B |
| [L2](#l2) | `%{VULKAN_SDK}/Include` casing | Build | `premake5.lua:97`, NVRHI fork | S | Appendix B |
| [L3](#l3) | D3D11/D3D12 sources compiled on Linux | Build | `premake5.lua:168,237` | S | Appendix B |
| [L4](#l4) | Executables don't link the engine's dependencies | Build | `premake5.lua:72-83` | S | Appendix B |
| [L5](#l5) | Members named after their type in public headers | Build | 6 public headers, 20 sites | S | Stopgap in Appendix B; proper fix tested on a reduced case |
| [L6](#l6) | assimp is vendored for Windows only | Build | `premake5.lua:99,112-113` | S | Appendix B (development setup) |
| [L7](#l7) | GLFW's Linux file list is incomplete | Build | GLFW fork | S | Appendix B |
| [L8](#l8) | `DE_DEBUG_BREAK` undefined off Windows | Build | `Assertion.h:8-10` | S | Appendix B |
| [L9](#l9) | Nested `TextParameters` used as a default argument | Build | `Renderer2D.h:72-87` | S | Appendix B |
| [L10](#l10) | Extra qualifier makes `VulkanSwapChain` abstract | Build | `VulkanSwapChain.h:25` | S | Appendix B |
| [L11](#l11) | `<glfw/glfw3.h>` include casing | Build | 5 files | S | Appendix B |

### Phase 2 — run correctly

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L12](#l12) | spdlog built with defines the engine doesn't use | Run | `premake5.lua:201-203` | S | Appendix B |
| [L13](#l13) | Missing validation layer segfaults at startup | Run | `VulkanGraphicsContext.cpp:154-157,209-220` | S | — |
| [L14](#l14) | Renderer2D pipelines assume the swap-chain format | Run | `Renderer2D.h:185` | M | Cause verified |
| [L15](#l15) | Test font path differs in case | Run | `TextTest.cpp:8` | S | Appendix B |
| [L16](#l16) | Backslash in a cache sub-path | Hygiene | `Font.cpp:69` | S | — |

### Phase 3 — keep the tree clean

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L17](#l17) | `premake5 gmake` overwrites FreeType's tracked `Makefile` | Hygiene | FreeType fork | S | Appendix B |
| [L18](#l18) | Generated makefiles aren't git-ignored | Hygiene | `.gitignore` | S | — |

### Phase 4 — supported

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L19](#l19) | Re-enable Linux CI, with a headless smoke test | Ship | `build-master.yml:121-180` | M | Smoke test run locally |
| [L20](#l20) | Linux release packaging | Ship | `build-release.yml` | M | Archive merge verified |
| [L21](#l21) | The docs say Windows-only | Ship | `getting-started.md`, `README.md`, `CLAUDE.md` | S | — |
| [L22](#l22) | Developer tooling is Windows-only | Ship | `.vscode/`, `vendor/premake/bin/` | S | — |
| [L23](#l23) | Validate on real hardware and desktops | Ship | — | M | — |
| [L24](#l24) | Native Wayland *(optional)* | Ship | GLFW fork | M | — |
| [L25](#l25) | Clang *(optional)* | Ship | — | S | — |

### Found along the way

Neither is Linux-specific. Linux's working sanitizers found them.

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [F1](#f1) | ImGui index upload reads 2 bytes past its buffer | Run | `ImGuiRenderer.cpp:409,430` | S | Appendix B |
| [F2](#f2) | FlappyBird leaks its textures and font | Hygiene | `GameLayer.cpp:77-110` | S | — |

---

<a id="prerequisites"></a>
## 1. Prerequisites