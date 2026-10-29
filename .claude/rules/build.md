---
paths:
  - "**/premake5.lua"
  - ".github/workflows/**"
  - "scripts/**"
  - "**/dpak.key"
---

**Build and shipping**
- Never modify anything under `vendor/` (`.claude/settings.json` denies edits there). Force pushes, pushes to `master` and rebases of a pushed branch are blocked by `.claude/hooks/guard-git.sh`.
- Every executable that links the engine calls `copyAssimpRuntime()` in its `premake5.lua`, or it dies with `STATUS_DLL_NOT_FOUND` before `main` (on Linux it fails to link). A game that ships packed assets also calls `embedPackKey()` after it and passes `Dingo::Generated::GetPackageKey()` as `ApplicationParams.Assets.PackageKey`. Each example and the test app also calls `exeMetadata { Name = ..., Company = ..., Copyright = ... }` after it, with an `icon.ico` and `icon.png` beside it (`scripts/icons/make-icons.sh`), or it ships without a version resource, icon or desktop entry. Add a new example to `.github/workflows/build-release.yml`'s packaging and the `.vscode` entries too.
- Never leave a `.dpak` in `build/bin/`: the engine mounts `<exe stem>.dpak` at start-up. Re-run premake after changing a `dpak.key`.
