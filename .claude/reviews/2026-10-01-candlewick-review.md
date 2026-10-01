# Code review — P8 *Candlewick* (the v0.7 example game)

**Date:** 2026-10-01 · **Branch:** `claude/v0-7-lighting-planning-cad5a1` at `8c5f8f6` · **Scope:** everything P8
committed, as `git diff 1ac5ee3..8c5f8f6` (70 files, +6385/−57): `examples/Candlewick/**`, the engine helper
(`GetLightAttenuation`, the components' `ToLight`, `LightMath.h`, `Renderer3D::SubmitLocalLight`, `LightSystem`),
the Vulkan `fillModeNonSolid` fix, the Lighting Test checks, the release job and `.vscode` entries, and
`docs/lighting.md`'s "Gameplay queries". Uncommitted working-tree edits (README, ROADMAP, CLAUDE.md, docs, the
`--vsync` flag in `main.cpp`) are out of scope; `main.cpp` line numbers are HEAD's.

**Method:** one fresh read-only reviewer who wrote none of the code. Every file in the range was read, plus the
engine code it leans on (Scene lifecycle, AudioSync, Renderer3D's light selection and batching, the asset path
resolver, the character controller and ray-cast contracts). The plan's §11 "As built" notes are the baseline:
what they record as fixed is not re-reported. Gameplay findings were worked out by hand from the `KeepMap.cpp`
grid, the routes and `GetLightAttenuation`'s formula with the as-built tuning. Each finding is marked
**CONFIRMED** (code path traced end to end) or **PLAUSIBLE** (computed, needs a playtest). Nothing was built or run.

---

## Summary

