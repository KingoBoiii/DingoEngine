# Versioning

DingoEngine follows [Semantic Versioning](https://semver.org) from 1.0.0 on. Before 1.0.0 any
release may break the API; every break between v0.9 and v1.0 is listed in `migrating-to-1.0.md`.

## What a version number promises

| Release | May | May not |
|---|---|---|
| **Patch** (1.0.x) | Fix bugs. Change a picture only to fix a filed bug. | Add, rename or remove public API. |
| **Minor** (1.x.0) | Add API: new types, functions, overloads, enum values, struct members (see below). Deprecate. | Remove or rename anything, or change what existing code means. |
| **Major** (2.0.0) | Anything, each break listed in a migration guide. | — |

- **A deprecation lives at least one minor release.** The old name is marked `[[deprecated("...")]]`
  with the replacement in the message and keeps working until the next major release removes it.
- **A change in how a game looks is a break**, unless it fixes a filed bug (a GitHub `bug` issue).
  Tone-mapping curves, default lights, shadow filtering, blending: a game that ships pixels it tuned
  can rely on them through 1.x.
- **A change in how a game simulates is a break** too: physics stepping, script and system order,
  animation sampling and event timing.

## What counts as the public API

Everything declared under `include/DingoEngine/`, outside a `namespace Internal`, and the behaviour
the docs in `docs/` describe. Not part of it:

- anything in `Dingo::Internal`, and everything under `src/`;
- the log text and the debug window's layout;
- the shader disk cache and font atlas cache formats (they rebuild themselves);
- the `.cache/` and log folders' file names.

## Source compatibility, not ABI

The promise is about **source**: a game that compiles against 1.0's headers compiles against any
1.x's and behaves the same. It is not about binaries. The engine ships as a static library rebuilt
for every release, and private members (`Renderer3D`'s, `Animator`'s) may change size in a minor
release, so always rebuild the game against the headers that came with the library.

## Rules that keep minor releases compatible

- **Params structs are aggregates, and their members are append-only.** Games build
  `ApplicationParams`, `WindowParams`, `GraphicsParams` and the rest with designated initializers,
  which fail to compile when a member they name moves before another. A minor release adds a member
  only at the end, never reorders or removes one, and never gives these structs a user-declared
  constructor (which would end aggregate initialization).
- **Function parameters are append-only too**, and only with a default: `Renderer3D::SubmitMesh` and
  `SubmitSkinnedMesh` keep their current order through 1.x. New ways to call them come as new
  overloads.
- **Enums only gain values at the end**, so stored values (settings, saves) keep their meaning.
- **Virtual functions a game overrides** (`Layer`, `ScriptableEntity`, `Application`) gain new
  virtuals with empty default bodies only.

## Simulation timing

Physics steps once per `Scene::OnUpdate` with that update's delta (capped at 4/60 s): 3D in
`ceil(dt * 60 - 0.1)` collision steps, clamped to 1..4, and 2D with 4 substeps. A script's
`OnFixedUpdate(step)` runs from a separate accumulator (`Scene::SetFixedTimeStep`, 1/60 s by default)
before `OnUpdate`; it is for game logic on a fixed clock and does not step physics. Moving physics
onto the fixed step would change how every game simulates, so 1.x keeps it as it is; a fixed-step
physics mode, if it comes, is opt-in.

## Version numbers in the build

`VERSION` at the repository root holds `major.minor.patch`. Premake writes
`include/DingoEngine/BuildInfo.h` (git-ignored) from it on every generate, and CI sets
`DE_BUILD_NUMBER` (the commit count) and, for a tagged release, `DE_VERSION`.
`Application::GetEngineVersion()` returns the packed version (`DE_VERSION_MAJOR/MINOR/PATCH` decode
it) and `GetEngineBuildNumber()` the build number.
