# P15 — *Marionette* on v0.9: VFX from the clips, impacts, moonlit shadows

Drafted 2026-10-08 on branch `claude/dingo-v0-9-0-planning-ee00a9` @ `719df64` (v0.9 P1–P14 built,
none of it run on a GPU yet). Scope source: §6 of `.claude/plans/2026-10-06-v0.9-shadows-post-vfx-plan.md`.
The game's own design is `.claude/plans/2026-10-03-marionette-plan.md`, whose "As built" holds.

**Status**: drafted 2026-10-08; decisions M1–M4 (§4) settled on the recommended options. Building V0–V4.

---

## 1. Where we start (read 2026-10-08)

- **Events and sockets:**
  - every combat window is a clip event: `step_l`/`step_r`, `dash`, `hitbox` (a range), `parry`, `iframes`;
  - `Fighter::OnAnimationEvent` turns footsteps into audio and the hitbox's begin into a swing sound;
  - weapons hang on the `handslot.r`/`handslot.l` sockets;
  - the blade's spheres are children of the weapon part, along `MeasureBlade`'s axis;
  - the feet are joints `foot.l`/`foot.r`.
- **Combat** (`Combat.cpp`):
  - `Judge` finds a touching pair: a swept blade sphere against a hurt sphere;
  - `Commit` applies Hit, Blocked, Parry or Dodged, with a sound and a 0.07 s hit stop;
  - nothing visual marks a contact, and the contact point is never kept.
- **The arena** (`ArenaWorld.cpp`):
  - a moon (`DirectionalLightComponent`, no shadow), ambient light, four braziers (point lights,
    9 m, and emissive sphere cores at `FLAME_EMISSIVE` 1.1), and the floor and walls;
  - `UpdateOcclusion` hides a wall or brazier that stands between the camera and a fighter, with
    `Visible = false`.
- **Cameras:** `FollowCamera` frames the duel; `CameraRig` frames the lineup and the overview.
- **Captures and tests:**
  - `--freeze` and `--tournament` imply a fixed delta;
  - `--perf` (with `--autoplay`) logs the mean frame, update and render times;
  - `--check` runs asset, movement, combat and AI checks read from the log;
  - a tournament's results must not change (the VFX must touch no gameplay state).

## 2. Design

### 2.1 VFX from events (V1)

On every fighter with an animator, a `ParticleEventComponent`, the P11 path, with no game code per
event:

| Event | Binding | Effect |
|---|---|---|
| `step_l`, `step_r` | `Bind(event, foot emitter, n)` | Low, flat, alpha dust (`FootDust`) from an emitter on the `foot.l`/`foot.r` socket |
| `dash` | `Bind`, a larger burst | The same dust from a hips-level emitter |
| `hitbox` | `BindRange` | `BladeTrail`, an additive streak, from an emitter at the blade's tip (the outermost blade sphere's offset, parented to the weapon part) |

- Each footfall bursts `FOOT_DUST_COUNT` (6). The dash bursts `DASH_DUST_COUNT` (18).
- The trail is world space, so a swing leaves an arc behind it.
- A fighter without a measured blade gets no trail.
- `Fighter::OnAnimationEvent` is unchanged.

### 2.2 Impacts (V2)

- **The contact point:** `Judge` keeps the first touching pair and stores where they meet, on the
  hurt sphere's surface towards the blade: `Decision::Contact`.
- **`ArenaVfx`** (new `ArenaVfx.h/.cpp`) owns the arena's effects and one rate-0 emitter entity per
  impact kind. `Commit` calls `ArenaVfx::Impact(kind, contact)`, which runs
  `Scene::EmitParticlesAt`:

  | Outcome | Effect |
  |---|---|
  | Hit | `HitSparks`: hot orange, fast, with gravity, 26 |
  | Blocked | `BlockSparks`: steel blue-white, bouncing off the shield, 34 |
  | Parry | `ParryFlash`: one big HDR flash that blooms, plus a ring of `BlockSparks`, 1 + 40 |
  | Dodged | Nothing |

- **K.O. dust:** `Combat` asks `ArenaVfx::KnockOut(position)` when a hit kills. The burst fires
  `KO_DUST_DELAY` (0.65 s) later, about when the body lands. `ArenaVfx::Update(dt)`, which the
  director calls each frame, drains the queue.