**No Critical or High findings.** Two Medium, both gameplay: a warden that notices you from less than a metre
away never turns round, so shadowing one from behind is permanently safe and replays the alert sting every 3 s
([B1](#b1)); and the Gallery's checkpoint sits one tile beside both patrol lanes, with the warden reset walking
straight at it, so a respawned player who hesitates is caught again in about 2–2.5 s ([B2](#b2)). Everything else
is Low or Nit.

**The core promise holds on the default path.** Frame order is `Wardens::Update` (position, aim, range clamp) →
`Detection` (the same `SpotLightComponent` through `ToLight` + `GetLightAttenuation`) → `Braziers` → camera →
`LightLod` → render. Nothing writes an eye after Detection, no eye is ever disabled, the respawn frame re-snaps
every range before the LOD counts it, and pause and the win freeze both sides together. The LOD's cull is the
engine's test term for term, it counts every light the engine will see (flames plus registered gameplay lights;
unlit braziers are disabled), and its snap pass holds the in-view total at budget − 2, so at the default budget
(14 gameplay lights exist, capacity 30) no eye or lamp is ever dropped. What still diverges: light through
pillars sideways (documented, LoS keeps detection honest), and a low `--light-budget` ([B5](#b5)).

**Engine side is correct.** `GetLightAttenuation` is `Renderer3D_Lit.glsl:124-134` step for step (the same
`!(d² < r²)` rejection, `max(d², 1e-8)` normalisation, saturate and both squares). `SubmitLocalLight` packs the
same data as before for both kinds (a point light's cone is scale 0, offset 1: the old `(0,0,0,1)` with
`Color.w = 0`). `ToLight` matches `LightSystem`'s old construction, `LightMath.h` stays private, and the public
header gains only a private member template. The Vulkan fix is right as far as it goes ([B4](#b4)).

**Gameplay.** No hard soft-lock: running out of oil can always be undone by being caught ([B3](#b3)). Every
brazier and the altar can be reached (the player's axis gets to 0.71 m of a brazier's centre and 0.95 m of the
altar's; reach is 1.5 m), none is in reach through a wall, and wardens have no collider, so none can block a
doorway. No checkpoint is inside a cone at reset; the Gallery's is reached by one within about 1.5 s ([B2](#b2)).
Difficulty reads as intended. The Gatehouse is free. The Hall's west brazier (17,3) sits outside every leg's seen
footprint. The Gallery's outer rows 4 and 9 are cone-safe from the lanes (a feet weight of at most 0.02 at 2 m to
the side), but the brazier blocks row 4, so passing it means a step into row 5. The altar's north side is
line-of-sight cover from the Chapel loop's near legs. One design note, not a finding: every brazier refills the
lantern to 100, and at 100 oil a lit lantern is noticed out to 11.2 m within 120°, so right after each checkpoint
the beacon is at its widest; in the 26 m Gallery any warden facing you investigates.

**Shipping.** The release job mirrors EchoVault's (artifact name, the Distribution path, `needs: build-windows`,
robocopy of `assets/` including the audio and the font, zip, `gh release upload`). The `.vscode` task and launch
entries match (launch cwd `examples/Candlewick`). There are no asserts, no C++23 features, and `NOMINMAX` is set,
so nothing here breaks the solution build under CI's `v143` toolset. Asset paths are cwd-relative
([B6](#b6)).

### Triage

| # | Finding | Sev | Effort |
|---|---|---|---|
| [B1](#b1) | A warden that notices you within 1 m never turns toward you; touch from behind is safe forever and re-stings every 3 s | Medium | S |
| [B2](#b2) | The Gallery checkpoint is one tile from both lanes and the reset sends warden 2 at it; no respawn grace | Medium | S |
| [B3](#b3) | Out of oil with no flask left, the only way on is to be caught on purpose, and the prompt says "relight" | Low | S |
| [B4](#b4) | `FillMode::Wireframe` on Vulkan without `fillModeNonSolid` is undefined behaviour, not a clean fallback | Low | S |
| [B5](#b5) | The LOD can't protect gameplay lights once they alone exceed its capacity, and nothing warns | Low | S |
| [B6](#b6) | The shipped zip finds its font and audio only when started from its own folder | Low | S |
| [O1](#o1) | The LOD's guarantee rests on a private copy of Renderer3D's cull, with no runtime check | Low | S |
| [T1](#t1) | No automated guard for the map's invariants or for `DroppedLights == 0` | Low | S |
| [B7](#b7) | The first Keep frame is drawn from OnStart state, before physics bakes | Nit | S |
| [B8](#b8) | Esc on the End screen, then Esc on the Title, quits | Nit | S |
| [B9](#b9) | Lighting a skipped brazier later moves the checkpoint backwards | Nit | S |
| [O2](#o2) | `--debug-cone` rebuilds every walking warden's footprint every frame | Nit | S |
| [O3](#o3) | Dead code, a duplicate, narrating comments, inline numbers, a generic `Dingo::PI` | Nit | S |
| [D1](#d1) | The touch channel is undocumented | Nit | S |
| [T2](#t2) | The Lighting Test skips two promises of "Gameplay queries" | Nit | S |

---

## Medium

### B1 — A warden that notices you within 1 m never turns toward you {#b1}
**Medium · S · CONFIRMED** · `examples/Candlewick/src/Wardens.cpp:296-315` (Looking), `:287` (re-plan), `:274-279`
(alert sting); `Detection.cpp:194-196` (touch)

When an investigation starts with its goal tile's centre within `WARDEN_INVESTIGATE_STOP` (1 m), `close` is true
on the first frame. The warden goes straight to Looking without a step or a turn, and `LookYaw = warden.Yaw` centres
the ±60° sweep on the heading it already had, so the cone covers at most about 84° either side of it. The touch
channel (`TOUCH_DISTANCE` 0.8 m, there because wardens have no collider) produces exactly this case: the player's
tile centre is usually within a metre of the warden.

**Scenario:** walk up behind a patrolling warden and stay within 0.8 m. Touch holds suspicion at 0.6: the marker
turns suspicious, the alert sting plays, and the warden stops and sweeps the way it was walking. The player
behind it never enters the cone, the beacon needs the 120° front field, and decay is clamped back up to 0.6, so
the player is never caught. Because the same tile is requested every frame, the warden enters Return after
`WARDEN_LOOK_TIME`. The next frame's request then finds `Mode != Investigate`, starts a new investigation and plays
the sting again: one sting every 3 s for as long as the player stays. The same repeat happens whenever any channel
keeps perceiving the player on one tile for more than 3 s without a catch (the beacon is capped at alert).

**Fix:** have the request carry the sighting's world position, and on entering Looking turn to face it
(`LookYaw = YawOf(sighting − Feet)` when that offset is not tiny) before sweeping. While Looking, a fresh request
for the current target should reset `LookTime` instead of letting the 3 s run out. Play the sting only on
Patrol → Investigate, or rate-limit it.

### B2 — The Gallery checkpoint is one tile from both patrol lanes, and the reset sends warden 2 at it {#b2}
**Medium · S · PLAUSIBLE (computed, not played)** · `KeepMap.cpp:59`, `:253-262` (`FindCheckpointTile`);
`Braziers.cpp:153-154`; `KeepDirector.cpp:205-212` (respawn); `Wardens.cpp:210-231` (`Reset`); route `KeepMap.cpp:52`

`FindCheckpointTile` takes the first patrol-floor neighbour, south first. The Gallery brazier (47,4) therefore
checkpoints at (47,5), one tile beside lane row 6 and two from row 7. `Reset` puts warden 2 (Gallery A) at (38,6)
facing east along row 6, 9 m away. With the as-built eye (1.7 m high, 25° down, range 8 down that open lane) and
`SEEN_WEIGHT` 0.1, the feet sample at (47.5, 5.5) reaches 0.1 when A's eye is 6.3 m short of it. That happens
about 1.5 s after the respawn, 1 s after the 0.5 s fade-in clears, and the catch follows about 1.1 s later.

A player caught with the lantern lit respawns lit at 100 oil, so the notice range is 11.2 m. A notices them at
once, investigates at 0.8 s, runs at 2.4 m/s, and the catch comes at about 2 s. Detection runs from the first
frame after the teleport, and there is no grace period. A hesitant player is caught again and again in the room
where most catches happen.

The other checkpoints are fine. The Gatehouse has no warden. The Hall's (17,4) is behind the reset warden. The
Hall's (28,11) is about 4.5 s from the column-27 leg. The `--room` spawns are clear.

**Fix (any one, ideally the first two):**
- Choose the neighbour farthest from the room's routes. (48,4) or (46,4), on the brazier's own row, is 2 m from
  the lane and cone-safe.
- Give a short respawn grace: no suspicion gain until the fade-in ends plus about 1 s.
- Reset each warden to the loop point farthest from the checkpoint.

[T1](#t1)'s map check would catch a future case.

---

## Low

### B3 — Out of oil with no flask left, the only way on is to be caught on purpose {#b3}
**Low · S · CONFIRMED** · `Braziers.cpp:97`, `:120-126`; `Hud.cpp:194-201`; `KeepWorld.cpp:446-467`; `KeepDirector.cpp:98-106`

Lighting a brazier needs a lit lantern. Lit braziers never refuel (`m_Lit` skips them), collected flasks stay
gone after a respawn, and the pause offers only "title". A player who has used up a room's flasks and lets the
lantern burn out (100 s lit) can progress only by walking into a cone, so that the respawn restores the
checkpoint's oil. The game never says so. At a brazier the prompt reads "Relight your lantern first"
(`Hud.cpp:201`) even when `IsOutOfOil()` or `IsTooLowToStrike()` makes relighting impossible. Under `--freeze`
catches are off, so it is a true soft-lock there (debug only).

**Fix:** let a lit brazier refill the lantern in reach (the same hold), or add "Restart from checkpoint" to the
pause. Show "Out of oil — find a flask" instead of the relight prompt when a strike is impossible.

### B4 — `FillMode::Wireframe` on Vulkan without `fillModeNonSolid` is undefined behaviour {#b4}
**Low · S · code CONFIRMED, driver effect PLAUSIBLE** · `src/DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.cpp:533`;
`Material.cpp:172`; `NvrhiPipeline.cpp:26`

`764a09c` enables the feature only where the device reports it, which is right, but nothing downstream knows. A
wireframe material still builds a pipeline with `VK_POLYGON_MODE_LINE`, which
VUID-VkPipelineRasterizationStateCreateInfo-polygonMode-01507 forbids without the feature. With validation that
is an error per pipeline; without it the driver decides (filled, lines, or a crash). Every desktop GPU that passes
the engine's unconditional features (geometry and tessellation shaders, BC compression) reports it, so in
practice only `--debug-cone` on unusual hardware is exposed.

**Fix:** record the feature on the context (say `GraphicsContext::SupportsWireframe()`), and have
`NvrhiPipeline` fall back to `Solid` with a one-time warning when it is missing. D3D11 and D3D12 need nothing.

### B5 — The light LOD can't protect gameplay lights once they alone exceed its capacity {#b5}
**Low · S · CONFIRMED** · `examples/Candlewick/src/LightLod.cpp:133-136`, `:214-226`; `main.cpp:41-57`

`slots = max(0, capacity − gameplay.Planned)`, and the snap pass zeroes only flames. When the gameplay lights in
view exceed `budget − 2`, the engine drops lights by its own ranking, and the dropped ones can include a warden's
eye. The drawn cone then vanishes while Detection still tests it, and nothing is logged by the game. The default
budget can't reach this, but `--light-budget` accepts 1–32. Example: `--light-budget=6` in the Gallery, with
both wardens, the lantern and the lit brazier, is 6 gameplay lights against a capacity of 4.

**Fix:** warn once when `gameplay.Exact > capacity` ("the drawn and tested cones may differ"). Also raise the
flag's lower bound to the game's peak gameplay load plus the headroom, or say what it is in the flag's warning.

### B6 — The shipped zip finds its font and audio only when started from its own folder {#b6}
**Low · S · CONFIRMED (shared with EchoVault)** · `Audio.cpp:21-34`; `Overlay.cpp:8`; `main.cpp:61-75` (no `.Assets`);
`include/DingoEngine/Asset/AssetManager.h:30`

Paths are `assets/...` under the default root `"assets"`, and that root is resolved against the cwd at start-up.
`ResolveRawAssetPath` tries `<cwd>/assets/assets/...`, misses, and falls back to the cwd-relative path. Launched
from Explorer or a default shortcut it works. Launched from a terminal in the parent folder
(`.\candlewick\Candlewick.exe`), from a launcher, or from a shortcut with another "Start in", it gets no font and
no audio: the Title, HUD and End screens are blank because a null font's text is skipped. Distribution is a
`WindowedApp`, so the only error goes to a log the player never sees.

**Fix:** `params.Assets.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets"))` (the usage
`Platform.h:19` recommends), with paths relative to it (`audio/footstep.wav`, `fonts/arialbd.ttf`). Worth doing
for every example at once.

### O1 — The LOD's guarantee rests on a private copy of Renderer3D's cull, with no runtime check {#o1}
**Low · S · CONFIRMED** · `LightLod.cpp:20-43` vs `src/DingoEngine/Graphics/Renderer3D.cpp:47-69`

Every term matches today: the same Gribb–Hartmann planes for [0, 1] depth, the same normalisation, culled when
`dot + w < −radius`, the same view-projection (`Scene::GetCameraViewProjection` with Renderer2D's viewport aspect,
as `SceneRenderer` does). But the cull is engine-internal. A guard band, culling spots by their cone, or a
different matrix source would silently void "DroppedLights = 0", and with it the drawn-is-tested promise for every
warden.

**Fix:** read the previous frame's `Renderer3D::GetStatistics().DroppedLights` in the director and warn once when
it is non-zero with the LOD on ([T1](#t1)). Longer term, the light-priority item in plan §10 removes the copy.

### T1 — No automated guard for the map's invariants or for `DroppedLights == 0` {#t1}
**Low · S · CONFIRMED** · `KeepMap.cpp:155-170` (`ValidateRoutes`); `LightLod.cpp`

Only the routes are validated at start-up. Nothing checks these, all of which the game depends on:
- every brazier and the altar have a walkable neighbour reachable from spawn 1;
- every checkpoint tile is reachable;
- no checkpoint lies within a tile of a patrol lane ([B2](#b2));
- at runtime, `DroppedLights` stays 0 with the LOD on ([O1](#o1)).

Each is cheap (a BFS over 1,106 tiles, one statistics read), and the first three would have flagged B2 at load.

**Fix:** grow `ValidateRoutes` into a `ValidateMap` that logs `DE_ERROR` like the route check does, and add
O1's warn-once.

---

## Nit

### B7 — The first Keep frame is drawn from OnStart state, before physics bakes {#b7}
**Nit · S · CONFIRMED** · `Wardens.cpp:108`, `:228`, `:530-545`; `Lantern.cpp:79`; `KeepDirector.cpp:79`;
`src/DingoEngine/Scene/Scene.cpp:381-382`

`Scene::OnStart` runs scripts before `OnPhysicsStart`, so the constructors' rays (the range-clamp snap, the
lantern's hand) see no bodies. Every eye starts at the full 8 m, the lantern takes the side offset whatever the
walls are, and the LOD is primed with those ranges. SceneManager renders that state on the transition frame,
before any `OnUpdate`. With `--room=2..4` a nearby warden's cone shines through a wall for one frame; the first
`Wardens::Update` shrinks it at once. Detection doesn't run on that frame, so gameplay is unaffected.

**Fix:** skip the ray-dependent setup in the constructors and snap on the first update, or accept it.

### B8 — Esc on the End screen, then Esc on the Title, quits {#b8}
**Nit · S · CONFIRMED** · `TitleScreen.cpp:92-93`, `:64-65`

On the End screen Esc returns to the Title, and on the Title Esc calls `Application::Close`. A player tapping Esc
twice to back out closes the game. **Fix:** make the End screen confirm-only, or have the Title ask for a second
Esc.

### B9 — Lighting a skipped brazier later moves the checkpoint backwards {#b9}
**Nit · S · CONFIRMED** · `Braziers.cpp:153-154`

The checkpoint is simply the last brazier lit. Light the Hall's west brazier, then the Gallery's, then go back for
the Hall's south-east one, and the next catch respawns in the Hall. **Fix:** move the checkpoint only forward (by
room), or keep the furthest.

### O2 — `--debug-cone` rebuilds every walking warden's footprint every frame {#o2}
**Nit · S · CONFIRMED** · `Detection.cpp:273`, `:298-329`

The `EyeState` cache hits only for a still warden. A walking one costs a 64 × 64 grid of `GetLightAttenuation`
calls, a line-of-sight ray for each passing point (a few hundred), and a new `Mesh` every frame. It is debug only
and fine for captures. **Fix if it matters:** rebuild only when the eye has moved more than half a dot spacing,
or every few frames.

### O3 — Dead code, a duplicate, narrating comments, inline numbers {#o3}
**Nit · S · CONFIRMED**

- Unused or write-only: `KeepWorld::GetFlaskSpots` (`KeepWorld.h:55`), `Player::GetEntity` (`Player.h:23`),
  `DecorFlame::Room` (`KeepWorld.h:21`), `FlaskSpot::Tile` and `FlaskSpot::Room` (`KeepWorld.h:36-37`).
- `Wardens.cpp:389-394` repeats `Reset`'s placement and would teleport the warden. It is unreachable today,
  because every path keeps wardens on patrol floor.
- `CandlewickLayer.cpp:23-24`: two trailing comments that narrate (copied from EchoVault).
- `TitleScreen.cpp:52-56`, `:82-85`: sizes and positions are inline, while their siblings (`TITLE_CONTROLS_*`,
  `END_*`) live in `GameTuning.h`.
- `GameMath.h:9` defines `Dingo::PI`, a name the engine could plausibly add itself; `glm::pi<float>()` already
  exists.

### D1 — The touch channel is undocumented {#d1}
**Nit · S · CONFIRMED** · `Detection.h:21-24`; `GameTuning.h:141-142`; plan §11 C4

The class comment names the cone and the beacon ("Only the cone can take suspicion to 1"), and the plan's as-built
rates list the cone, the beacon and decay. Neither mentions that touching a warden (closer than 0.8 m) holds
suspicion at 0.6 or more. The statement stays true, since touch is capped below the catch, but it hides the
channel [B1](#b1) is about. **Fix:** one clause in each.

### T2 — The Lighting Test skips two promises of "Gameplay queries" {#t2}
**Nit · S · CONFIRMED** · `test/src/Tests/Renderer/LightingTest.cpp:287-379`; `docs/lighting.md:206-224`

The docs say the helper shares the renderer's angle clamps, and that an infinite `Range` gives a falloff of 1
everywhere. Neither is checked: no case has an outer angle below 1° or above 179°, an inner angle above the outer,
or `Range = INFINITY`. Both hold in the code today. **Fix:** three more checks in the same step.

---

## Fix status {#fix-status}

Plan §5's bar for C6 ("no open Critical/High") was met as written. C6 then fixed every finding that is
the example's to fix, in one uncommitted change set (code and docs). Candlewick (Debug and Distribution) and
the test app (Debug) build with no new warnings; nothing has been run since, so the fixes are not yet
verified in play.

| Finding | Severity | Status |
|---|---|---|
| [B1](#b1) | Medium | fixed (C6). `Wardens.cpp` (`Think`, `BeginLook`, `FaceSighting`): a sighting is now the player's position, the warden turns to face it before the sweep and centres the sweep on that bearing, a repeat sighting while looking resets the look timer, and the alert sting plays only on Patrol → Investigate. `Detection.cpp` passes the position |
| [B2](#b2) | Medium | fixed (C6). `KeepMap.cpp` `FindCheckpointTile` takes the brazier neighbour that no route-start cone covers and that is farthest from every patrol lane (worked by hand, the Gallery's is now (48,4)); `KeepDirector.cpp` skips detection for `RESPAWN_GRACE_TIME` (2.5 s) after a respawn |
| [B3](#b3) | Low | fixed (C6). `Braziers.cpp`: the same hold at any lit brazier calls `Lantern::Refill` (full oil, a snuffed lantern burns again, no checkpoint change) and toasts "Lantern refilled"; `Hud.cpp` says "Out of oil - find a flask or a lit brazier" when a strike is impossible |
| [B4](#b4) | Low | filed as K19 in `KNOWN-BUGS.md`: it is an engine fix (expose the capability, fall back to solid), not the example's |
| [B5](#b5) | Low | fixed (C6). `LightLod.cpp` warns once when the gameplay lights in view exceed its capacity; `main.cpp` rejects `--light-budget` below `LIGHT_BUDGET_MIN` = 16 (`GameTuning.h`: 14 gameplay lights plus the headroom of 2) |
| [B6](#b6) | Low | fixed (C6) for Candlewick. `main.cpp` sets the asset root to `Platform::FindDirectoryUpward("assets").value_or("assets")`, and `Audio.cpp` and `Overlay.cpp` use root-relative paths (`audio/…`, `fonts/arialbd.ttf`). The other examples are untouched |
| [O1](#o1) | Low | fixed (C6). `CandlewickLayer.cpp` `CheckDroppedLights` warns once if `Renderer3D`'s `DroppedLights` is non-zero in the Keep with the LOD on |
| [T1](#t1) | Low | fixed (C6). `KeepMap.cpp` `ValidateMap` (`DE_WARN`): every brazier, the altar and every brazier's checkpoint tile must be reachable from spawn 1, and no checkpoint may lie on a patrol lane or inside a route-start cone. The `DroppedLights` half is O1's check |
| [B7](#b7) | Nit | fixed (C6). `Wardens.cpp`: each eye starts disabled and `ClampRange` enables it at its first clamp with physics running |
| [B8](#b8) | Nit | open (by design): Esc steps back one screen at a time and quits from the Title; a double tap is a quit, as in most games |
| [B9](#b9) | Nit | open (by design): the checkpoint is the last brazier lit, which is predictable; relighting out of order is the player's choice |
| [O2](#o2) | Nit | open (debug-only): `--debug-cone` costs a rebuild per walking warden per frame, and only captures and debugging use it |
| [O3](#o3) | Nit | fixed (C6). Dead accessors and fields removed, `Wardens::BeginReturn` uses `ResetWarden` instead of a copy of `Reset`, the two comments removed, the Title and End numbers moved to `GameTuning.h`, and `PI`, `WrapAngle` and `ApproachAngle` now live in `Dingo::GameMath` |
| [D1](#d1) | Nit | fixed (C6). `Detection.h`'s class comment and the plan's C4 rates now state the touch channel |
| [T2](#t2) | Nit | fixed (C6). `LightingTest.cpp`: one check for the angle clamps (an outer angle of 200 acts as 179, of 0.2 as 1, an inner past the outer as equal to it) and one for an infinite `Range` |

---

## Checked and found correct

- **Cross-phase timing.**
  - A catch in the same frame as the altar's last hold wins over the win: Detection runs first, then `canLight`
    is false.
  - Pause blocks the win; the win blocks the pause toggle.
  - A strike during the caught fade can't unlock movement, because the director re-locks after the Lantern.
  - A hold survives a pause only while E stays down.
  - The respawn resets suspicion, requests, paths, markers and eye ranges, and snaps the camera before the LOD
    counts.
- **Keep rebuild** (Esc→Title→Keep and End→Title→Keep).
  - The layer clears the Keep the frame it is left, before any render.
  - Subsystems free their materials and meshes in reverse order of creation, and each holds references only to
    longer-lived ones.
  - `Scene::OnStop` stops the crackle sources; `~GameAudio` stops the drone and restores a master volume left
    muted by the pause.
  - The LOD and its registrations are rebuilt each run.
  - Renderer3D's batches hold no material-derived state, so a reused material address is harmless.
  - The only static state is the launch options.
- **Engine.**
  - The helper equals the shader term, and `SubmitLocalLight` preserves both kinds' packed data.
  - `ToLight` uses designated initializers in declaration order.
  - The point-light cone is the old neutral value.
  - `docs/lighting.md`'s "Gameplay queries" and the `Light.h` and `Components.h` comments match the code, and
    the doc's sample compiles.
- **Light LOD.**
  - The plane extraction, normalisation and sphere test equal `Renderer3D.cpp:49-69`.
  - The view-projection comes from the same aspect source as `SceneRenderer`.
  - Every enabled point or spot light in the Keep is either a flame or a registered gameplay light.
  - Flicker never takes an intensity to 0, so the counts agree with what the engine accepts.
- **Detection.**
  - Line of sight can't be fooled by `SIGHT_SLACK`: the capsule keeps every wall at least 0.3 m from the sample.
  - The range-clamp ray at 1.7 m clears every brazier and the altar.
  - No ray starts inside a body.
  - Candles, flasks and decor have no colliders.
- **Map.** Every route stays on patrol floor in its room. The two Gallery wardens never meet head-on in patrol
  (both go east on row 6 and west on row 7). Lower-index right of way can't deadlock.
- **Input.** The left stick's +Y is down, which maps to +Z (south) as on the keys. Every edge-triggered key that
  two scenes share (Enter, Esc, Start, A) fires in only one scene per press.
- **No per-frame allocations** outside the debug view and re-plans, which happen only when the sighted tile
  changes.
- **Every `GameTuning.h` constant** is used.
- **Shipping.** The release job, the root `premake5.lua` include, `copyAssimpRuntime()` and the `.vscode`
  entries are all present. The assets are committed outside LFS and detected as binary.
