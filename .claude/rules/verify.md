---
paths:
  - "src/**"
  - "include/**"
  - "test/**"
  - "examples/**"
  - "**/*.glsl"
  - "**/premake5.lua"
---

**Verifying a code change** (CLAUDE.md "Verifying changes" covers docs-only changes and what never to run or claim)

A PR's `lint.yml` only syntax-compiles its changed files with GCC and MSVC (warnings reported, never fatal), and the full smoke test (`build-linux.yml`, plus `build-windows.yml`) runs only on master, so these steps are its only build-and-run check before the owner runs the GPU backends.

- **Code**: build Debug on Linux, only the projects you touched (the engine plus the test app or example that uses it). A change to a public header or a `premake5.lua` builds every project (`make -j"$(nproc)" config=debug`), since nothing else catches a broken example before merge. No warnings in files you changed.
- **Run only the affected feature's test case**: `cd test && timeout 60 xvfb-run -a ../build/bin/Debug-linux-x86_64/Dingo-TestFramework/Dingo-TestFramework --test=<case>`. The app stays open after its checks, so the timeout ends it. Pass = no `[FAIL]`, no `VUID-`, no `LOG ERROR` in the output. If you changed an example that has `--check`, run that too. On Windows, build the same projects with MSBuild as described under CLAUDE.md "Build and run", and run the test app directly from `test/` with the same `--test=<case>` flag and the same pass criteria.
- **Add a check only for new behaviour or a fixed bug**, inside the existing test case (`test/src/Tests/`, helpers in `Tests/TestChecks.h`). Pure engine code (no window or device) gets a doctest case in `test/UnitTests` instead, run with `../build/bin/Debug-linux-x86_64/Dingo-UnitTests/Dingo-UnitTests`.