- **Brazier flames:** each brazier gets the flame, embers and smoke of *Candlewick*'s keep, at the
  arena's own scale. The effects are copied into `ArenaVfx`, not shared: the two examples are separate
  programs.
- **Occlusion:** a brazier that `UpdateOcclusion` hides stops its emitters, and plays them again when
  it is restored.

### 2.3 Light (V3)

- **The moon casts:** `DirectionalLightComponent::CastShadows`, four cascades by default. The
  fighters are skinned, so both throw skinned shadows. The arena is about 16 m across, so the
  default `MaxDistance` 60 holds it all.
- **The braziers cast** too: four point lights at six faces each fit the default 8 shadow slots.
  They give the fighters a second, warm shadow that turns as they circle.
- **What casts nothing:** the flame cores and the debug hit spheres are `ShadowCasting::Off`.
- **Occlusion:** a wall or brazier the camera sees through turns `ShadowsOnly` instead of
  `Visible = false`, so its shadow stays; the flame core is still hidden.
- **Post:** `PostProcessComponent` on both cameras:
  - `Soft` tone mapping;
  - bloom at intensity `BLOOM_INTENSITY` (0.3);
  - AO at its defaults.

  `FLAME_EMISSIVE` goes from 1.1 to 2.6, so the cores bloom.
- **Switches:** `--no-post`, `--no-shadows` and `--no-particles`, for A/B captures and `--perf`.

### 2.4 What must not change

- **Gameplay:** the VFX read gameplay and never write it. `--tournament=10 --seed=1` must report the
  same results before and after, which `--check` and the tournament line show in the log.
- **Determinism:** particles take no part in the fixed-delta step; their seeds come from the
  renderer's scene counter, so a frozen capture stays reproducible.

## 3. Milestones

| | What | Done when |
|---|---|---|
| **V0** | `--no-post`, `--no-shadows`, `--no-particles` parsed | `--autoplay --perf` per backend on the current build, as the "before" |
| **V1** | Footfall and dash dust, the blade trail, from `ParticleEventComponent` | `--drive=duel` captures: dust at each foot's touchdown, a trail exactly while the hitbox is open |
| **V2** | Contact sparks, block sparks, the parry flash, K.O. dust, brazier particles | A capture of each outcome (`--check` already provokes each). The tournament is unchanged |
| **V3** | Moon and brazier shadows, `ShadowsOnly` occlusion, post chain, AO, emissives | Before/after `--freeze` and `--freeze --lineup`; `--perf` against V0 |
| **V4** | Review pass in a fresh agent, fixes, docs | Every Critical/High fixed |

## 4. Decisions (settled 2026-10-08 on the recommended options; the user can revisit)

- **M1** Footfalls, dash and trail go through `ParticleEventComponent`, not `OnAnimationEvent`. That
  is the feature P11 built, and the game is its showcase.
- **M2** Impacts are emitted from `Combat::Commit`, the one place an outcome is decided, through
  `ArenaVfx`, so `Combat` stays free of scene code beyond a pointer.
- **M3** The braziers cast as well as the moon (§2.3). With `--no-shadows` neither does.
- **M4** The brazier effects are duplicated from *Candlewick*'s (§2.2).

## 5. Risks

- **Trail density:** at 60 fps a fast swing moves the tip about 10 cm a frame, so a rate of 400/s
  leaves dots, not a streak. Mitigation: short-lived, larger, stretched-looking particles; tune in
  the F4 editor.
- **Hit stop:** the 0.07 s hit stop slows the animators but not the particles, so sparks fly on while
  the fighters freeze. That reads as impact, and is intended.
- **Event count:** a footfall for each fighter's step, and a range per swing: a handful of particle
  events a second, far inside the pool.

## 6. Verification (on the GPU machine)

- **Builds:** Marionette Debug and Release on all three backends.
- **Checks:** `--check` passes as before (combat, movement, AI). `--tournament=10 --seed=1
  --player-tier=3` reports the same win counts as on `719df64`.
- **Captures:**
  - `--freeze` and `--freeze --lineup` before/after, into the visual log;
  - an outcome gallery from `--drive=duel`.
- **Perf:** `--autoplay --perf --vsync=off` per backend, Release, with each `--no-*` flag and none.

## 7. As built

*(filled in per milestone)*
