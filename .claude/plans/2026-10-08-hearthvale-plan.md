# Hearthvale — the v1.0 release game: plan

Drafted 2026-10-08 on `claude/gallant-albattani-0gyirn` (base `master` @ `4ff48ea`, v0.9.0 plus the
Linux merge). Every file:line below was read that day. This is P13 of
`.claude/plans/2026-10-08-v1.0-plan.md` (its §6 and decision D1). *Hearthvale* is a working title;
HD9 settles the name.

**Status**: draft, no code. HD1–HD10 in §9 were settled by the owner on 2026-10-08, each as
recommended. The asset choices also depend on downloads that could not be
made from the planning machine (§2.7).

**What it is**: a cozy 3D builder/farm sim. You inherit an overgrown valley, then clear it, fence it,
plant it and build it up, day by day through a year of seasons, until the old village festival can be
held again. It is DingoEngine's **1.0 release game**, published on itch.io (Steam as a stretch goal),
and it lives in `examples/Hearthvale` beside the other examples.

**Why a builder** (v1.0 plan §6): it leans on every part of v1.0 at once:

| v1.0 work | How *Hearthvale* uses it |
|---|---|
| Culling and the arena (P2) | One valley holds thousands of objects, and the camera sees one corner of it |
| Per-object data (P3) | Ripe crops tinted, lit windows and lanterns glowing, all on the default material |
| Instancing (P4) | Every crop at each growth stage, every fence post, tree, rock and grass tuft |
| Static geometry (P5) | Everything placed is `Static`, and placing or removing rebakes one cell, many times a day |
| Skinned throughput (P6) | The farmer, the villagers and the animals, with animation LOD off screen |
| Stability (P7) | Crash logs and dumps in the user-data folder of a game players install |
| DingoUI (workstream U) | Every menu and the HUD, pad first |
| SaveData, buses, display, fog, translucency (P8) | The whole valley in a save, music and ambience sliders, morning mist, greenhouse glass, the pond |
| `OnFixedUpdate`, body lookup (P9) | Deterministic day simulation for `--check`; picking a villager by ray |
| v0.9 | A moving sun with cascades, shadowed lamps, bloom on windows, rain and firefly particles, AO |

---

## 1. Where we start (verified facts)

### 1.1 What the engine gives a farm game today

