# P14 — *Candlewick* on v0.9: shadows you hide in, bloom, particle flames

Drafted 2026-10-08 on branch `claude/dingo-v0-9-0-planning-ee00a9` @ `0543d88` (v0.9 P1–P13 done, none
of it run on a GPU yet). Scope source: §6 of `.claude/plans/2026-10-06-v0.9-shadows-post-vfx-plan.md`.
The game's own design is `.claude/plans/2026-10-01-candlewick-plan.md`, whose §11 "As built" holds.

**Status**: drafted 2026-10-08; decisions C1–C4 (§4) settled on the recommended options. Building W0–W5.

---

## 1. Where we start (read 2026-10-08)

- **Lights:** every gameplay light is a component the game writes each frame:
  - the lantern, a point light (`Lantern::Apply`; range = oil, `LANTERN_RANGE_MIN..MAX`);
  - five braziers, points that light when kindled (`Braziers::Light`), the altar among them;
  - four wardens, each a lamp (point, 3.5 m) and an eye (spot, 8 m, 24° outer).

  Sconces and candles are decorative points, which `LightLod` fades in and out to keep the budget
  (`GameTuning.h`: `GAMEPLAY_LIGHTS_MAX = 14`).
- **Detection** (`Detection.cpp`) weighs three samples on the player (feet, chest, head):
  - the cone weight is `GetLightAttenuation(eye, sample) >= SEEN_WEIGHT`, then a physics line-of-sight ray;
  - the beacon is the lantern's `1.6 × range`, or 6 m while `IsFlameLit(chest)`, inside a 120° field;
  - `IsFlameLit` takes a sconce at its base intensity, or a lit brazier, at weight ≥ 0.25.

  Nothing knows about cover: a pillar between a brazier and the player still counts as lit.
- **Walls leak light:**
  - `Wardens::ClampRange` shortens each eye's range to the wall it faces, so the cone doesn't shine
    through it;
  - `--no-range-clamp` turns that off;
  - the clamp also arms the eye once physics exists.
- **Cut-away** (`KeepWorld::UpdateCutaway`, `HideSouthWalls`) hides walls between the camera and the
  player with `Visible = false`, which would let light through them once shadows exist.
- **Flames** are emissive spheres (`FLAME_EMISSIVE` 1.1, `LANTERN_EMISSIVE_MAX` 1.1). Without tone
  mapping they were held near 1.2 so they didn't clip.
- **Captures:** `--freeze` holds the wardens and the oil, but the delta is real, so frozen frames
  drift by up to 445 px run to run (P0). There is no `--perf`, and no frame time for the game was
  measured in P0.
- **The engine has, since P5–P11:**
  - `CastShadows` and `ShadowStrength` on point and spot lights;
  - `ShadowCasting { On, Off, ShadowsOnly }` on meshes;
  - `Scene::GetLightVisibility` and `GetShadowedLightAttenuation`, answering one to three frames late;
  - `PostProcessComponent`;
  - `ParticleEmitterComponent` and `Scene::EmitParticles`/`EmitParticlesAt`;
  - `ParticleEffectParams`, including `StartRotation`.

## 2. Design

### 2.1 Light (W1)

- The camera entity gets a `PostProcessComponent`:
  - `Soft` tone mapping, which leaves everything below 0.8 untouched, so the stone, wax and cloak
    look the same;
  - bloom at the default threshold of 1, so only flames and the lantern glow;
  - AO, added in W4.
- Flames are retuned past 1:

  | Constant | Before | After |
  |---|---|---|
  | `FLAME_EMISSIVE` | 1.1 | 2.6 |
  | `LANTERN_EMISSIVE_MAX` | 1.1 | 2.4 |
  | `LANTERN_EMISSIVE_MIN` | 0.4 | 0.9 |
  | `WARDEN_LAMP_EMISSIVE` | 1.0 | 1.8 |
  | `WARDEN_MARKER_EMISSIVE` | 1.2 | 2.0 |

  The cores roll off under `Soft` instead of clipping, and they bloom.
- Light intensities stay. Under `Soft`, a pool that clipped before (K17) now rolls off.
- `--no-post` drops the component, for A/B captures.

### 2.2 Shadows (W2)

- **Casting lights:** the lantern, every brazier, the warden lamps and eyes. Sconces and candles
  never cast.
