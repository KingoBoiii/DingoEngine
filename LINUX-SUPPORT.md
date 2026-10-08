# Linux Support

What it takes to move DingoEngine from **Windows-only** to **supported on Linux**: what breaks, where,
how to fix it, and how each fix was verified. Companion to [ROADMAP.md](ROADMAP.md), whose v1.0 milestone
lists *"cross-platform validation (Linux + Vulkan)"*, and to the
[known-bug issues](https://github.com/KingoBoiii/DingoEngine/issues?q=is%3Aissue+is%3Aopen+label%3Abug).

- **Verified on 2026-10-06 against `master` @ `52acd47` (v0.8.2)** on Ubuntu 24.04 with GCC 13.3, premake
  5.0.0-beta8 (`gmake`), Vulkan SDK components 1.4.357 and assimp 6.0.4 (both built from source,
  [Appendix A](#appendix-a)), and Mesa's llvmpipe (Vulkan 1.4) under Xvfb. Every claim carries a
  `file:line` anchor from that pass. Code drifts, so re-confirm before fixing.
- **It is close.** With Phase 1 (L1–L13) plus L14, L17, L18 and F1 applied (the exact, tested diff is in
  [Appendix B](#appendix-b)), `premake5 gmake && make` builds **all 22 projects in all four
  configurations** (Debug, Debug-ASan, Release, Distribution). The test framework's 14 cases, **all ten
  examples** and their built-in self-checks (188 in the test framework, 107 in Marionette's `--check`)
  run, pass and shut down cleanly, including under AddressSanitizer. None of it is on `master` yet.
- **History.** On 2026-05-14, `4378ccd`, `2f6a991`, `5e6d382` and `f42842c` guarded the D3D and MSVC-only
  bits in the NVRHI and spdlog premake scripts, and `c81a9bc` added `-Wno-changes-meaning`. Then
  `4d73d8b` switched the CI job off (`.github/workflows/build-master.yml:122`, `if: 1 == 0`). This plan
  was first written against v0.6.1 (`f8e3486`) and re-verified against v0.8.2; [§4](#since-v061) lists
  what changed in between.

**Why Windows never saw any of this**

| On Windows… | …which hides |
|---|---|
| MSVC accepts an extra member qualifier, default member initialisers used by a nested default argument, and members named after their own type | [L5](#l5), [L9](#l9), [L10](#l10) |
| MSVC's `std::filesystem` has a `file_type::junction` extension | [L13](#l13) |
| `cl` embeds the ASan runtime itself; GCC needs `-fsanitize=address` at link time | [L12](#l12) |
| NTFS is case-insensitive | [L2](#l2), [L11](#l11), [L17](#l17), [L18](#l18) |
| The Windows build uses the vendored assimp 6.0.4 binaries; Linux distros ship older versions | [L6](#l6) |
| MSBuild links a referenced static lib's own dependencies, and `lib.exe` merges them into `DingoEngine.lib` | [L4](#l4), [L23](#l23) |
| Windows drivers offer an RGBA8 surface format, so the swap chain and RGBA8 render targets agree | [L16](#l16) |
| The Vulkan SDK installer always includes the validation layer | [L15](#l15) |
| The MSVC linker presumably never pulls the conflicting spdlog objects in | [L14](#l14) |
| MSVC's ASan most likely doesn't intercept `memcpy` inside a driver DLL | [F1](#f1) |

**Kind**: **Build** stops compile or link. **Run** builds, then breaks or misbehaves. **Hygiene** works,
but leaves something wrong behind. **Ship** isn't needed to build, but is needed to call Linux
*supported*.
**Effort**: **S** ≤ 1 day · **M** 2–4 days · **L** 1–2 weeks.

---

## Checklist

### Phase 1 — compile and link

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L1](#l1) | Windows-only library names | Build | `premake5.lua:117-131,202` | S | Appendix B |
| [L2](#l2) | `%{VULKAN_SDK}/Include` casing | Build | `premake5.lua:104`, NVRHI fork | S | Appendix B |
| [L3](#l3) | D3D11/D3D12 sources compiled on Linux | Build | `premake5.lua:175,262` | S | Appendix B |
| [L4](#l4) | Executables don't link the engine's dependencies; Windows-only copy steps | Build | `premake5.lua:76-90` | S | Appendix B |
| [L5](#l5) | Members named after their type in public headers | Build | 6 public headers, 20 sites | S | Stopgap in Appendix B; proper fix tested on a reduced case |
| [L6](#l6) | assimp: Linux needs 6.0.x; only Windows binaries are vendored | Build | `Model.cpp:372`, `premake5.lua:106,114,119-120` | S | Appendix A + B |
| [L7](#l7) | GLFW's Linux file list is incomplete | Build | GLFW fork | S | Appendix B |
| [L8](#l8) | `DE_DEBUG_BREAK` undefined off Windows | Build | `Assertion.h:8-10` | S | Appendix B |
| [L9](#l9) | Nested `TextParameters` used as a default argument | Build | `Renderer2D.h:72-89` | S | Appendix B |
| [L10](#l10) | Extra qualifier makes `VulkanSwapChain` abstract | Build | `VulkanSwapChain.h:25` | S | Appendix B |
| [L11](#l11) | `<glfw/glfw3.h>` include casing | Build | 5 files | S | Appendix B |
| [L12](#l12) | Debug-ASan executables don't link | Build | `premake5.lua:32-39` | S | Appendix B |
| [L13](#l13) | Marionette uses MSVC's `file_type::junction` | Build | `LiveEdit.cpp:58` | S | Appendix B |

**Phase 1 is done** (2026-10-06). It differs from [Appendix B](#appendix-b) in four places:

- [L5](#l5) uses the preferred fix. All 20 members spell their type `Dingo::`-qualified, so consumers build
  without `-Wno-changes-meaning`. The engine keeps the flag for four engine-internal members
  (`NvrhiRenderPass.h:56`, `Renderer.cpp:31-32` and `ImGuiLayer.cpp:147`).
- The NVRHI half of [L2](#l2), and [L7](#l7), re-open `NVRHI-Vulkan` and `GLFW` from the root
  `premake5.lua`, so the forks are untouched.
- [L4](#l4)'s link list applies to executables only (`kind:ConsoleApp or WindowedApp`), because the
  engine's own project calls the same helper.
- [L8](#l8)'s fallback on any other platform is `std::abort()` rather than an empty macro, so a failed
  `DE_*_VERIFY` still stops.

It was verified in WSL2: Ubuntu 24.04, GCC 13.3, premake 5.0.0-beta8, LunarG's 1.4.350.0 Linux SDK and
assimp 6.0.4.

- **Build**: all 22 projects build in all four configurations.
- **Include casing**: every path in the builds' `.d` files matches its directory entries exactly. The
  worktree sat on case-insensitive NTFS, so this audit stood in for a case-sensitive filesystem.
- **Windows**: regenerating the vs2026 projects gives identical files apart from the `include` casing, and
  the whole solution builds in Debug.
- **Headless runs**, under Xvfb and llvmpipe with the validation layer:
  - Debug and Release: all ten examples and all 14 test cases run and close with exit code 0. 188 of 189
    self-checks pass, and Marionette's `--check` passes 107/107.
  - The one failing check is [L18](#l18). The examples report no validation errors. The 7 test cases that
    draw 2D into the test's render target report [L16](#l16)'s `02684`, and the `LOG ERROR` lines are
    [L14](#l14).
  - Debug-ASan: executables link and run instrumented. Each one stops at [L14](#l14), where the
    garbage-sized allocation that throws `bad_alloc` elsewhere aborts under ASan. With
    `allocator_may_return_null=1`, FlappyBird runs clean. The test framework passes its 78 animation checks
    under ASan, then stops at [F1](#f1).

### Phase 2 — run correctly

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L14](#l14) | spdlog built with defines the engine doesn't use | Run | `premake5.lua:210-212` | S | Appendix B |
| [L15](#l15) | The validation layer is required outside Distribution | Run | `VulkanGraphicsContext.cpp:160-163,215-226` | S | — |
| [L16](#l16) | Renderer2D pipelines assume the swap-chain format | Run | `Renderer2D.h:189` | M | Cause verified |
| [L17](#l17) | Test font paths differ in case | Run | 4 test files | S | Appendix B |
| [L18](#l18) | The Asset Manager test expects Windows path-case folding | Run | `AssetManagerTest.cpp:32` | S | Appendix B |
| [L19](#l19) | Backslash in a cache sub-path | Hygiene | `Font.cpp:71` | S | — |

**Phase 2 is done** (2026-10-08), one commit per item, on top of `master` @ `c14fe7d` (v0.8.3, which also
carries [F1](#f1)'s fix) merged into the branch. It differs from the plan in four places:

- [L15](#l15) also makes `VK_EXT_debug_report` optional, since it came with the layer, and creates the
  debug-report callback only when the extension is enabled. A missing layer logs a warning that says where
  to get it.
- [L16](#l16) keys the pipelines on the target's *format* rather than on the framebuffer, as `Material`
  does. `Internal::GetFramebufferFormatKey` (`Graphics/FramebufferFormat.h`) hashes NVRHI's
  `FramebufferInfo` (colour formats, depth format, sample count). Each batch pass keeps a pipeline and a
  render-pass pool per key and picks one at flush time from the current target, so render targets of one
  format share a pipeline and nothing dangles when a target is destroyed. No BGRA8 `TextureFormat` was
  added.
- [L17](#l17) covers five tests: v0.8.3's Render Target Test loads the same `ArialBD.ttf`.
- [L18](#l18) and [L19](#l19) are as planned. [L14](#l14) is as planned and still needs a run on Windows.

It was verified on Ubuntu 24.04 (GCC 13.3, premake 5.0.0-beta8, [Appendix A](#appendix-a)'s Vulkan SDK
1.4.357 components and assimp 6.0.4, Ubuntu's validation layer) under Xvfb and llvmpipe:

- **Build**: all 22 projects in Debug, Debug-ASan, Release and Distribution.
- **Debug, Release and Debug-ASan** (leak detection off): the test framework's 15 cases, all ten examples
  and Marionette's `--check` run and close through `WM_DELETE_WINDOW` with exit code 0. The self-checks
  pass 204/204 (L18's included) and 107/107, with no validation errors, no `LOG ERROR` lines and no
  AddressSanitizer reports.
- **L16 A/B**: with the pre-L16 `Renderer2D`, the Color Quad Test logs 1,048
  `VUID-vkCmdDrawIndexed-renderPass-02684` errors and the Render Target Test 3,882 validation lines.
  With the fix, both log none.
- **L15**: with `VK_LOADER_LAYERS_DISABLE=VK_LAYER_KHRONOS_validation`, FlappyBird logs the warning, runs
  and exits 0.

### Phase 3 — keep the tree clean

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L20](#l20) | `premake5 gmake` overwrites FreeType's tracked `Makefile` | Hygiene | FreeType fork | S | Appendix B |
| [L21](#l21) | Generated makefiles aren't git-ignored | Hygiene | `.gitignore` | S | Appendix B |

**Phase 3 is done** (2026-10-08), one commit per item. Neither touches a fork:

- [L20](#l20) moves the makefiles themselves out of the submodules rather than adding `location` to the FreeType
  fork. Under `gmake` the root `premake5.lua` re-opens every vendor project and sets its `location` to
  `build/make/<project>`, which is already git-ignored. That keeps FreeType's tracked `Makefile` intact and the
  other six submodules clean too. The vendor projects' paths resolve against their own scripts, so the
  libraries still build into `vendor/*/bin/`. The Visual Studio projects don't move.
- [L21](#l21) ignores the root `Makefile` and `*.make`, `test/Makefile`, `examples/*/Makefile` and
  `vendor/assimp/lib/linux-x86_64/`. The forks' `.gitignore` files need nothing, since L20 keeps their
  makefiles out of their trees.

Verified on the same setup as Phase 2. After `premake5 gmake` and `make` in all four configurations,
`git status` is empty in the superproject and in every submodule, recursively. FreeType's `libfreetype.a`
and spdlog's `libspdlog.a` rebuilt from scratch into their usual `bin/` folders, and FlappyBird relinked and
ran.

### Phase 4 — supported

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [L22](#l22) | Re-enable Linux CI, with a headless smoke test | Ship | `build-master.yml:121-180` | M | Smoke test run locally |
| [L23](#l23) | Linux release packaging | Ship | `build-release.yml:697-772` | M | Archive merge verified |
| [L24](#l24) | The docs say Windows-only | Ship | `getting-started.md`, `README.md`, `CLAUDE.md` | S | — |
| [L25](#l25) | Developer tooling is Windows-only | Ship | `.vscode/`, `vendor/premake/bin/` | S | — |
| [L26](#l26) | Validate on real hardware and desktops | Ship | — | M | — |
| [L27](#l27) | Native Wayland *(optional)* | Ship | GLFW fork | M | — |
| [L28](#l28) | Clang *(optional)* | Ship | — | S | — |

**Phase 4 is done except [L26](#l26) and [L27](#l27)** (2026-10-08), one commit per item:

- [L22](#l22) is its own workflow, `build-linux.yml`, on pushes **and pull requests** to master, so a GCC-only
  break shows up before it lands. Its setup (build packages, LunarG's latest Linux SDK tarball, a static assimp
  6.0.4 and premake, the last three cached) is the composite action `.github/actions/setup-linux`, which the
  release job shares. Each of Debug, Release and Distribution (the Windows job's configurations; GCC only) builds every project and runs
  `scripts/ci/linux-smoke-test.sh` under Xvfb and llvmpipe with the SDK's validation layer. The script reads
  the test list from `TestLayer.cpp`, finds the examples itself, closes each app through `WM_DELETE_WINDOW`
  (`scripts/ci/close-windows.py`, since Ubuntu's xdotool predates `windowquit`), and fails on a non-zero exit,
  a hang, a `[FAIL]` line, a validation error, an ASan report or an spdlog `LOG ERROR`. A concurrency group
  cancels a pull request's superseded runs. The old disabled job in `build-master.yml` is gone.
- [L23](#l23): the engine's Linux post-build step builds `build/dist/<cfg>/libDingoEngine.a`
  (`scripts/merge-static-libs.sh`, `ar -M` through numbered links, since MRI scripts can't quote paths with
  spaces). The release job ships it with `include/` and `glm/` for Debug, Release and Distribution, plus
  every example as its executable and `assets/`. It runs only on a `v*` tag, so its first real run is the next
  release; its steps were dry-run locally for `v0.9.0`.
- [L24](#l24) also fixes Getting Started's sample app, which never set `Graphics.GraphicsAPI` and so crashed in
  `Application::Initialize` on every platform. The engine-side bug (no default) is
  [#109](https://github.com/KingoBoiii/DingoEngine/issues/109).
- [L25](#l25) gives every existing VS Code task a `"linux"` override, so the task names, and the launch
  entries' `preLaunchTask`, are shared. It vendors premake beta8's Linux binary (force-added: `**/bin/` ignores
  it, and `!Vendor/**` only matches case-insensitively) and adds `Generate-Linux.sh`.
- [L28](#l28) moves `-Wno-changes-meaning` under `toolset:gcc` (clang warned about it once per engine file).
  Clang 18 builds everything and passes the smoke test locally, but CI builds with GCC only: one compiler, as
  on Windows.

Verified:

- **CI**, on GitHub's Ubuntu 24.04 runners with Vulkan SDK 1.4.363.0: Debug, Release and Distribution build
  with GCC and pass the smoke test (15 test cases, 10 examples and Marionette's `--check`). Before the matrix was
  trimmed to the Windows job's configurations, Debug-ASan with GCC and Debug with clang 18 passed too.
- **Merged archive**: 650 members from 12 inputs (64 MB). FlappyBird's objects and Getting Started's
  sample link against it with only the SDK's ShaderC and SPIRV-Cross, `z`, `dl` and `pthread`, with no link
  group, and run. Marionette runs from its extracted release tarball.
- **Tooling**: `Generate-Linux.sh` works from any directory; every task's `make` command dry-runs; every
  Linux launch entry's program and `cwd` exist.

Still open: [L26](#l26) needs real hardware (a USB-booted Ubuntu on a real GPU is the cheapest start), and
[L27](#l27) is a decision before it is work.

### Found along the way

Not Linux-specific. Linux's working sanitizers found it.

| # | Item | Kind | Where | Effort | Tested fix |
|---|---|---|---|---|---|
| [F1](#f1) | ImGui index upload reads 2 bytes past its buffer ([#86](https://github.com/KingoBoiii/DingoEngine/issues/86)) | Run | `ImGuiRenderer.cpp:409,430` | S | Appendix B |

---

<a id="prerequisites"></a>
## 1. Prerequisites

| Requirement | Notes |
|---|---|
| x86-64 Linux | Verified on Ubuntu 24.04. |
| GCC ≥ 13 | The engine's C++20 `<format>` use needs libstdc++ 13, so Ubuntu 22.04's GCC 11 and Debian 12's GCC 12 are too old. Verified with 13.3. For clang, see [L28](#l28). |
| premake5 5.0.0-beta8, Linux build | The repo only ships `vendor/premake/bin/premake5.exe`; CI already pins beta8. Use the **`gmake`** action. |
| Vulkan SDK ≥ 1.4 | LunarG's Linux tarball, **not** the distro packages (see below). |
| assimp 6.0.x, static | Built from source into `vendor/assimp/lib/linux-x86_64/` ([L6](#l6), [Appendix A](#appendix-a)). Distro packages are too old. |
| Build tools and headers | `cmake ninja-build python3 zlib1g-dev` (for Appendix A), plus `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` for GLFW's X11 back-end, which loads the libraries themselves at runtime. |
| Runtime | `libvulkan1` and a Vulkan **1.3** driver (`VulkanGraphicsContext.cpp:246`) offering `VK_KHR_maintenance1` and `VK_KHR_swapchain` (`VulkanGraphicsContext.h:79-80`). Headless: `xvfb` plus `mesa-vulkan-drivers` (llvmpipe). |

**Why the distro packages don't work** (Ubuntu 24.04):

- The Vulkan headers are 1.3.275. NVRHI's Vulkan back-end uses `VK_NV_cluster_acceleration_structure` and
  doesn't compile against them. Its own bundled `vendor/nvrhi/thirdparty/Vulkan-Headers` is at
  `VK_HEADER_VERSION` 313.
- shaderc 2023.8 has no `shaderc_env_version_vulkan_1_4`, which
  `src/DingoEngine/Graphics/ShaderCompiler.cpp:76` uses.
- assimp 5.3.1 predates `aiVectorKey::mInterpolation` and `aiAnimInterpolation_Step`, which v0.8's
  animation import uses (`Model.cpp:372`).

The build needs these from `$VULKAN_SDK`:

- `include/{vulkan,shaderc,spirv_cross}`
- `lib/libshaderc_combined.a`
- `lib/libspirv-cross-{core,glsl,hlsl}.a`
- for Debug and Release, the `VK_LAYER_KHRONOS_validation` layer ([L15](#l15)). Ubuntu's own
  `vulkan-validationlayers` package works; the engine produced no validation errors with it beyond
  [L16](#l16).

LunarG's download host wasn't reachable from the sandbox this was verified in, so those pieces were built
from source at the `vulkan-sdk-1.4.357.0` tags ([Appendix A](#appendix-a)). A real tarball has the same
file names: Phase 1 was built and run against LunarG's `vulkansdk-linux-x86_64-1.4.350.0.tar.xz` (the
Windows SDK's version), whose `setup-env.sh` also puts its validation layer on the loader's path.

<a id="building"></a>
## 2. Building and running (once Phase 1 lands)

```bash
# System packages (Ubuntu/Debian names)
sudo apt install build-essential git cmake ninja-build python3 zlib1g-dev \
                 libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Vulkan SDK: extract LunarG's Linux tarball, then source its environment script,
# which points VULKAN_SDK at the SDK's x86_64 directory
source <sdk-dir>/setup-env.sh

# assimp 6.0.4 as a static library into vendor/assimp/lib/linux-x86_64/ (the last part of Appendix A)

# premake5, Linux build
curl -LO https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-linux.tar.gz
tar -xzf premake-5.0.0-beta8-linux.tar.gz && chmod +x premake5

# Generate and build
git submodule update --init --recursive
./premake5 gmake
make -j"$(nproc)" config=debug                # debug | debug-asan | release | distribution
make -j"$(nproc)" config=release FlappyBird   # or a single project

# Run from the project's own directory; asset paths are relative to it
cd examples/FlappyBird && ../../build/bin/Debug-linux-x86_64/FlappyBird/FlappyBird
```

Outputs follow the Windows layout: `build/bin/<Config>-linux-x86_64/<Project>/`, with the vendor libraries in
`vendor/*/bin/<Config>-linux-x86_64/`.

<a id="verified"></a>
## 3. What was verified

All against `52acd47` with Appendix B applied.

| Check | Result |
|---|---|
| `premake5 gmake` | Generates all 22 projects. The only warning is a deprecation for `flags { "multiprocessorcompile" }` (`premake5.lua:11`), on every platform. The new shader-embedding build step (`scripts/embed.lua`) works under `gmake`. |
| `make`: Debug, Debug-ASan, Release, Distribution | All 22 projects build in each: 10 vendor libraries, the engine, the test framework and the 10 examples. |
| Test framework, all 14 cases (Debug, validation on) | All run and close with exit code 0. Their self-checks: 188 passed, 0 failed (Animation 78, Hierarchy 47, Lighting 25, Asset Manager 22, Mesh Collider 5, Mesh 3D 4, Background 4, Renderer3D Batch 3), with [L18](#l18) applied. Every 2D case that renders into the test's offscreen target trips [L16](#l16). |
| All 10 examples (Debug, validation on) | All run and close with exit code 0, with no validation errors. |
| Marionette `--check` | Asset 17/17, movement 27/27, combat 51/51, AI 12/12. |
| AddressSanitizer (`debug-asan`) | All 14 test cases and all 10 examples were closed through `WM_DELETE_WINDOW`: exit code 0, no reports, and the 188 self-checks pass under ASan too. Needs [L12](#l12), and holds only after [F1](#f1), which ASan found on v0.6.1. |
| LeakSanitizer (FlappyBird) | 260 bytes in 5 allocations, all inside `libxcb`, reached from Mesa's swap-chain creation and GLFW's X11 teardown. None belong to the engine. (On v0.6.1 it was 917 bytes, mostly FlappyBird's own textures and font, since fixed.) |
| One merged archive ([L23](#l23)) | A single 64 MB `libDingoEngine.a` (engine, vendor libraries and the static assimp) links FlappyBird and Marionette with only the SDK libraries, zlib and libc. Both run without asset-load errors, and need only `libz`, `libstdc++`, `libm`, `libgcc_s` and `libc` at load time. |
| Not covered | Real GPUs, Wayland, audio output (there was no sound device, so miniaudio only logged ALSA errors), gamepads, any input beyond the built-in self-checks, and clang. See [L26](#l26). |

<a id="since-v061"></a>
## 4. Changes since the first pass (v0.6.1 → v0.8.2)

- **New blockers**: [L6](#l6) now needs assimp ≥ 5.4 (v0.8 animation import), [L12](#l12) (the v0.6.3
  K5 fix removed the ASan link flag GCC needs), [L13](#l13) (Marionette), [L17](#l17) grew from one test to
  four, [L18](#l18) (the Asset Manager test), and [L4](#l4) gained a Windows-only copy step for the ASan
  runtime DLL.
- **Fixed on `master`**: FlappyBird's texture and font leak (now `DestroyAndDelete`), and the validation
  false positive the first pass reported: the engine no longer uses `AttachmentLoadOp::eNone` or requires
  `VK_KHR_load_store_op_none`. [L15](#l15) is half fixed: a missing layer now fails loudly instead of
  segfaulting.
- **Unchanged**: everything else, at new line numbers. The vendor submodules haven't moved, so the three
  fork patches are the same.

---

## 5. The items

### Phase 1 — compile and link

<a id="l1"></a>
### L1 — Windows-only library names

**Build** — `premake5.lua:117-131`, `premake5.lua:202`

The `Library` table names Windows artifacts:

- `vulkan-1.lib`
- the debug-suffixed `shaderc_combinedd.lib` and `spirv-cross-*d.lib`
- `assimp-vc145-mt(d).lib`

premake passes them through verbatim (`-lvulkan-1.lib` and so on), and they can't resolve on Linux.

**Fix**: use plain Linux names without the debug suffix. The Linux SDK ships none, and there's no
debug/release CRT split to match. Also don't link the Vulkan loader on Linux at all: `VulkanGraphicsContext`
resolves it through `vk::DynamicLoader`, and GLFW `dlopen`s it. `ldd` confirmed the verified binaries don't
depend on `libvulkan.so`. Move `%{Library.vulkan}` into the engine's `system:windows` filter. The
`LibraryDir['assimp']` line belongs to [L6](#l6).

```lua
if os.istarget("linux") then
    LibraryDir['assimp'] = "%{wks.location}/vendor/assimp/lib/linux-x86_64"
    Library['assimp_Debug']   = "assimp"
    Library['assimp_Release'] = "assimp"
    for _, cfg in ipairs({ "Debug", "Release" }) do
        Library["ShaderC_" .. cfg]          = "shaderc_combined"
        Library["SPIRV_Cross_" .. cfg]      = "spirv-cross-core"
        Library["SPIRV_Cross_GLSL_" .. cfg] = "spirv-cross-glsl"
        Library["SPIRV_Cross_HLSL_" .. cfg] = "spirv-cross-hlsl"
    end
end
```

<a id="l2"></a>
### L2 — `%{VULKAN_SDK}/Include` casing

**Build** — `premake5.lua:104` and `vendor/nvrhi/premake5.lua:35` (fork: KingoBoiii/NVRHI)

The Windows SDK installs `Include`, and NTFS doesn't care about case. The Linux SDK has `include`, and the
path is case-sensitive. With the wrong case the SDK headers are never seen and the build falls back to
whatever `/usr/include` holds. The first error is then NVRHI failing on
`VK_NV_cluster_acceleration_structure`, which looks like a version problem rather than a path problem.

**Fix**: `%{VULKAN_SDK}/include` in both scripts. That works on Windows too.

<a id="l3"></a>
### L3 — D3D11/D3D12 sources compiled on Linux

**Build** — `premake5.lua:175` (the `src/**.cpp` glob) and `premake5.lua:262`

The factories already fence off the D3D back-ends (`GraphicsContext.cpp:6,19` and `SwapChain.cpp:7,20`).
The glob still compiles `Graphics/NVRHI/DirectX11/` and `DirectX12/`, though, and they stop at
`dxgi1_6.h`.

**Fix**, in the engine's `system:linux` filter:

```lua
removefiles { "src/DingoEngine/Graphics/NVRHI/DirectX11/**", "src/DingoEngine/Graphics/NVRHI/DirectX12/**" }
```

<a id="l4"></a>
### L4 — Executables don't link the engine's dependencies; Windows-only copy steps

**Build** — `premake5.lua:76-90` (`copyAssimpRuntime`) and `docs/getting-started.md:55`

Getting Started promises that `links { "DingoEngine" }` "pulls in every transitive dependency". That's
MSBuild behaviour. With `gmake`, an executable's link line is just `libDingoEngine.a`, so every executable
fails with hundreds of undefined references into ImGui, Jolt, GLFW, Box2D, msdf-atlas-gen/msdfgen, spdlog,
SPIRV-Cross, shaderc, NVRHI and assimp (reproduced by linking FlappyBird that way). GNU `ld` is also
order-sensitive.

The same helper's copy steps are Windows-only too. The assimp DLL copies would put Windows DLLs next to
Linux binaries. The Debug-ASan copy of `clang_rt.asan_dynamic-x86_64.dll` (`:86-87`) uses the MSBuild
macro `$(VCToolsInstallDir)`, which is an empty variable under `make`, so the copy fails.

**Fix**: every executable already calls `copyAssimpRuntime()` (it's on the new-executable checklist in
`CLAUDE.md`), so the Linux link list goes there and no example script has to change. `linkgroups "On"`
wraps the list in `--start-group … --end-group`, so order no longer matters; `z` is there for the static
assimp ([L6](#l6)). Consider renaming the helper, since it no longer only copies assimp.

```lua
filter { "system:windows", "configurations:Debug or configurations:Debug-ASan" }
    postbuildcommands { '{COPY} "' .. debugBin .. '" "%{cfg.targetdir}"' }

filter { "system:windows", "configurations:Release or configurations:Distribution" }
    postbuildcommands { '{COPY} "' .. releaseBin .. '" "%{cfg.targetdir}"' }

filter { "system:windows", "configurations:Debug-ASan" }
    postbuildcommands { '{COPY} "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "%{cfg.targetdir}"' }

filter "system:linux"
    linkgroups "On"
    libdirs { "%{LibraryDir.vulkan}", "%{LibraryDir.assimp}" }
    links {
        "DingoEngine", "spdlog", "GLFW", "NVRHI", "NVRHI-Vulkan", "ImGui",
        "msdf-atlas-gen", "msdfgen", "freetype", "box2d", "Jolt",
        "shaderc_combined", "spirv-cross-hlsl", "spirv-cross-glsl", "spirv-cross-core",
        "assimp", "z", "dl", "pthread"
    }

filter {}
```

<a id="l5"></a>
### L5 — Members named after their type, in public headers

**Build** — 20 sites in 6 public headers:

- `Scene/Components.h:81,101,342,352`
- `Graphics/Pipeline.h:14,39-42,46,48`
- `Graphics/Material.h:18-20,163-164`
- `Graphics/RenderPass.h:12`
- `Graphics/Renderer2D.h:148-149`
- `Graphics/GraphicsParams.h:13`

`Mesh* Mesh = nullptr;` declares a member that changes what `Mesh` means inside its class. That's
ill-formed with no diagnostic required ([basic.scope.class]). MSVC is silent, but GCC 13+ reports it as an
**error** (`-Wchanges-meaning`). `c81a9bc` silenced it for the engine project only (`premake5.lua:264`), so
every consumer translation unit fails on all 20 sites: the test framework, each example, and any external
game built with GCC. The list comes from compiling `DingoEngine.h` with GCC and no suppression.

**Fix (preferred)**: qualify the type, as in `Dingo::Mesh* Mesh = nullptr;` and
`Dingo::CullMode CullMode = Dingo::CullMode::Back;`. There's no API change, and a reduced case compiles
cleanly with GCC 13 and clang 18.

**Stopgap** (what Appendix B uses): add `buildoptions { "-Wno-changes-meaning" }` to the [L4](#l4) helper.
External GCC projects would need that flag too.

<a id="l6"></a>
### L6 — assimp: Linux needs 6.0.x; only Windows binaries are vendored

**Build** — `src/DingoEngine/Graphics/Model.cpp:372`, `premake5.lua:106,114,119-120` and
`vendor/assimp/{include,lib,bin}`

`vendor/assimp` holds the Windows build of assimp **6.0.4** (its headers are byte-identical to that tag):
MSVC import libraries, DLLs and headers, none of it usable by a Linux linker. Linking a distro assimp
instead no longer works either. v0.8's animation import uses `aiVectorKey::mInterpolation` and
`aiAnimInterpolation_Step` (`Model.cpp:372`), which arrived in assimp 5.4, and Ubuntu 24.04 ships 5.3.1.

**Fix** (verified): build assimp 6.0.4 as a static library into `vendor/assimp/lib/linux-x86_64/`
([Appendix A](#appendix-a)'s last part, about 6 minutes on 4 cores).

- Keep `vendor/assimp/include`. 6.0.4's installed headers are byte-identical to it, `config.h` included.
- Point `LibraryDir['assimp']` at the new folder on Linux ([L1](#l1)'s snippet).
- Link `assimp` plus `z`, since the static assimp needs zlib ([L4](#l4)'s snippet).
- Git-ignore the folder ([L21](#l21)). The library is 31 MB and the repo has no Git LFS.

A static assimp also settles shipping ([L23](#l23)): nothing to install and no soname to match. The
alternatives are committing the static library like the Windows binaries (zero setup, but 31 MB per assimp
upgrade), or an assimp fork with its own `premake5.lua`, like Box2D and Jolt (the most consistent option,
and the most work).

<a id="l7"></a>
### L7 — GLFW's Linux file list is incomplete

**Build** — `vendor/glfw/premake5.lua:29-51` (fork: KingoBoiii/glfw)

GLFW 3.4 moved module loading and `poll()` handling into `posix_module.c` and `posix_poll.c`. The submodule
is 3.5-dev, but its premake's Linux file list predates that change. Every executable then fails to link on:

- `_glfwPlatformLoadModule`
- `_glfwPlatformGetModuleSymbol`
- `_glfwPlatformFreeModule`
- `_glfwPollPOSIX`

Linking with and without the two files confirmed this.

**Fix**: add `"src/posix_module.c"` and `"src/posix_poll.c"` to the Linux `files` list, then bump the
submodule. That's the route `4378ccd` took for the NVRHI and spdlog forks. To leave the fork alone, re-open
the project from the root script after `include "vendor/glfw"` (`project "GLFW"`, then
`filter "system:linux"`, then `files { … }`). That route is untested.

<a id="l8"></a>
### L8 — `DE_DEBUG_BREAK` undefined off Windows

**Build** — `include/DingoEngine/Assertion.h:8-10`

`DE_DEBUG_BREAK` is only defined under `DE_PLATFORM_WINDOWS`. Yet every `DE_CORE_ASSERT`/`DE_ASSERT` (in
Debug) and every `DE_CORE_VERIFY`/`DE_VERIFY` (in all configurations) expands to it. That's 78 errors in
the engine alone.

**Fix**: raise `SIGTRAP` on Linux. It stops in gdb, and without a debugger it ends the process with a core
dump, which is the same contract as `__debugbreak()`.

```cpp
#if defined(DE_PLATFORM_WINDOWS)
#define DE_DEBUG_BREAK __debugbreak()
#elif defined(DE_PLATFORM_LINUX)
#include <csignal>
#define DE_DEBUG_BREAK std::raise(SIGTRAP)
#else
#define DE_DEBUG_BREAK
#endif
```

<a id="l9"></a>
### L9 — Nested `TextParameters` used as a default argument

**Build** — `include/DingoEngine/Graphics/Renderer2D.h:72-89`

`const TextParameters& textParameters = {}` uses a nested struct with default member initialisers as a
default argument while `Renderer2D` is still incomplete. GCC and Clang reject that, because the initialisers
aren't usable until the enclosing class is complete; MSVC accepts it. It's in a public header, so every
translation unit that includes `DingoEngine.h` fails.

**Fix** (source-compatible): hoist the struct to namespace scope as `Renderer2DTextParameters`, and keep
`using TextParameters = Renderer2DTextParameters;` in the class so `Renderer2D::TextParameters{ … }` keeps
working. Confirm MSVC accepts it; that hasn't been tested.

<a id="l10"></a>
### L10 — Extra qualifier makes `VulkanSwapChain` abstract

**Build** — `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h:25`

`virtual Framebuffer* SwapChain::GetCurrentFramebuffer() const` carries a stray `SwapChain::` qualifier
inside the class. MSVC tolerates it. GCC rejects the declaration, so the pure virtual stays unimplemented and
`new VulkanSwapChain(params)` fails (`SwapChain.cpp:19`). The D3D swap chains spell it correctly.

**Fix**: `virtual Framebuffer* GetCurrentFramebuffer() const override`.

<a id="l11"></a>
### L11 — `<glfw/glfw3.h>` include casing

**Build** — in `src/DingoEngine/`:

- `Windowing/Window.cpp:11`
- `Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp:5`
- `Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp:4`
- `Graphics/NVRHI/DirectX11/DirectX11SwapChain.cpp:4` and `Graphics/NVRHI/DirectX12/DirectX12SwapChain.cpp:4`

The directory is `GLFW/`, and `Core/Input.cpp:4` already spells it correctly. A scan of every `#include` in
`src/`, `include/`, `test/` and `examples/` found no other include that only matches when case is ignored.

**Fix**: `#include <GLFW/glfw3.h>`.

<a id="l12"></a>
### L12 — Debug-ASan executables don't link

**Build** (Debug-ASan) — `premake5.lua:32-39`

v0.6.3's K5 fix removed `linkoptions { "-fsanitize=address" }`, which is right for MSVC, because `cl`
embeds `/INFERASANLIBS`. GCC has no such mechanism, so on Linux every Debug-ASan executable fails to link
with hundreds of thousands of undefined `__asan_report_*` references. The `_DISABLE_*_ANNOTATION` defines
are MSVC STL macros and harmless under libstdc++.

**Fix**, next to the existing Debug-ASan settings:

```lua
filter { "system:linux", "configurations:Debug-ASan" }
    linkoptions { "-fsanitize=address" }
```

<a id="l13"></a>
### L13 — Marionette uses MSVC's `file_type::junction`

**Build** (Marionette) — `examples/Marionette/src/LiveEdit.cpp:58`

`IsLink` treats NTFS junctions like symlinks through `std::filesystem::file_type::junction`, an MSVC STL
extension that libstdc++ doesn't have.

**Fix**: junctions only exist on Windows anyway.

```cpp
#ifdef DE_PLATFORM_WINDOWS
		return !error && (std::filesystem::is_symlink(status) || status.type() == std::filesystem::file_type::junction);
#else
		return !error && std::filesystem::is_symlink(status);
#endif
```

### Phase 2 — run correctly

<a id="l14"></a>
### L14 — spdlog built with defines the engine doesn't use

**Run** — `vendor/spdlog/premake5.lua:19-23` against the engine's `defines` (`premake5.lua:210-212`)

`libspdlog.a` is built with `SPDLOG_COMPILED_LIB` and `SPDLOG_USE_STD_FORMAT`. The engine includes spdlog's
headers without them, which means header-only mode with the bundled `fmt`. Two incompatible definitions of
the same classes end up in one binary, and on Linux the linker takes some of each. A trace-level log in
`VulkanGraphicsContext::CreateInstance` throws `std::bad_alloc` inside spdlog's file sink and prints
`[*** LOG ERROR #0001 ***] std::bad_alloc`; gdb traced it (on v0.6.1). Windows presumably survives because
its linker never needs the library's copies once the engine's inline ones exist. That's luck, not
correctness.

**Fix**: add both defines to the engine project. With them the error is gone.

**This changes the Windows build too**, because the engine starts using the compiled library. Build and run
it on Windows before merging.

<a id="l15"></a>
### L15 — The validation layer is required outside Distribution

**Run** — `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp:160-163,215-226` and `:80-84`

Outside Distribution, `VK_LAYER_KHRONOS_validation` is a *required* layer. On v0.6.1 a missing layer made
`CreateInstance` return without an instance and the process segfaulted shortly after. Since then
`CreateInstance` returns `false`, logs the missing layer, and `Initialize` fails a `DE_CORE_VERIFY`, so the
app now stops with a clear message (a `SIGTRAP` with [L8](#l8)). The Windows SDK installer always ships the
layer; on Linux it's a separate package (or comes with the SDK's `setup-env.sh`), so this is still the first
thing a Linux developer hits.

**Fix**: make the layer optional by moving it to `optionalExtensions.layers` and warning when it's missing.

<a id="l16"></a>
### L16 — Renderer2D pipelines assume the swap-chain format

**Run** — `include/DingoEngine/Graphics/Renderer2D.h:189` and
`src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp:29-60`

`BatchPass::Initialize` builds the quad, circle and text pipelines against
`Renderer::GetSwapChainFramebuffer()`. The swap chain prefers `R8G8B8A8_UNORM` and falls back to BGRA8. X11
surfaces on Mesa only offer `B8G8R8A8_UNORM/SRGB` (verified with llvmpipe).

So 2D drawn into an RGBA8 render target through `Renderer::SetRenderTarget` binds a pipeline whose render
pass doesn't match the active one. The test framework's `TestOutputFramebuffer` (`test/src/TestLayer.cpp:36-41`)
is such a target. Every draw reports `VUID-vkCmdDrawIndexed-renderPass-02684`, which is undefined behaviour;
on `52acd47` that's 7 of the 14 test cases. llvmpipe happens to draw it correctly, but real drivers don't
have to. On Windows the swap chain gets RGBA8 and the formats agree by luck.

Two things are unaffected:

- Games that draw straight to the swap chain. Every example does.
- 3D. `Material` caches a pipeline per target framebuffer (`src/DingoEngine/Graphics/Material.cpp:178-224`).

`TextureFormat` also has no BGRA member (`include/DingoEngine/Graphics/Enums/TextureFormat.h`), so a game
can't even create a render target that matches a BGRA swap chain.
[#89](https://github.com/KingoBoiii/DingoEngine/issues/89) (rendering a scene into a texture) will run
straight into this.

**Fix**: key Renderer2D's pipelines on the target's format the way `Material` does, building or looking them
up in `Flush` against `Renderer::GetCurrentTarget()`. Adding a BGRA8 `TextureFormat` helps too. The cause is
verified (on v0.6.1): forcing render-target textures to BGRA8 made every `02684` error disappear.

<a id="l17"></a>
### L17 — Test font paths differ in case

**Run** — in `test/src/Tests/`:

- `Renderer2D/TextTest.cpp:8`
- `Scene/HierarchyTest2D.cpp:154`
- `Core/BackgroundTest.cpp:32`
- `Input/CursorTest.cpp:14`

All four load `assets/fonts/ArialBD.ttf`, but the file is `test/assets/fonts/arialbd.ttf`. `Font::Create`
returns nullptr and the case crashes or draws no text. A scan of every literal `assets/…` path in `test/`
and `examples/` found no other case mismatch. Paths assembled at runtime weren't covered, but every example
loaded its assets in the runs above.

**Fix**: `assets/fonts/arialbd.ttf`.

<a id="l18"></a>
### L18 — The Asset Manager test expects Windows path-case folding

**Run** (test) — `test/src/Tests/Asset/AssetManagerTest.cpp:32`

The check "FindByPath ignores path casing" encodes Windows behaviour. `AssetManager` folds path case only on
Windows, and deliberately so (`src/DingoEngine/Asset/AssetManager.cpp:79-104`): on a case-sensitive
filesystem the two spellings are different files. It's the one self-check of 188 that failed on Linux.

**Fix**: expect the opposite off Windows.

```cpp
#ifdef DE_PLATFORM_WINDOWS
		Check(assets.FindByPath("Textures/Container.JPG") == m_SyncTexture, "FindByPath ignores path casing");
#else
		Check(assets.FindByPath("Textures/Container.JPG") == k_InvalidAsset, "FindByPath keeps path casing on a case-sensitive filesystem");
#endif
```

<a id="l19"></a>
### L19 — Backslash in a cache sub-path

**Hygiene** — `src/DingoEngine/Graphics/Font.cpp:71`

On Linux, `CacheManager::GetCacheDirectory("fonts\\atlas")` creates a single directory literally named
`fonts\atlas` under `.cache/`. It works, but the cache layout differs between operating systems.

**Fix**: `"fonts/atlas"`. `std::filesystem` accepts `/` on Windows.

### Phase 3 — keep the tree clean

<a id="l20"></a>
### L20 — `premake5 gmake` overwrites FreeType's tracked `Makefile`

**Hygiene** — `vendor/msdf-atlas-gen/msdfgen/freetype/premake5.lua:1` (fork: KingoBoiii/freetype)

premake writes each project's makefile next to its script. FreeType ships its own top-level `Makefile`, which
is tracked in git, and every `premake5 gmake` overwrites it (+358/−25 lines). That's an accidental vendor
modification. The other vendor submodules just gain untracked `Makefile`/`*.make` files and show as dirty.

**Fix**: make `location "premake-build"` the first line of the `freetype` project. Verified: the generated
makefile moves, the tracked one stays untouched, and the library still builds into the same `bin/`.

A workspace-wide `location` does **not** work as-is. The root scripts build every path from
`%{wks.location}`, which would then point into the generated folder.

<a id="l21"></a>
### L21 — Generated makefiles aren't git-ignored

**Hygiene** — `.gitignore`

`premake5 gmake` leaves these untracked in the superproject: `Makefile`, `DingoEngine.make`, `test/Makefile`
and `examples/*/Makefile`.

**Fix**: ignore them, together with the locally built `vendor/assimp/lib/linux-x86_64/` ([L6](#l6)), and
ignore the generated files in the forks' own `.gitignore`s.

### Phase 4 — supported

<a id="l22"></a>
### L22 — Re-enable Linux CI, with a headless smoke test

**Ship** — `.github/workflows/build-master.yml:121-180`

The `build-linux` job is disabled (`if: 1 == 0`, `:122`). As written it can't pass even after Phase 1. It
points `VULKAN_SDK` at `/usr`, where the distro packages are too old ([§1](#prerequisites)). It also builds
only `DingoEngine Dingo-TestFramework` (`:157`).

**Fix**:

- **SDK and assimp**: get the SDK the way the Windows job does, a pinned, cached version with
  `VULKAN_SDK=<sdk>/x86_64`. Alternatively, run [Appendix A](#appendix-a) and cache the prefix. Either way,
  build assimp with its last part and cache `vendor/assimp/lib/linux-x86_64/`.
- **Build**: run `make -j"$(nproc)" config=<cfg>` without a project list, so every example builds. Add
  `debug-asan` to the matrix once [L12](#l12) lands.
- **Smoke test**, exactly as verified here:
  1. Install `xvfb`, `mesa-vulkan-drivers` (llvmpipe) and the validation layers.
  2. Run the test framework once per `--test=` case, each example, and `Marionette --check`, for about 10 s
     each.
  3. Close each window through `WM_DELETE_WINDOW`. This exercises teardown, which a `timeout` kill doesn't.
  4. Fail on any non-zero exit, any AddressSanitizer report, any `[FAIL]` line (the test framework and
     Marionette print one per failed self-check), or any `Validation Error` once [L16](#l16) is fixed.

<a id="l23"></a>
### L23 — Linux release packaging

**Ship** — `.github/workflows/build-release.yml:697-772`

The release workflow has a `build-linux` job too, disabled the same way (`if: 1 == 0`, `:698`). Besides the
[L22](#l22) problems it passes `config=${config}_x86_64` (`:733`), the configuration name that `3db0d2e`
already fixed in the CI workflow (premake's names have no architecture suffix). It also tars the engine-only
`libDingoEngine.a` (`:746`), which consumers can't link without every vendor library.

- **One self-contained archive**, the Linux twin of the `lib.exe` merge (`premake5.lua:251-260`).
  This script, with the static assimp folded in, produced a 64 MB archive. FlappyBird and Marionette linked
  against it plus only the SDK libraries and zlib, with no link groups needed, and ran.

  ```bash
  C=Release-linux-x86_64
  mkdir -p build/dist/$C
  ar -M <<EOF
  CREATE build/dist/$C/libDingoEngine.a
  ADDLIB build/bin/$C/DingoEngine/libDingoEngine.a
  ADDLIB vendor/spdlog/bin/$C/spdlog/libspdlog.a
  ADDLIB vendor/glfw/bin/$C/GLFW/libGLFW.a
  ADDLIB vendor/nvrhi/bin/$C/NVRHI/libNVRHI.a
  ADDLIB vendor/nvrhi/bin/$C/NVRHI-Vulkan/libNVRHI-Vulkan.a
  ADDLIB vendor/imgui/bin/$C/ImGui/libImGui.a
  ADDLIB vendor/msdf-atlas-gen/bin/$C/msdf-atlas-gen/libmsdf-atlas-gen.a
  ADDLIB vendor/msdf-atlas-gen/msdfgen/bin/$C/msdfgen/libmsdfgen.a
  ADDLIB vendor/msdf-atlas-gen/msdfgen/freetype/bin/$C/freetype/libfreetype.a
  ADDLIB vendor/box2d/bin/$C/box2d/libbox2d.a
  ADDLIB vendor/JoltPhysics/bin/$C/Jolt/libJolt.a
  ADDLIB vendor/assimp/lib/linux-x86_64/libassimp.a
  SAVE
  END
  EOF
  ```

  Consumers then link
  `libDingoEngine.a -lshaderc_combined -lspirv-cross-hlsl -lspirv-cross-glsl -lspirv-cross-core -lz -ldl -lpthread`.
  The SDK libraries come from the consumer's own SDK, as on Windows.
- **Packaging**: ship `DingoEngine-<version>-<Config>-linux-x86_64.tar.gz` with `include/`, next to the
  Windows ZIPs.
- **Example tarballs**: the executable plus `assets/`. With the static assimp ([L6](#l6)), at runtime they
  need only `libvulkan1` and a Vulkan driver.

<a id="l24"></a>
### L24 — The docs say Windows-only

**Ship** — `docs/getting-started.md:20-22,55`, `README.md:63,81`, and the Build system section of
`CLAUDE.md` (`:7-13`)

- Getting Started lists "Windows 10/11, x64" and MSVC as the only prerequisites, and promises that
  `links { "DingoEngine" }` pulls in every transitive dependency. That's MSBuild-only until [L4](#l4).
- The README's requirements and build steps are Windows-only.
- CLAUDE.md's Build system section covers MSBuild only.

**Fix**: fold [§1](#prerequisites) and [§2](#building) into Getting Started and the README. Add a Linux line
to CLAUDE.md covering `premake5 gmake`, `make config=<lowercase config>`, the assimp step and the output
paths.

<a id="l25"></a>
### L25 — Developer tooling is Windows-only

**Ship** — `.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/c_cpp_properties.json` and
`vendor/premake/bin/`

- All three `.vscode` files are Windows-only: the MSBuild path, `cppvsdbg`, and
  `cl.exe`/`windows-msvc-x64`. They have grown since v0.6.1 (Candlewick and Marionette entries), but still
  have no Linux entries. CLAUDE.md's new-executable checklist sends people there.
- Only `premake5.exe` is vendored, and generating goes through `Generate-Windows.bat`.

**Fix**:

- Add `make` tasks, `cppdbg` (gdb) launch entries with the example directory as `cwd`, and a Linux
  IntelliSense configuration.
- Vendor the Linux premake binary as `vendor/premake/bin/premake5` next to the `.exe`.
- Add a `Generate-Linux.sh` that runs `premake5 gmake` without pausing.

<a id="l26"></a>
### L26 — Validate on real hardware and desktops

**Ship**

Everything here ran on llvmpipe under Xvfb, with no sound device and no input beyond the built-in
self-checks. Before calling Linux supported:

- **GPUs**: NVIDIA (proprietary driver), AMD (RADV) and Intel (ANV). Each must meet the [§1](#prerequisites)
  runtime requirements. Check each one's X11 surface formats against [L16](#l16).
- **Sessions**: X11, and Wayland through XWayland ([L27](#l27)).
- **Subsystems**:
  - audio (miniaudio picks PipeWire, PulseAudio or ALSA at runtime)
  - gamepads (GLFW's Linux joystick back-end and its bundled mappings)
  - HiDPI and content scale
  - resize and minimise (swap-chain recreation, and v0.7.2's background updates)
  - the `AssetManager`'s hot-reload file polling, and Marionette's `--hot-reload` and `--live-edit-demo`
  - the user-data directory (`Platform.cpp` already follows XDG) and `GetExecutablePath`
    (`/proc/self/exe`)

**Tester bundle**: `build-linux.yml`'s Release job uploads the `DingoEngine-linux-tester` artifact, built by
`scripts/linux-tester/make-bundle.sh` (the same command builds it locally from any configuration). It holds the test app and
every example beside its assets, so it runs without the SDK or a build. The tester follows its README:

- `run-checks.sh` opens every test case and example on the tester's own desktop. It closes them by PID
  through `close-windows.py`, which leaves the tester's other windows alone. At the end it packs the system details,
  a summary and one log per run into a report archive.
- `play.sh` starts any program from its own directory.
- The README's checklist covers the subsystems above.

The binaries need glibc 2.38 and GCC 13's libstdc++ (Ubuntu 24.04-era distros or newer) and `libvulkan.so.1`.

<a id="l27"></a>
### L27 — Native Wayland *(optional)*

**Ship · optional** — `vendor/glfw/premake5.lua:48-51`, which defines only `_GLFW_X11`

The premake builds only GLFW's X11 back-end, so on Wayland desktops the engine runs through XWayland. That's
fine for most users. Native Wayland needs:

- `_GLFW_WAYLAND`
- the `wl_*.c` sources
- `libwayland-dev` and `libxkbcommon-dev`
- protocol headers generated by `wayland-scanner`, as a premake pre-build step

It isn't required for "supported". Decide whether it's worth it.

<a id="l28"></a>
### L28 — Clang *(optional)*

**Ship · optional**

Everything was verified with GCC 13.3. Clang 18 compiles the reduced [L5](#l5) case, but the full tree hasn't
been built with it, and `-Wno-changes-meaning` (if kept) is a GCC-only flag. Try
`make CC=clang CXX=clang++` once L5 uses qualified names.

### Found along the way

<a id="f1"></a>
### F1 — ImGui index upload reads 2 bytes past its buffer

**Run** — `src/DingoEngine/ImGui/ImGuiRenderer.cpp:409,430`; filed as
[#86](https://github.com/KingoBoiii/DingoEngine/issues/86)

NVRHI's Vulkan `writeBuffer` handles uploads up to 64 KiB with `vkCmdUpdateBuffer`. It rounds their size up
to a multiple of 4 and passes that many bytes of the source to the driver
(`vendor/nvrhi/src/vulkan/vulkan-buffer.cpp:457,471-473`). ImGui indices are 16-bit, so any odd index count
reads 2 bytes past `m_IndexBufferData`.

AddressSanitizer reports it as a `heap-buffer-overflow` on the test framework's first frame: `READ of size
7208` from a 7206-byte vector. The read happens in the driver's own `memcpy`, which Linux's ASan intercepts
process-wide. A Windows `Debug-ASan` build (which links since v0.6.3) most likely doesn't see it.

**Fix**: pad the host copy to an even count with
`m_IndexBufferData.resize((drawData->TotalIdxCount + 1) & ~1);`. With it, ASan was clean across every test
case and example.

---

<a id="appendix-a"></a>
## Appendix A — Vulkan SDK components and assimp from source

This is what the verification used in place of LunarG's tarball, plus the static assimp [L6](#l6) needs.
Run it from the repository root.

- It installs the SDK pieces into a prefix with the `include/` + `lib/` layout the premake scripts expect, so
  set `VULKAN_SDK` to that prefix.
- It builds assimp 6.0.4 into `vendor/assimp/lib/linux-x86_64/`. With LunarG's SDK installed, run only the
  last part.
- Validation layers aren't included. Install them separately.
- shaderc is `v2026.4`, the latest tag at the time, with the dependency versions its `DEPS` file pins.
- It takes about 20 minutes on 4 cores, mostly shaderc and assimp.

```bash
#!/bin/bash
# Run from the DingoEngine repository root.
set -euo pipefail
REPO=$PWD
PREFIX=${PREFIX:-$HOME/vulkan-sdk-src/x86_64}
TAG=vulkan-sdk-1.4.357.0
mkdir -p "$PREFIX/include" "$PREFIX/lib" "$REPO/vendor/assimp/lib/linux-x86_64"
cd "$(mktemp -d)"

git clone -q --depth 1 -b $TAG https://github.com/KhronosGroup/Vulkan-Headers.git
cmake -S Vulkan-Headers -B build-vh -G Ninja -DCMAKE_INSTALL_PREFIX="$PREFIX" -DVULKAN_HEADERS_ENABLE_MODULE=OFF
cmake --install build-vh

git clone -q --depth 1 -b $TAG https://github.com/KhronosGroup/SPIRV-Cross.git
cmake -S SPIRV-Cross -B build-sc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_INSTALL_PREFIX="$PREFIX" -DSPIRV_CROSS_SHARED=OFF -DSPIRV_CROSS_CLI=OFF -DSPIRV_CROSS_ENABLE_TESTS=OFF
cmake --build build-sc
cmake --install build-sc

git clone -q --depth 1 -b v2026.4 https://github.com/google/shaderc.git
(cd shaderc && ./utils/git-sync-deps)
cmake -S shaderc -B build-shaderc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON \
      -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DENABLE_GLSLANG_BINARIES=OFF -DSPIRV_TOOLS_BUILD_STATIC=ON
cmake --build build-shaderc --target shaderc_combined
cp -r shaderc/libshaderc/include/shaderc "$PREFIX/include/"
cp build-shaderc/libshaderc/libshaderc_combined.a "$PREFIX/lib/"

# assimp: the same version as the Windows binaries in vendor/assimp, whose headers it shares.
git clone -q --depth 1 -b v6.0.4 https://github.com/assimp/assimp.git
cmake -S assimp -B build-assimp -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF
cmake --build build-assimp
cp build-assimp/lib/libassimp.a "$REPO/vendor/assimp/lib/linux-x86_64/"
```

<a id="appendix-b"></a>
## Appendix B — The verified patch

This is the exact diff behind [§3](#verified), against `52acd47`. Each block applies cleanly with `git apply`
from the root of its own repository (checked with `git apply --check`).

- **Contains**: Phase 1 (with the [L5](#l5) stopgap), [L14](#l14), [L17](#l17), [L18](#l18), [L20](#l20),
  [L21](#l21) and [F1](#f1).
- **Leaves out**: [L15](#l15), [L16](#l16), [L19](#l19) and Phase 4.
- The static `libassimp.a` isn't in it: [Appendix A](#appendix-a) builds it.

The three fork patches belong in KingoBoiii/glfw, KingoBoiii/NVRHI and KingoBoiii/freetype, followed by a
submodule bump here. Alternatively, apply the same settings from the root `premake5.lua` (see [L7](#l7)).

<details>
<summary><b>Superproject</b>: <code>KingoBoiii/DingoEngine</code> (17 files)</summary>

```diff
diff --git a/.gitignore b/.gitignore
index 6c51274..a99d51e 100644
--- a/.gitignore
+++ b/.gitignore
@@ -46,6 +46,11 @@ Intermediates/
 **.vcxproj
 **.vcxproj.filters
 **.vcxproj.user
+/Makefile
+/*.make
+/test/Makefile
+/examples/*/Makefile
+/vendor/assimp/lib/linux-x86_64/
 **.csproj
 
 !Vendor/**
diff --git a/examples/Marionette/src/LiveEdit.cpp b/examples/Marionette/src/LiveEdit.cpp
index 2f78021..eae6a16 100644
--- a/examples/Marionette/src/LiveEdit.cpp
+++ b/examples/Marionette/src/LiveEdit.cpp
@@ -55,7 +55,11 @@ namespace
 	{
 		std::error_code error;
 		const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
+#ifdef DE_PLATFORM_WINDOWS
 		return !error && (std::filesystem::is_symlink(status) || status.type() == std::filesystem::file_type::junction);
+#else
+		return !error && std::filesystem::is_symlink(status);
+#endif
 	}
 
 	bool IsPlain(const std::filesystem::path& path)
diff --git a/include/DingoEngine/Assertion.h b/include/DingoEngine/Assertion.h
index 1732dbf..fd69c23 100644
--- a/include/DingoEngine/Assertion.h
+++ b/include/DingoEngine/Assertion.h
@@ -5,8 +5,13 @@
 
 #include "Log.h"
 
-#ifdef DE_PLATFORM_WINDOWS
+#if defined(DE_PLATFORM_WINDOWS)
 #define DE_DEBUG_BREAK __debugbreak()
+#elif defined(DE_PLATFORM_LINUX)
+#include <csignal>
+#define DE_DEBUG_BREAK std::raise(SIGTRAP)
+#else
+#define DE_DEBUG_BREAK
 #endif
 
 #ifdef DE_DEBUG
diff --git a/include/DingoEngine/Graphics/Renderer2D.h b/include/DingoEngine/Graphics/Renderer2D.h
index 0df4484..177faed 100644
--- a/include/DingoEngine/Graphics/Renderer2D.h
+++ b/include/DingoEngine/Graphics/Renderer2D.h
@@ -35,6 +35,20 @@ namespace Dingo
 		Renderer2DCapabilities Capabilities = {};
 	};
 
+	struct Renderer2DTextParameters
+	{
+		glm::vec4 Color{ 1.0f };
+		float Kerning = 0.0f;
+		float LineSpacing = 0.0f;
+
+		// Horizontally center the string on position.x instead of starting there. The
+		// width is taken from the pen while the glyphs are emitted and the quads are
+		// shifted afterwards, so this costs one walk of the string where
+		// GetStringWidth() + DrawText() costs two. Multi-line strings center as a
+		// block on their widest line, matching what GetStringWidth() reports.
+		bool Centered = false;
+	};
+
 	class Renderer2D
 	{
 	public:
@@ -69,19 +83,7 @@ namespace Dingo
 
 		void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f);
 
-		struct TextParameters
-		{
-			glm::vec4 Color{ 1.0f };
-			float Kerning = 0.0f;
-			float LineSpacing = 0.0f;
-
-			// Horizontally center the string on position.x instead of starting there. The
-			// width is taken from the pen while the glyphs are emitted and the quads are
-			// shifted afterwards, so this costs one walk of the string where
-			// GetStringWidth() + DrawText() costs two. Multi-line strings center as a
-			// block on their widest line, matching what GetStringWidth() reports.
-			bool Centered = false;
-		};
+		using TextParameters = Renderer2DTextParameters;
 
 		// `string` is UTF-8; a byte that is not valid UTF-8 reads as Latin-1. The atlas bakes
 		// Latin-1, the printable General Punctuation and the euro sign; anything else draws '?'.
diff --git a/premake5.lua b/premake5.lua
index 5b3ae97..0de9b03 100644
--- a/premake5.lua
+++ b/premake5.lua
@@ -38,6 +38,9 @@ workspace "DingoEngine"
             -- LNK2038. cl embeds /INFERASANLIBS, so the linker needs no ASan flag of its own.
             defines { "_DISABLE_STRING_ANNOTATION", "_DISABLE_VECTOR_ANNOTATION" }
 
+        filter { "system:linux", "configurations:Debug-ASan" }
+            linkoptions { "-fsanitize=address" }
+
 	    filter "system:windows"
 		    buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }
 
@@ -77,15 +80,26 @@ function copyAssimpRuntime()
     local debugBin   = path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/bin/debug")
     local releaseBin = path.join(_MAIN_SCRIPT_DIR, "vendor/assimp/bin/release")
 
-    filter "configurations:Debug or configurations:Debug-ASan"
+    filter { "system:windows", "configurations:Debug or configurations:Debug-ASan" }
         postbuildcommands { '{COPY} "' .. debugBin .. '" "%{cfg.targetdir}"' }
 
-    filter "configurations:Release or configurations:Distribution"
+    filter { "system:windows", "configurations:Release or configurations:Distribution" }
         postbuildcommands { '{COPY} "' .. releaseBin .. '" "%{cfg.targetdir}"' }
 
-    filter "configurations:Debug-ASan"
+    filter { "system:windows", "configurations:Debug-ASan" }
         postbuildcommands { '{COPY} "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "%{cfg.targetdir}"' }
 
+    filter "system:linux"
+        buildoptions { "-Wno-changes-meaning" }
+        linkgroups "On"
+        libdirs { "%{LibraryDir.vulkan}", "%{LibraryDir.assimp}" }
+        links {
+            "DingoEngine", "spdlog", "GLFW", "NVRHI", "NVRHI-Vulkan", "ImGui",
+            "msdf-atlas-gen", "msdfgen", "freetype", "box2d", "Jolt",
+            "shaderc_combined", "spirv-cross-hlsl", "spirv-cross-glsl", "spirv-cross-core",
+            "assimp", "z", "dl", "pthread"
+        }
+
     filter {}
 end
 
@@ -101,7 +115,7 @@ IncludeDir['stb'] = "%{wks.location}/vendor/stb/include";
 IncludeDir['imgui'] = "%{wks.location}/vendor/imgui";
 IncludeDir["msdfgen"] = "%{wks.location}/vendor/msdf-atlas-gen/msdfgen"
 IncludeDir["msdf_atlas_gen"] = "%{wks.location}/vendor/msdf-atlas-gen/msdf-atlas-gen"
-IncludeDir['vulkan'] = "%{VULKAN_SDK}/Include";
+IncludeDir['vulkan'] = "%{VULKAN_SDK}/include";
 IncludeDir['dx_headers'] = "%{wks.location}/vendor/nvrhi/thirdparty/DirectX-Headers/include";
 IncludeDir['assimp'] = "%{wks.location}/vendor/assimp/include";
 IncludeDir['entt'] = "%{wks.location}/vendor/entt/include";
@@ -130,6 +144,18 @@ Library["SPIRV_Cross_Release"] = "%{LibraryDir.vulkan}/spirv-cross-core.lib"
 Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.vulkan}/spirv-cross-glsl.lib"
 Library["SPIRV_Cross_HLSL_Release"] = "%{LibraryDir.vulkan}/spirv-cross-hlsl.lib"
 
+if os.istarget("linux") then
+    LibraryDir['assimp'] = "%{wks.location}/vendor/assimp/lib/linux-x86_64"
+    Library['assimp_Debug']   = "assimp"
+    Library['assimp_Release'] = "assimp"
+    for _, cfg in ipairs({ "Debug", "Release" }) do
+        Library["ShaderC_" .. cfg]          = "shaderc_combined"
+        Library["SPIRV_Cross_" .. cfg]      = "spirv-cross-core"
+        Library["SPIRV_Cross_GLSL_" .. cfg] = "spirv-cross-glsl"
+        Library["SPIRV_Cross_HLSL_" .. cfg] = "spirv-cross-hlsl"
+    end
+end
+
 -- Windows
 Library["WinSock"] = "Ws2_32.lib"
 Library["WinMM"] = "Winmm.lib"
@@ -199,7 +225,6 @@ group "Engine"
 		links {
 			"spdlog",
 			"glfw",
-			"%{Library.vulkan}",
 			"nvrhi",
 			"imgui",
 			"msdf-atlas-gen",
@@ -208,7 +233,9 @@ group "Engine"
 		}
 
 		defines {
-			"GLFW_INCLUDE_NONE"
+			"GLFW_INCLUDE_NONE",
+			"SPDLOG_COMPILED_LIB",
+			"SPDLOG_USE_STD_FORMAT"
 		}
 
 		filter "files:src/**/Shaders/*.glsl"
@@ -230,6 +257,7 @@ group "Engine"
 		filter "system:windows"
 			systemversion "latest"
 			defines { "DE_PLATFORM_WINDOWS", "GLFW_EXPOSE_NATIVE_WIN32" }
+			links { "%{Library.vulkan}" }
 			buildoptions { "/utf-8" }
 
 			includedirs {
@@ -262,6 +290,7 @@ group "Engine"
 		filter "system:linux"
 			defines { "DE_PLATFORM_LINUX" }
 			buildoptions { "-Wno-changes-meaning" }
+			removefiles { "src/DingoEngine/Graphics/NVRHI/DirectX11/**", "src/DingoEngine/Graphics/NVRHI/DirectX12/**" }
 
 		filter "configurations:Debug or configurations:Debug-ASan"
 			runtime "Debug"
diff --git a/src/DingoEngine/Graphics/NVRHI/DirectX11/DirectX11SwapChain.cpp b/src/DingoEngine/Graphics/NVRHI/DirectX11/DirectX11SwapChain.cpp
index b9b7b43..5090e24 100644
--- a/src/DingoEngine/Graphics/NVRHI/DirectX11/DirectX11SwapChain.cpp
+++ b/src/DingoEngine/Graphics/NVRHI/DirectX11/DirectX11SwapChain.cpp
@@ -1,7 +1,7 @@
 #include "depch.h"
 #include "DirectX11SwapChain.h"
 
-#include <glfw/glfw3.h>
+#include <GLFW/glfw3.h>
 #include <GLFW/glfw3native.h>
 
 #include "DirectX11GraphicsContext.h"
diff --git a/src/DingoEngine/Graphics/NVRHI/DirectX12/DirectX12SwapChain.cpp b/src/DingoEngine/Graphics/NVRHI/DirectX12/DirectX12SwapChain.cpp
index 0cac17f..c12a1a9 100644
--- a/src/DingoEngine/Graphics/NVRHI/DirectX12/DirectX12SwapChain.cpp
+++ b/src/DingoEngine/Graphics/NVRHI/DirectX12/DirectX12SwapChain.cpp
@@ -1,7 +1,7 @@
 #include "depch.h"
 #include "DirectX12SwapChain.h"
 
-#include <glfw/glfw3.h>
+#include <GLFW/glfw3.h>
 #include <GLFW/glfw3native.h>
 
 #include "DirectX12GraphicsContext.h"
diff --git a/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp b/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp
index e1a074a..fd6e34e 100644
--- a/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp
+++ b/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp
@@ -2,7 +2,7 @@
 #include "VulkanGraphicsContext.h"
 #include "VulkanCommon.h"
 
-#include <glfw/glfw3.h>
+#include <GLFW/glfw3.h>
 
 #include <iostream>
 
diff --git a/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp b/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp
index 2e56a31..e898123 100644
--- a/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp
+++ b/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.cpp
@@ -1,7 +1,7 @@
 #include "depch.h"
 #include "VulkanSwapChain.h"
 
-#include <glfw/glfw3.h>
+#include <GLFW/glfw3.h>
 #include "VulkanGraphicsContext.h"
 
 #include <algorithm>
diff --git a/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h b/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h
index e5bb118..bbd14c1 100644
--- a/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h
+++ b/src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h
@@ -22,7 +22,7 @@ namespace Dingo
 		virtual void AcquireNextImage() override;
 		virtual void Present() override;
 
-		virtual Framebuffer* SwapChain::GetCurrentFramebuffer() const
+		virtual Framebuffer* GetCurrentFramebuffer() const override
 		{
 			return GetFramebuffer(m_SwapChainIndex);
 		}
diff --git a/src/DingoEngine/ImGui/ImGuiRenderer.cpp b/src/DingoEngine/ImGui/ImGuiRenderer.cpp
index 05b9c79..4a137de 100644
--- a/src/DingoEngine/ImGui/ImGuiRenderer.cpp
+++ b/src/DingoEngine/ImGui/ImGuiRenderer.cpp
@@ -406,7 +406,7 @@ void main()
 		}
 
 		m_VertexBufferData.resize(drawData->TotalVtxCount);
-		m_IndexBufferData.resize(drawData->TotalIdxCount);
+		m_IndexBufferData.resize((drawData->TotalIdxCount + 1) & ~1);
 
 		// copy and convert all vertices into a single contiguous buffer
 		ImDrawVert* vtxDst = &m_VertexBufferData[0];
diff --git a/src/DingoEngine/Windowing/Window.cpp b/src/DingoEngine/Windowing/Window.cpp
index 76e7fc0..18914b5 100644
--- a/src/DingoEngine/Windowing/Window.cpp
+++ b/src/DingoEngine/Windowing/Window.cpp
@@ -8,7 +8,7 @@
 #include "DingoEngine/Events/MouseEvents.h"
 #include "DingoEngine/Events/GamepadEvents.h"
 
-#include <glfw/glfw3.h>
+#include <GLFW/glfw3.h>
 
 namespace Dingo
 {
diff --git a/test/src/Tests/Asset/AssetManagerTest.cpp b/test/src/Tests/Asset/AssetManagerTest.cpp
index b56da9c..9dfa311 100644
--- a/test/src/Tests/Asset/AssetManagerTest.cpp
+++ b/test/src/Tests/Asset/AssetManagerTest.cpp
@@ -29,7 +29,11 @@ namespace Dingo
 		Check(assets.Load("textures/container.jpg") == m_SyncTexture, "re-Load dedups to the same handle");
 		Check(assets.Get<Texture>(m_SyncTexture) == assets.GetTexture(m_SyncTexture), "Get<Texture> matches GetTexture");
 		Check(assets.FindByPath("textures/container.jpg") == m_SyncTexture, "FindByPath resolves the handle");
+#ifdef DE_PLATFORM_WINDOWS
 		Check(assets.FindByPath("Textures/Container.JPG") == m_SyncTexture, "FindByPath ignores path casing");
+#else
+		Check(assets.FindByPath("Textures/Container.JPG") == k_InvalidAsset, "FindByPath keeps path casing on a case-sensitive filesystem");
+#endif
 		Check(assets.FindByPath(assets.ResolvePath("textures/container.jpg")) == m_SyncTexture, "FindByPath folds an absolute path under the root");
 		Check(assets.GetShader(m_SyncTexture) == nullptr, "typed Get of the wrong type returns nullptr");
 
diff --git a/test/src/Tests/Core/BackgroundTest.cpp b/test/src/Tests/Core/BackgroundTest.cpp
index 5b006e6..a01a007 100644
--- a/test/src/Tests/Core/BackgroundTest.cpp
+++ b/test/src/Tests/Core/BackgroundTest.cpp
@@ -29,7 +29,7 @@ namespace Dingo
 	{
 		Renderer2DTest::Initialize();
 
-		m_Font = Font::Create("assets/fonts/ArialBD.ttf");
+		m_Font = Font::Create("assets/fonts/arialbd.ttf");
 
 		m_AppUpdateInBackground = Application::Get().GetUpdateInBackground();
 		if (auto value = Application::Get().GetCommandLineArgs().Get("update-in-background"))
diff --git a/test/src/Tests/Input/CursorTest.cpp b/test/src/Tests/Input/CursorTest.cpp
index 7153b1a..88e9c42 100644
--- a/test/src/Tests/Input/CursorTest.cpp
+++ b/test/src/Tests/Input/CursorTest.cpp
@@ -11,7 +11,7 @@ namespace Dingo
 	{
 		Renderer2DTest::Initialize();
 
-		m_Font = Font::Create("assets/fonts/ArialBD.ttf");
+		m_Font = Font::Create("assets/fonts/arialbd.ttf");
 
 		Input::SetCursorMode(CursorMode::Normal);
 		m_LookYaw = 0.0f;
diff --git a/test/src/Tests/Renderer2D/TextTest.cpp b/test/src/Tests/Renderer2D/TextTest.cpp
index b8a7e65..ec0334b 100644
--- a/test/src/Tests/Renderer2D/TextTest.cpp
+++ b/test/src/Tests/Renderer2D/TextTest.cpp
@@ -5,7 +5,7 @@ namespace Dingo
 
 	void TextTest::Initialize()
 	{
-		m_ArialFont = Font::Create("assets/fonts/ArialBD.ttf");
+		m_ArialFont = Font::Create("assets/fonts/arialbd.ttf");
 	}
 
 	void TextTest::Cleanup()
diff --git a/test/src/Tests/Scene/HierarchyTest2D.cpp b/test/src/Tests/Scene/HierarchyTest2D.cpp
index 6bb2a7b..5b9dec5 100644
--- a/test/src/Tests/Scene/HierarchyTest2D.cpp
+++ b/test/src/Tests/Scene/HierarchyTest2D.cpp
@@ -151,7 +151,7 @@ namespace Dingo
 
 	void HierarchyTest::BuildScene2D()
 	{
-		m_Font = Font::Create("assets/fonts/ArialBD.ttf");
+		m_Font = Font::Create("assets/fonts/arialbd.ttf");
 		m_Scene2D = new Scene("Hierarchy Test 2D");
 
 		Entity camera = m_Scene2D->CreateEntity("Camera");
```

</details>

<details>
<summary><b>Fork</b>: <code>vendor/glfw</code> → KingoBoiii/glfw (L7)</summary>

```diff
diff --git a/premake5.lua b/premake5.lua
index 61c8abc..1575416 100644
--- a/premake5.lua
+++ b/premake5.lua
@@ -39,6 +39,8 @@ project "GLFW"
 			"src/xkb_unicode.c",
 			"src/posix_time.c",
 			"src/posix_thread.c",
+			"src/posix_module.c",
+			"src/posix_poll.c",
 			"src/glx_context.c",
 			"src/egl_context.c",
 			"src/osmesa_context.c",
```

</details>

<details>
<summary><b>Fork</b>: <code>vendor/nvrhi</code> → KingoBoiii/NVRHI (L2)</summary>

```diff
diff --git a/premake5.lua b/premake5.lua
index 8ee94c7..35835d0 100644
--- a/premake5.lua
+++ b/premake5.lua
@@ -32,7 +32,7 @@ project "NVRHI-Vulkan"
 
 		"rtxmu/include",
 
-		"%{VULKAN_SDK}/Include",
+		"%{VULKAN_SDK}/include",
 	}
 
 	filter "system:windows"
```

</details>

<details>
<summary><b>Fork</b>: <code>vendor/msdf-atlas-gen/msdfgen/freetype</code> → KingoBoiii/freetype (L20)</summary>

```diff
diff --git a/premake5.lua b/premake5.lua
index cf4eb22..9d434d0 100644
--- a/premake5.lua
+++ b/premake5.lua
@@ -1,4 +1,5 @@
 project "freetype"
+	location "premake-build"
 	kind "StaticLib"
 	language "C"
   staticruntime "off"
```

</details>