| Area | Today | Anchor |
|---|---|---|
| Picking | `Scene::ScreenPointToRay(screenPos, viewportSize)` (top-left origin, primary perspective camera); `Ray::IntersectGroundPlane(height, outHit)` (false when parallel or behind) | `Scene.h:160`, `Core/Ray.h:24,40,53` |
| Physics casts | `Physics3D::RayCast(ray, maxDistance, outHit)`, `ShapeCastSphere`, `OverlapSphere`. No body-to-entity lookup (v1.0 P9 adds it) | `Physics3D.h:120-131`, `PhysicsTypes3D.h:77-83` |
| Character | `CharacterController3DComponent { Radius 0.3, Height 1.8, StepHeight 0.3, MaxSlopeAngle 45 }`. The game applies gravity, sets velocity and fetches the controller every frame; `Scene::OnUpdate` steps it | `Components.h:574-585`, `CharacterController3D.h:38-95` |
| Movement pattern | Candlewick's `Player::Update`: WASD, D-pad and stick on world axes, gravity by hand, yaw toward the move direction | `examples/Candlewick/src/Player.cpp:93-135` |
| Follow camera | Candlewick's `CameraRig::Update`: a lagged focus plus a fixed view direction | `examples/Candlewick/src/CameraRig.cpp:110-139` |
| Sun | `DirectionalLightComponent { Direction, Color, Intensity, Ambient 0.35, CastShadows, ShadowStrength }`; `Ambient = 0` plus an `AmbientLightComponent` is the clean setup (Marionette's moon) | `Components.h:179-208`, `examples/Marionette/src/ArenaWorld.cpp:86-94` |
| Cascades and a moving sun | Cascades are sphere-fit and texel-snapped, so a moving *camera* doesn't shimmer. Nothing covers a moving *light*. The light view switches its up vector when `abs(dir.y) > 0.99` | `docs/shadows.md:162-165`, `Renderer3D.cpp:1388,1409-1410` |
| Light budget | 32 local lights, 16 shadowed at most; *Candlewick*'s `LightLod` keeps in-view lights under the budget so none pops | `Renderer3D.h:55,248-259`, `examples/Candlewick/src/LightLod.h:14-17` |
| Text and HUD | `TextComponent`, `Renderer2D::DrawText` (no word wrap), `Font::GetStringWidth`. *Marionette* builds its HUD from sprite quads and text entities on an ortho camera, its screens as `ScriptableEntity` controllers | `Components.h:104-114`, `Renderer2D.h:40-56,106-107`, `examples/Marionette/src/Overlay.cpp:27-48`, `Screens.cpp:19-98`, `Hud.cpp:41-66` |
| Audio | `AudioEngine::Play(clip, SoundPlayParams{ Looping, Spatialized, Position, ... })`, `AudioSourceComponent`; the examples' sounds are synthesized by `scripts/generate_audio.ps1`, so no licences | `examples/Candlewick/src/Audio.cpp:83-106`, `Components.h:593-608` |
| Particles | `ParticleEffectParams` (shape, rate, bursts, lifetime, gravity, drag, noise, size, spin, 4 colour keys, blend, flipbook, soft distance); *Candlewick*'s `Flames.h` owns its effects and spawns emitters | `Graphics/Particles.h:44-120`, `examples/Candlewick/src/Flames.h:12-26` |
| Fixed step | No engine helper (v1.0 P9 adds `OnFixedUpdate`). *Marionette*'s `--fixed-dt` steps the `SceneManager` `StepsPerFrame` times with a fixed delta | `Core/Timer.h:7-18`, `examples/Marionette/src/MarionetteLayer.cpp:153-170` |
| Checks | `CheckReport.h`: `Check(bool, name)` logs `[PASS]`/`[FAIL]` and counts; suites log a summary; the exit code is unchanged; CI fails on any `[FAIL]` | `examples/Marionette/src/CheckReport.h:8-31`, `scripts/ci/linux-smoke-test.sh:50-54` |
| Launch options | A struct plus `ParseInt`/`ParseSeconds`/`ParseFlag` over `ApplicationCommandLineArgs`, cached once; scripted runs set `UpdateInBackground` | `examples/Marionette/src/LaunchOptions.cpp:16-239`, `main.cpp:62,86` |

### 1.2 What models bring with them

- **The loader reads only diffuse textures.** `MeshVertex` is position, normal and uv
  (`Mesh.h:12-17`). `AssignMaterial` creates a plain `Material` and sets slot 0 from the diffuse
  texture path (`Model.cpp:160-172`). Embedded textures (`*0` paths) are skipped (`Model.cpp:91-100`,
  #99). **Vertex colours and material base colours are dropped**, so a pack coloured that way loads
  white. Engine gap **E1** (§10).
- **What works today**: a model whose material points at an **external PNG**, which is how KayKit
  ships its gradient atlases and how Kenney's kits reference `Textures/colormap.png`. *Marionette*
  makes one lit material per PNG and puts the texture in slot 0 (`examples/Marionette/src/GameAssets.cpp:42-60`).
- **Characters and clips**: KayKit `Rig_Medium` characters (23 joints) and the five KayKit clip
  libraries already load, retarget by joint name, and play in *Marionette*
  (`2026-10-03-marionette-plan.md:160-174`).

### 1.3 Scale reference

- *Candlewick*: about 330 meshes, 15 draws, about 33k indices; Release 1.8–2.1 ms, Debug 13–14 ms
  (`2026-10-01-candlewick-plan.md:559-562`).
- A flat 10k-mesh scene: 4.35 ms in Release today (v1.0 plan §1.4). *Hearthvale*'s target is about
  10k placed and decorative objects, which only v1.0's P2–P5 make affordable.

---

## 2. The game

### 2.1 The loop

**A day** runs from 6:00 to 2:00, 12 real minutes at normal speed (HD5). Time stops in every menu
and dialogue. Over a day you:

1. **Tend the farm**: till, plant, water and harvest crops.
2. **Gather and clear**: chop trees and stumps for wood, break rocks for stone, scythe weeds.
3. **Build** fences, paths, gates, sprinklers, lamps, a coop, a barn and a greenhouse.
4. **Sell** through the shipping bin, which pays overnight, and buy seeds and plans at the shop.
5. **Meet villagers**: talk and give gifts; their requests fill the community board.
6. **Sleep**, which ends the day, grows the crops and autosaves.

Staying up past 2:00 makes you pass out where you stand and lose 10% of your coins, the genre's
usual nudge.

**The year**: four seasons of 14 days each. Each season brings its own crops and weather and one
board of villager requests. Clearing all four boards before the end of winter holds the **Lantern
Festival** in the square, which is the credits and the ending (HD6). Play goes on after it as a
sandbox, with the boards repeating harder requests.

**No combat, no stamina** (HD8). The clock is the only limit.

### 2.2 Content target (1.0)

| Kind | Count | Notes |
|---|---|---|
| Crops | 12 (3 a season) | Growth stages: seed, sprout, young, ripe (4 models or 2 models scaled); regrowing crops (2) go back to young after a harvest |
| Buildables | 20 | Wood fence, stone fence, gate, dirt path, stone path, scarecrow, sprinkler (waters 3 × 3 at dawn), quality sprinkler (5 × 5), chest, lamp post, bench, flower bed, well, beehive, coop, barn, greenhouse, mailbox, sign, festival lantern |
| Tools | 5 | Hoe, watering can (refilled at the well or the pond), axe, pickaxe, scythe; no upgrades in 1.0 |
| Resources | 4 | Wood, stone, fibre (weeds), coins |
| Animal products | 2 | Eggs (chickens, coop), milk (cows, barn), if HD3 keeps animals |
| Villagers | 4 | The shopkeeper, the carpenter (building plans), the mayor (the board), a child; each with a daily schedule, 10+ lines of dialogue and liked gifts |
| Weather | 3 | Clear, rain (waters everything, rain particles, heavier mist, dimmer sun), snow in winter (no crops outside the greenhouse) |
| Board requests | 4 boards × 4 | "Ship 20 parsnips", "Build 3 lamp posts in the square", "Give the child a sunflower"... |
| Music | 5 tracks | One per season plus the festival; night is ambience only |

The valley is a 96 × 96 tile map (1 m tiles). The farm starts as a cleared 12 × 12 plot inside a
40 × 40 overgrown area. The village square, shop, carpenter, mayor's house and pond sit to the east,
and wooded hills ring the edge as decoration you can't build on. Expected live counts, used as the
throughput target: 4,000 decoration objects (trees, rocks, grass, flowers), up to 1,500 crops, up to
1,000 placed buildables, plus 1,600 ground tiles merged into static cells.

### 2.3 Controls (pad first)

| Action | Pad | Keyboard and mouse |
|---|---|---|
| Move | Left stick | WASD |
| Use tool / place | A | Left click, or Space |
| Interact (talk, open, harvest) | X | Right click, or E |
| Previous / next hotbar slot | LB / RB | Wheel or 1–9 |
| Rotate the placement | Y | R |
| Camera rotate 90° | D-pad left/right | Q / E |
| Camera zoom | Right stick up/down | Ctrl + wheel |
| Inventory and build menu | View | Tab / B |
| Pause | Menu | Esc |

The target tile is the one in front of the farmer, shown as an outline. With a mouse, the hovered
tile within 3 tiles of the farmer is the target instead (`ScreenPointToRay` plus
`IntersectGroundPlane(0)`), and the farmer turns to it.

### 2.4 Screens (all DingoUI, v1.0 workstream U)

**Title** (Continue, New game, Load, Options, Credits, Quit), **save slots** (3, each showing the
day, season, coins and farm name), **HUD** (clock, day and season, weather icon, coins, hotbar with
item counts), **inventory** (a 6 × 4 grid), **build palette** (a grid of buildables with their cost;
greyed when unaffordable), **shop**, **shipping bin**, **dialogue** (portrait, word-wrapped text,
gift choice), **community board**, **pause**, **options** (master, music, sfx, ambience and UI
volume; VSync; fullscreen; display mode; camera speed; hold-to-place), **credits** (every asset's
author and licence). Every screen is fully usable with a pad, and a `--check` suite proves it (§5).

### 2.5 The look

- A **day/night sun**: elevation from 10° at 6:00 to 65° at noon to below the horizon by 20:00,
  colour and intensity from a 6-key curve, cascaded shadows with `ShadowStrength` 0.85, and ambient
  from a matching curve.
- **Night**: a moon (a second directional light, no shadows); lamp posts and lit windows are point
  lights under a *Candlewick*-style light LOD; the moon brightens windows through surface emissive
  overrides; fireflies.
- **Post**: `Soft` tone mapping, bloom (0.3) for windows and lanterns, AO.
- **Fog**: morning mist from 6:00 to 9:00, densest on rain days.
- **Translucency**: the greenhouse glass and the pond surface.
- **Particles**: watering spray, rain, snow, chimney smoke, a harvest puff, a build dust burst, wood
  chips from the axe, fireflies, and festival lanterns and sparks.

### 2.6 Sound

The buses are the engine's defaults: Master, Music, Sfx, Ui and Ambience. Each season has its own
music track, which crossfades at dawn and stops at 20:00. Ambience follows the time of day (birds by
day, crickets and wind at night, rain) as looping, non-spatial layers. Tool sounds, footsteps on
grass, dirt and path, and the pond and chickens are positional. The UI has click, focus, open/close,
coin and error sounds.

### 2.7 Assets (HD1, HD2, HD3, HD4)

The planning machine's network policy blocked kenney.nl, quaternius.com, itch.io, opengameart.org
and freesound.org. What was **verified** came from official GitHub repos and from licence files
mirrored on GitHub; everything else is marked unverified and is checked in H0.

| Need | Candidate | Licence | Status |
|---|---|---|---|
| Crops (stages), fences, trees, rocks, grass, paths | **Kenney Nature Kit** | CC0 | Kit contents and licence **unverified** here. The Kenney pattern of an external `colormap.png` that this engine loads is verified from Kenney's own MIT GitHub starter kits |
| Houses, shop, well, market props | **Kenney Fantasy Town Kit** (modular, square footprint) | CC0 | Unverified |
| Tools, chests, barrels, crates | **Kenney Survival Kit** | CC0 | Unverified |
| Alternative world set | KayKit Medieval Hexagon Pack (221 glTFs: windmill, well, market, homes, fences, trees, rocks; one external atlas) | CC0, verified (GitHub `KayKit-Game-Assets`) | Verified, but **hex footprints**, and no crops |
| Interiors and produce props | KayKit Furniture Bits (53), Restaurant Bits (144) | CC0, verified | Verified |
| Farmer and villagers | **KayKit Adventurers** (4 characters on `Rig_Medium`) plus the KayKit **Character Animations** libraries (Idle, Interact, PickUp, Use_Item, Throw, walks and runs) | CC0, verified; already in `examples/Marionette/assets` | Verified and working in the engine |
| Farm animals | Quaternius Farm Animals (cow and horse fully animated; pig and sheep idle and jump only) | CC0 in the mirrored `License.txt`, but *Marionette*'s plan recorded a non-CC0 "Quaternius Asset License v1.0" on quaternius.com in 2026-08 | **Material colours only** (needs E1); **licence contested** |
| UI sounds | Kenney Interface Sounds (100), Kenney UI Audio (50) | CC0, verified (Kenney's `License.txt` in GitHub mirrors) | Verified |
| UI art, icons | Kenney UI Pack, Fantasy UI Borders (nine-slice), Game Icons | CC0 | Unverified |
| Music | OpenGameArt CC0 tracks: *Sunset Plains* (Yoiyami), *Forest Charm* (Melox6), *Cozy Puzzle In-Game 1* (MintoDog), *Calm Loop* (wipics) | CC0 per search results | **Unverified** |
| Ambience | *Crickets Ambient Noise* (Wolfgang_), *Forest Ambience* (TinyWorlds); *Cozy Farm SFX* (roberto mellado) has every loop needed but is **CC-BY 4.0** | per search results | Unverified |
| Body font | **Inter 4.1**, static `Inter-Regular.ttf`/`Inter-SemiBold.ttf` from the release zip's `extras/ttf` | OFL 1.1, verified | Covers Latin-1 except the invisible U+00AD; General Punctuation 63 of the 71 the atlas bakes (missing only U+2056, U+2058–205E) |
| Heading font | **Fredoka** as a static SemiBold instance cut with `fonttools varLib.instancer` | OFL 1.1, verified | Latin-1 complete except U+00AD; the common punctuation (– — ‘ ’ “ ” • … ‰ ‹ › €) present |

Two traps found in the research:

- **Variable fonts**: `Font.cpp` gives msdfgen the raw file with no variation axes, so a variable TTF
  bakes at its default instance: Fredoka at Light (300), Nunito at ExtraLight (200). *Hearthvale*
  ships static instances only.
- **Mixed styles**: Kenney and KayKit are both low-poly gradient-atlas styles but not identical.
  KayKit's characters are chunkier. H0 puts them side by side in one frame before committing (HD2).

**Licence handling**: every pack's own `License.txt` (or `OFL.txt`) is kept beside its files under
`examples/Hearthvale/assets/<pack>/`, and the credits screen and `README.md` list every author. The
v1.0 `THIRD-PARTY-NOTICES.md` (P10) gathers them into the release archive. A Quaternius pack is used
only if the `License.txt` inside the downloaded zip says CC0.

---

## 3. Technical design

### 3.1 Project layout

```
examples/Hearthvale/
  premake5.lua                 copyAssimpRuntime(), NOMINMAX, WindowedApp in Distribution
  assets/                      packs (each with its licence), audio, fonts, ui
  src/
    main.cpp                   CreateApplication, --graphics/--vsync, asset root, UpdateInBackground
    HearthvaleLayer.*          SceneManager, fixed-step driving, screens
    LaunchOptions.*            flags (§3.6)
    GameTuning.h               every number: day length, prices, growth days, radii, curves
    Defs/                      CropDefs, BuildDefs, ItemDefs, VillagerDefs, BoardDefs (code tables)
    World/
      TileGrid.*               the authoritative grid: ground state, occupant, crop, watered
      Valley.*                 seeded generation of decoration and the village layout
      ValleyWorld.*            builds and rebuilds entities from the grid (Static cells)
    Farm/
      Calendar.*               time, day, season, weather (seeded per day)
      Crops.*                  growth at the day roll, regrowth, seasons, the greenhouse
      Placement.*              target tile, ghost preview, rules, place/remove, costs
      Tools.*                  hoe, can, axe, pickaxe, scythe
      Economy.*                inventory, prices, shipping bin, shop
      Board.*                  community requests, festival
    Life/
      Player.*                 controller, facing, tool use, interact
      Villagers.*              schedules on waypoints, dialogue, gifts
      Animals.*                wander in pens, produce daily (if HD3)
    Look/
      Sky.*                    sun, moon, ambient, fog curves; light LOD for lamps
      Vfx.*                    particle effects (owned), emitters
      Audio.*                  buses, season music crossfade, ambience layers
      CameraRig.*              three-quarter follow, 90° rotation, zoom
    Ui/                        Title, SaveSlots, Hud, Inventory, BuildPalette, Shop,
                               ShippingBin, Dialogue, Board, Pause, Options, Credits
    Save/
      SaveGame.*               schema, version, migrate, read/write through SaveData
    Debug/
      Checks.*, CheckReport.h  --check suites (§5)
      Autoplay.*               the farmer bot (§3.5)
```

### 3.2 One owner of the update order

A `ValleyDirector` script, like *Marionette*'s `ArenaDirector`, runs the frame in one fixed order:

input → player intent → tools and placement → calendar (and the day roll) → crops (only at the day
roll) → villagers → animals → sky → VFX and audio → camera → UI.

Nothing else updates game state. The day roll (sleep or 2:00) runs, behind a fade: crops grow,
sprinklers water, the shipping bin pays, weather is rolled, the board updates, and the autosave
writes.

### 3.3 The grid is the truth; entities are derived

`TileGrid` is plain data: per tile, a ground state (grass, tilled, watered, path, water, blocked), an
occupant id (a buildable instance, a decoration or a crop), and a crop record (def, planted day,
stage, days watered). Saves write the grid and the occupant list, never entities.

`ValleyWorld` turns the grid into entities:

- **Ground**: 16 × 16-tile cells, each a handful of merged quads per ground state, `Static`.
- **Decoration and buildables**: one entity each, `Static`, on the default or atlas material. Kenney
  and KayKit share one texture atlas per pack, so a pack is one material and one batch key.
- **Crops**: one entity each, `Static`. A stage change at the day roll swaps the mesh; the rebakes
  all happen behind the sleep fade.
- **Watered soil**: the tilled tile's colour darkens through its `Color`. This is why v1.0's static
  change detection must compare colour and surface overrides, not only transforms and meshes (E2,
  §10).
- **Characters, animals, the ghost preview and particles**: dynamic.

Placing or removing one object changes one cell, so a build session is a stream of single-cell
rebakes: the P5 path exercised every few seconds, not once at load.

### 3.4 Determinism

- **Seeds**: one valley seed for generation, and each day's weather and random events from
  `hash(seed, day)`.
- **Time**: game time advances only through the director's fixed step. Under `--fixed-dt`, a
  simulated week is identical run to run, which the growth and save checks rely on.
- **Floats**: growth is in whole days. Prices and counts are integers. Nothing gameplay-relevant
  depends on frame timing.

### 3.5 The autoplay farmer

`Autoplay` drives the same `PlayerIntent` the input fills, as *Marionette*'s brains do. Each day it
waters every crop, harvests what is ripe, replants, ships the harvest, buys seeds, places one fence
run and one lamp, and sleeps. With `--days=N --fixed-dt=1/60 --steps-per-frame=K` it plays N days
fast. That serves balance (coins and board progress by day), the soak (memory flat over 60 seasons
of save and load) and perf (`--perf` over a fixed autoplay day).

### 3.6 Flags

| Flag | Effect |
|---|---|
| `--check` | Runs every suite in §5, logs `[PASS]`/`[FAIL]`, then continues |
| `--autoplay` | The farmer bot plays |
| `--days=N` | With `--autoplay`, plays N days and closes, logging a `[Day]` summary line per day |
| `--fixed-dt=<s>`, `--steps-per-frame=K` | As *Marionette*; `--days` implies `--fixed-dt=1/60` |
| `--seed=N` | Valley and weather seed |
| `--day=N`, `--season=spring|summer|fall|winter`, `--time=HH:MM` | Start at that point of a fresh game |
| `--weather=clear|rain|snow` | Forces today's weather |
| `--load=<slot>`, `--save-dir=<path>` | Loads a slot; redirects saves (tests never touch a player's saves) |
| `--layout=demo` | A pre-built late-game farm: every buildable placed, every crop planted (captures, perf) |
| `--perf` | Mean frame, `Scene::OnUpdate`, render and GPU timers over 600 frames, as `[PERF]` lines |
| `--freeze` | Stops time and animation (captures) |
| `--no-post`, `--no-shadows`, `--no-particles`, `--no-fog` | Feature before/afters |
| `--no-instancing`, `--no-static`, `--no-cull` | The v1.0 throughput before/afters (forces the engine's fallbacks) |
| `--graphics=`, `--vsync=` | As every example |

Scripted runs (`--check`, `--autoplay`, `--days`, `--perf`) set `UpdateInBackground`, as *Marionette*
does, so an unfocused window doesn't pause them.

---

## 4. Milestones

Each milestone is one commit, or a short run of commits, that builds on all three backends and
leaves the game playable. Each ends with captures on the v1.0 log and a fresh-agent review of its
diff. Estimates are focused dev days.

| # | Milestone | Est. | Needs (v1.0) | Done when |
|---|---|---|---|---|
| **H0** | Assets and scaffold: download and verify every pack's licence, settle HD1–HD4, import test (each model in the Model 3D Test), Kenney and KayKit side by side, E1 if HD3 needs it, project scaffold, premake, launch options, an empty valley with the farmer standing on it | 3 d | — | Every chosen asset loads textured; licences beside the files; the asset `--check` suite passes |
| **H1** | Valley and farmer: `TileGrid`, seeded generation, ground cells, decoration, farmer movement and animation, the camera, the sun's day curve, a text HUD (clock) | 3 d | P2 (culling) for perf | Walk the whole valley; `--seed` reproduces it; Release ≥ 120 fps on the GPU machine with P2 |
| **H2** | Building: target tile, ghost preview, rules (blocked, cost, footprint), place/remove/rotate, axe and pickaxe, wood and stone, a temporary text build menu | 4 d | P3–P5 | 500 fences placed and removed without a dropped frame; each place or remove rebakes one cell (F4 stat); placement checks pass |
| **H3** | Crops and calendar: tilling, seeds, watering, growth, regrowth, seasons, weather, sleep and the day roll, the shipping bin, sprinklers | 4 d | — | `--days=28 --autoplay` grows and sells two seasons; growth checks pass |
| **H4** | Economy and UI: inventory, hotbar, shop, prices, the board and its requests, every menu on DingoUI | 4 d | U5 (DingoUI with focus navigation) | Every screen works with a pad only; the UI navigation check passes |
| **H5** | Life: villagers (schedules, dialogue, gifts), animals (HD3), the coop and barn, the greenhouse and pond, lamps and windows, mist, particles, music and ambience on buses | 4.5 d | P6, P8 | A night in the square stays in the light budget with no popping; the greenhouse grows in winter |
| **H6** | Saves and shipping: slots, autosave, schema versioning, the `.bak` fallback, options, title, credits, the festival ending, icons and exe metadata, Windows and Linux packages, the itch.io page (cover, screenshots, description) | 3.5 d | P7, P8, P10 | Save checks pass, including a kill mid-write; a full year plays start to festival with `--autoplay` |
| **H7** | Balance, review and perf: tune prices, growth and board goals from `--days=56` logs; a fresh-agent review (`.claude/reviews/<date>-hearthvale-review.md`); `--perf` on all three backends with `--layout=demo`; a 60-minute soak; the name settled (HD9) | 4 d | P10 RC | Review Criticals and Highs fixed; perf targets met (§6); soak flat |

**Total: about 30 dev days, about 6 weeks.** H0–H3 need only P2–P5 (perf) and run in parallel with
P6–P9; H4 waits for DingoUI's U5 (v1.0 §3.1). Before U5 lands, H1–H3 use *Marionette*-style text
HUDs, which H4 replaces.

**Cut order** if time runs short:

1. Animals (HD3), which may already be cut by the licence.
2. Snow and winter weather; winter stays, crops only in the greenhouse.
3. The quality sprinkler and the beehive.
4. Villager gifts; dialogue stays.

Never cut saves, pad support, the credits or the ending.

Suggested commit trail:

1. `feat(examples): scaffold Hearthvale with its assets and licences`
2. `feat(examples): Hearthvale's valley, farmer and day`
3. `feat(examples): build and clear in Hearthvale`
4. `feat(examples): crops, seasons and weather in Hearthvale`
5. `feat(examples): Hearthvale's economy, board and menus`
6. `feat(examples): villagers, animals, lights and sound in Hearthvale`
7. `feat(examples): saves, options and the festival in Hearthvale`
8. `chore(examples): balance and review fixes for Hearthvale`

---

## 5. Checks (`--check`)

Every suite logs a summary line (`Placement checks: N passed, M failed`). CI's smoke test runs
`Hearthvale --check` explicitly, as it does *Marionette*'s (`scripts/ci/linux-smoke-test.sh:77`), so
any `[FAIL]` fails CI.

| Suite | Checks |
|---|---|
| Assets | Every model loads with a texture (or E1 colours), every clip resolves on the farmer's skeleton, every sound and font loads, every def references assets that exist |
| Generation | The same seed gives the same valley (hash of the grid); no decoration on the farm plot, the village paths or the pond |
| Placement | Placing on a free tile succeeds and charges the cost; on a blocked, occupied or out-of-bounds tile fails and charges nothing; removing refunds the right share; a 2 × 2 building checks all four tiles; rotation changes the footprint |
| Static | Placing one object rebakes exactly one cell; removing it rebakes the same cell; a day roll rebakes only cells with crops (reads the renderer's stats) |
| Growth | A crop watered every day is ripe on its def's day and not before; an unwatered day delays it by one; out of season it withers at the season change, except in the greenhouse; regrowing crops come back on schedule; rain waters everything |
| Economy | Every price is positive; selling pays at the day roll, not before; the shop refuses what you can't afford; a full inventory refuses a pickup and leaves the item on the ground |
| Save | A seeded game played 7 days with `--fixed-dt`, saved, loaded and played 7 more matches a straight 14-day run (grid hash, coins, inventory, board); a truncated save falls back to `.bak` with a warning; an older schema version migrates |
| UI | On every screen, every interactive widget is reachable with pad focus moves and returns focus where it came from; nothing overflows its panel at 1280 × 720 and 3840 × 2160 |
| Determinism | `--days=7 --seed=1` logs the same `[Day]` lines twice in a row |

---

## 6. Verification

- **Builds**: Debug, Release and Distribution on Windows and Linux, the whole solution at H0 and H6.
- **Backends**: Vulkan, DX11 and DX12 on the GPU machine at H1, H5 and H7; `--layout=demo` frozen at
  noon, dusk and night, pixel-diffed backend against backend (no gross differences; v0.9's known
  16F rounding aside).
- **Perf targets** (RX 7900 XTX, Release, 1600 × 900, VSync off, `--layout=demo`, the camera over
  the farm at noon with every feature on): frame ≤ 4 ms on every backend; `RenderEntities3D` plus
  `EndScene` ≤ 1 ms CPU; draws ≤ 150. Debug ≤ 16 ms (playable while developing).
  `--no-instancing --no-static --no-cull` gives the before number for the v1.0 log.
- **Engine regression**: *Hearthvale* adds no engine code except E1 (and E2 inside v1.0 P5), so the
  v1.0 0 px set is unaffected.
- **Playthrough**: before H7's review, one full year played by a person on a pad, Windows and Linux,
  notes filed as issues.
- **Launches**: the full game and its scripted runs are started only on the owner's go-ahead on the
  GPU machine, as before.

---

## 7. New-executable and docs checklist

- `examples/Hearthvale/premake5.lua` calls `copyAssimpRuntime()` and defines `NOMINMAX` on Windows.
- Root `premake5.lua` includes it in the Examples group (`:427-438`).
- `.github/workflows/build-release.yml`: an `example-game-hearthvale` job and its upload step (as
  `:190-195`, `:647-695`). Linux packaging, the smoke test and the tester bundle find it through
  `examples/*/`; the smoke test and `run-checks.sh` get an explicit `--check` run; the tester
  `README.md` table gets a row.
- `.vscode/tasks.json` and `launch.json`: Windows and Linux entries, cwd = the example directory.
- `README.md`: the examples table, the roadmap row, credits for every pack. `CLAUDE.md`: project
  structure and the Roadmap's v1.0 entry. `ROADMAP.md`: the v1.0 game paragraph (already names
  *Hearthvale*).
- `examples/Hearthvale/README.md`: how to play, flags, and every asset's author and licence.

---

## 8. Risks

| Risk | Mitigation |
|---|---|
| A chosen pack's licence turns out not to be CC0 (Quaternius' licence change) | Only packs whose own `License.txt`, read in H0, says CC0 (or CC-BY, if HD4 allows it); the animals are the likeliest cut |
| Kenney and KayKit look mismatched | H0's side-by-side frame before any building; fallback: Kenney's own characters or recoloured KayKit textures |
| The moving sun makes cascades crawl | The sun moves in 0.25° steps (a step every ~22 real seconds), each step a quick 1 s blend; elevation capped at 65°, far from the `abs(dir.y) > 0.99` basis switch; checked in H1 against a still sun |
| Rebaking on every placement hitches | Rebakes happen at most once per cell per frame; a cell is ≤ 64k vertices; measured in H2 with 500 placements |
| Ten thousand static entities make `Scene` itself slow (hierarchy memo, ECS iteration) | Measured in H1 before any gameplay; decoration and ground merge into cell entities if needed |
| DingoUI arrives late (U5) | H1–H3 use text HUDs; H4 starts the day U5 lands |
| Save format churn during development | A schema version from H3 on, with a migration test from each saved version kept in `test/` assets |
| A release game needs polish the engine phases don't give (balance, music, art direction) | H7 is balance first; the content target (§2.2) is fixed now, and the cut order protects the ending |
| The name collides with an existing game or trademark | HD9 checks itch.io and Steam before H6's page goes up |

---

## 9. Decisions (settled 2026-10-08, each as recommended)

| # | Decision | Chosen | Rejected |
|---|---|---|---|
| HD1 ✅ | Grid shape | **Square 1 m tiles**: fences, paths and crop rows read naturally, and Kenney's kits are square | Hex tiles with KayKit's Medieval Hexagon Pack: distinctive, but crops and fences on hexes are awkward and that pack has no crops |
| HD2 ✅ | Art set | **Kenney Nature, Fantasy Town and Survival Kits for the world; KayKit Adventurers and clip libraries for people**, confirmed by H0's side-by-side frame | All KayKit (verified licences, no crops) or all Kenney (unverified characters) |
| HD3 ✅ | Animals | **Conditional**: chickens and cows only if a pack's own licence says CC0 in H0, with E1 for material colours; otherwise cut, and the coop and barn go with them | Always (model them from primitives), or never |
| HD4 ✅ | Audio licences | **CC0 first; CC-BY 4.0 allowed** with the credits screen and README naming the author | CC0 only (fewer tracks), or synthesized like the other examples (not release quality) |
| HD5 ✅ | Day length | **12 real minutes**, time stopped in menus | 15 minutes; or a setting in options |
| HD6 ✅ | Ending | **The Lantern Festival when the four boards are done**, then sandbox | Pure sandbox, no ending |
| HD7 ✅ | Saving | **On sleep only, 3 slots**, autosave at the day roll | Save anywhere (state mid-day is much larger and harder to keep consistent) |
| HD8 ✅ | Stamina | **None**: the clock is the only limit | A stamina bar as in the genre |
| HD9 ✅ | Name | **Decide by H6**, after an itch.io and Steam search; *Hearthvale* until then | Fix it now |
| HD10 ✅ | Co-op | **None in 1.0**. *Hearthvale* is the natural candidate for the networking module's showcase later (`ROADMAP.md:425`), so its state stays grid-based and serialisable | Plan for co-op now |

---

## 10. Engine gaps found (feed v1.0)

| # | Gap | Where it goes |
|---|---|---|
| E1 | The model loader drops material base colours and vertex colours (`Model.cpp:160-172`, `Mesh.h:12-17`); a colour-only pack loads white | v1.0 **P1**: `SubMesh::BaseColor` from `AI_MATKEY_BASE_COLOR`, falling back to `AI_MATKEY_COLOR_DIFFUSE`, which games pass as `MeshRendererComponent::Color` (vertex colours stay out: the 48 B vertex layout is frozen) |
| E2 | v1.0 P5's static change detection compared only transforms and meshes; a watered tile changes only its colour | v1.0 **§2.6**, now comparing the whole draw state (transform, mesh id, colour, surface override, shadow mode) |
| E3 | `Renderer2D::DrawText` has no word wrap; dialogue and tooltips need it | DingoUI's U0 (`2026-10-08-v1.0-game-ui-plan.md` §3.1): `Font::MeasureText` with wrap, shared by `DrawText` and DingoUI's `Text` |
| E4 | No body-to-entity lookup | Already v1.0 P9 |
| E5 | No fixed-step hook | Already v1.0 P9 (`OnFixedUpdate`); until then the director steps the scene itself, as *Marionette* does |
| E6 | Cascades are stable for a still light only | Game-side mitigation (§8); an engine fix is post-1.0 unless H1 shows the stepped sun isn't enough |

---

## 11. As built

*(filled in per milestone)*