- **Shadow slots:**
  - only gameplay lights cast, so at most 14 compete for slots;
  - `Renderer3DCapabilities::MaxShadowedLocalLights = 16` (the engine's most) holds them all, and a
    `static_assert` keeps it so;
  - the light LOD has no shadow slots to budget. (The sketch's "the LOD budgets shadow slots" becomes
    "nothing to budget", stated in the LOD's comment.)
- **What casts:**

  | Mesh | `ShadowCasting` | Why |
  |---|---|---|
  | Walls, caps, floors, brazier brass, flasks, wardens' armour | `On` | |
  | Lantern parts | `Off` | They enclose the lantern's own light |
  | Warden lamp glass and cap | `Off` | They enclose the lamp |
  | Flame cores, wax, debug meshes | `Off` | |
  | The player's body | `Off` | See below |

- **Why the player casts no shadow:**
  - the detection samples and `IsFlameLit` probe points inside the player's body, so a body that
    casts would hide the player from every light;
  - the lantern hangs at the player's side, so a body that casts would also black out half the
    room around them.

  The engine has no per-light or per-probe caster exclusion. That gap is noted for an issue (§7).
  The player still *receives* shadows, so standing in cover visibly darkens them: the readable cue
  that they are hidden.
- **Cut-away:** a hidden wall or cap switches to `ShadowsOnly` instead of `Visible = false`, so it
  still stops light. Mounted sconce parts are still hidden.
- **The range clamp goes:** `ClampRange`, `WARDEN_EYE_RANGE_MARGIN`/`GROWTH` and `--no-range-clamp`
  are removed. The eye keeps its full 8 m, and the eye's own shadow stops it at the wall. The arming
  rule (enable the eye once physics exists, for the sight ray) moves into `Wardens::Update`.
- `--no-shadows` sets every `CastShadows` false, for A/B captures and the perf comparison. The eyes
  then leak through walls, as `--no-range-clamp` used to show.

### 2.3 Hiding (W3)

- **A brazier's light** counts only where it isn't in the brazier's own shadow. `IsFlameLit` uses
  `GetShadowedLightAttenuation` (key 0, the chest) for braziers. Sconces keep the plain weight:
  they don't cast, and they hang on walls.
- **The lantern's beacon** needs the lantern to reach the warden: `GetLightVisibility(lantern,
  warden eye, key = 16 + warden index)` ≥ 0.5, as well as the sight ray. A pillar between the player
  and a warden hides the lantern's glow, and the drawn shadow shows exactly that.
- **The flame beacon** (standing in a brazier's light) keeps its 6 m floor and the sight ray.
- **The cone:** each sample's weight is `GetShadowedLightAttenuation(eye, sample, key = sample)`,
  still against `SEEN_WEIGHT`. The sight ray stays as a guard:
  - probes answer one to three frames late;
  - a light the camera culled answers 1;
  - so a warden off screen still behaves as in v0.8.
- **Feedback:** `--debug-cone` colours a sample red when seen, violet when inside the cone but in
  the eye's shadow, and grey otherwise. The footprint dots stay attenuation plus ray, and say so:
  a grid would need far more than the 256 probes a scene allows.
- **The level:** the Great Hall is already the room for it. Its central 2×2 pillar block and four
  single pillars stand between both braziers (17, 3) and (28, 10) and the warden's loop around the
  centre, so a lit hall becomes long shadow lanes. No map change is planned before the hall has been
  played on the GPU machine. If the lanes don't read, add a pillar at (25, 9) (§6).

### 2.4 Flames and smoke (W4)

| Emitter | Effect | Notes |
|---|---|---|
| Candle | `CandleFlame` | Small additive cone, rate 24, life 0.25–0.4 s, HDR orange to red |
| Sconce | `SconceFlame` | A larger candle flame |
| Lit brazier | `BrazierFlame`, `BrazierEmbers`, `BrazierSmoke` | Flame: rate 70, life 0.5–0.8 s, noise. Embers: rate 6, noise, rising. Smoke: alpha, start rotation 0..360, soft. All start `Playing = false` and play from `Braziers::Light` |
| Kindling | `KindleBurst` | Rate 0. `EmitParticles(brazier kindle entity, 80)` when a brazier lights |
| Lantern | `LanternFlame` | Follows the glass. `Playing` = lit, `RateScale` = the light's strength |
| Snuffing | `SnuffSmoke` | Rate 0. `EmitParticlesAt(lantern smoke entity, glass, 24)` when it snuffs |

- The emissive cores stay as the hot centre: a brazier's core turns from ash to flame when lit.
- A wall cut away stops its sconce's emitter (`Playing = false`); the flames already alive die out
  within 0.4 s.
- The light LOD never touches particles: a flame whose light slot was taken keeps burning.
- **AO:** `AmbientOcclusionSettings` on, at its defaults.
- `--no-particles` skips every emitter. A frozen capture steps particles at the fixed delta
  (§2.5), so it is reproducible.
- **Pool:** about 50 decorative emitters at 64 slots each (3,200), five braziers at about 900 each,
  and the lantern. That is well inside the 65,536-particle pool and the 256 emitters a scene takes.

### 2.5 Captures and perf (W0)

- `--fixed-dt=<s>` steps the scene by a fixed delta, as *Marionette* does. `--freeze` implies 1/60,
  so frozen captures stop drifting: 0 px run to run is the new gate, replacing the 445 px floor.
- `--perf` logs, after a 2 s warm-up:
  - the mean frame, `SceneManager::OnUpdate` and `OnRender` times over 600 frames;
  - the mean of every GPU pass timer, from `Renderer::GetGpuTimers`.
- `--no-post`, `--no-shadows` and `--no-particles` (above) turn each v0.9 feature off on its own.

## 3. Milestones

| | What | Done when |
|---|---|---|
| **W0** | `--fixed-dt` (implied by `--freeze`), `--perf` with GPU timers, the `--no-*` flags parsed | Frozen captures are 0 px run to run; perf lines for all four rooms on the current build, as the "before" |
| **W1** | Post chain and the emissive retune | Before/after per room. Stone, wax and floor within 1/255 where they were below 0.8 |
| **W2** | Casting lights, `ShadowCasting` per mesh, `ShadowsOnly` cut-away, the clamp removed, 16 slots | A capture beside each warden facing a wall: the cone stops at the wall with no clamp. F4 shows no unshadowed lights |
| **W3** | Hiding: shadowed `IsFlameLit`, the lantern beacon by visibility, the cone weighted by the eye's shadow, the `--debug-cone` colours, `--hide-check` | The scripted hide check in §6 passes |
| **W4** | Particle flames, embers, smoke, the kindle burst, the snuff puff, AO | Before/after per room, and perf against W0 |
| **W5** | Review pass in a fresh agent, fixes, docs, flags table | Every Critical/High fixed |

## 4. Decisions (settled 2026-10-08 on the recommended options; the user can revisit)

- **C1** The player casts no shadow (§2.2): it is forced by probing inside the body, and the
  alternative, an engine per-light caster exclusion, is not v0.9 work.
- **C2** The range clamp is deleted, not kept behind a flag. Shadows replace it, and keeping both
  would leave two definitions of the cone.
- **C3** No map change before GPU play (§2.3).
- **C4** All six effects are authored in code, in a new `Flames.h/.cpp`, and tuned in the F4 editor
  (D7). The editor copies the params back as code.

## 5. Risks

- **Shadow cost:** every point light renders six faces of the whole keep. The keep is a few hundred
  boxes, and the shadow pass is instanced per view, so it is cheap on the GPU. `--perf`'s `Shadows`
  timer says how cheap.
- **Lantern acne:** a point light close to the floor at hand height. If acne shows, raise
  `NormalBias` for the game alone (`Renderer3DShadowSettings`).
- **Late probes:** answers come one to three frames late. Stepping out of cover is noticed up to 50 ms
  late at 60 fps, well under `SUSPICION_INVESTIGATE`'s 0.4 s of exposure.
- **Particles over HUD text:** HUD text already draws at `HUD_TEXT_Z`; particles are 3D, under the
  2D overlay.

## 6. Verification (on the GPU machine)

- **Builds:** Candlewick Debug and Release on all three backends.
- **Run to run:** `--room=1..4 --freeze` and `--overview` twice each, 0 px apart (W0's new gate).
- **Before/after:** each room frozen and from above, at W0 (the before), W1, W2 and W4, into the
  visual log.
- **The wall test:** `--room=2 --freeze --debug-cone` and `--room=3 --freeze --debug-cone`. No cone
  light on the far side of any wall.
- **The hide check (W3):** `--room=2 --all-lit --freeze --hide-check` logs `IsFlameLit` and every
  visibility once, after 10 frames.
  - `--spawn=24,5` puts the player behind the pillar at (26, 7) as seen from the brazier at
    (28, 10). A walk along the segment says so, and the pillar at (19, 4) hides them from (17, 3).
    The brazier at (28.5, 10.5) logs visibility 0, and flame-lit "no".
  - `--spawn=28,6`, in the open, logs visibility 1 for that brazier, and flame-lit "yes".
- **Perf:** `--perf --vsync=off` per room, Release, with each `--no-*` flag and with none. Recorded
  in §8 next to W0's line.
- **Engine:** none of this changes the engine. The test app's start-up checks are still owed from
  P1–P13.

## 7. Engine gaps found (issue candidates)

- **No per-light caster exclusion:** a mesh can't skip one light's shadow, say its own lantern, while
  casting the others'. So *Candlewick*'s player casts nothing (C1).
- **Off-screen probes:** a probe of a light the camera culled answers 1 (review S7's family). Off-screen
  wardens fall back to the sight ray.

## 8. As built

*(filled in per milestone)*
