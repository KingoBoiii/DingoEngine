# Hearthvale — the v1.0 release game: plan

Drafted 2026-10-08 on `feat/v1` (base `master` @ `4ff48ea`, v0.9.0 plus the
Linux merge). Every file:line below was read that day. This is P13 of
`.claude/plans/2026-10-08-v1.0-plan.md` (its §6 and decision D1). *Hearthvale* is a working title;
HD9 settles the name.

**Status**: refreshed 2026-10-15 (phase P0) on `feat/1-0-hearthvale` against master `f414ba0`
(v1.0 P0–P9 and DingoUI U0–U6 landed) plus `7f4c1e0` (the Kenney packs). Every anchor in §1, §2.7,
§3, §5, §7 and §10 was re-read that day. HD1–HD10 in §9 were settled by the owner on 2026-10-08,
each as recommended. The game is now built in **cloud-only phases P0–P18** (§11.1), which replace
the H0–H7 milestones of §4 as the execution order; §4 stays as the content and estimate reference.
The Kenney packs are on disk (§2.7); music and ambience are synthesized placeholders until the
owner picks tracks.

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
| SaveData, display modes (P8); buses, fog, translucency (on master, #110) | The whole valley in a save, music and ambience sliders, morning mist, greenhouse glass, the pond |
| `OnFixedUpdate` (P9); body lookup (on master, #149) | Deterministic day simulation for `--check`; picking a villager by ray |
| v0.9 | A moving sun with cascades, shadowed lamps, bloom on windows, rain and firefly particles, AO |

---

## 1. Where we start (verified facts)

### 1.1 What the engine gives a farm game today

| Area | Today | Anchor |
|---|---|---|
| Picking | `Scene::ScreenPointToRay(screenPos, viewportSize)` (top-left origin, primary perspective camera); `Ray::IntersectGroundPlane(height, outHit)` (false when parallel or behind) | `Scene/Scene.h:193`, `Core/Ray.h:10,44` |
| Physics casts | `Physics3D::RayCast(ray, maxDistance, outHit, filter)`, `ShapeCastSphere`, `OverlapSphere`, each with an optional `QueryFilter3D` (ignored bodies, layers, sensors); sensors and `Scene::GetEntityFromBody3D` / `GetSensorOverlaps` (#149) | `Physics/3D/Physics3D.h:152-171`, `Physics/3D/PhysicsTypes3D.h:115`, `Scene/Scene.h:298-302`, `docs/physics-3d.md:235` |
| Character | `CharacterController3DComponent { Radius 0.3, Height 1.8, StepHeight 0.3, MaxSlopeAngle 45, CollideWithCharacters }`. The game applies gravity, sets velocity and fetches the controller every frame; `Scene::OnUpdate` steps it | `Scene/Components.h:800-812`, `Physics/3D/CharacterController3D.h:68` |
| Movement pattern | Candlewick's `Player::Update`: WASD, D-pad and stick on world axes, gravity by hand, yaw toward the move direction | `examples/Candlewick/src/Player.cpp:78-135` |
| Follow camera | Candlewick's `CameraRig::Update`: a lagged focus plus a fixed view direction | `examples/Candlewick/src/CameraRig.cpp:110-139` |
| Sun | `DirectionalLightComponent { Direction, Color, Intensity, CastShadows, ShadowStrength, ShadowCasterGroups }`. `Ambient` is **deprecated with default 0** (v1.0 P9, A305): light the scene with an `AmbientLightComponent`, as *Marionette* does | `Scene/Components.h:227-240,254`, `examples/Marionette/src/ArenaWorld.cpp:84-94` |
| Cascades and a moving sun | Cascades are sphere-fit and texel-snapped, so a moving *camera* doesn't shimmer. Nothing covers a moving *light*. The light view switches its up vector when `abs(dir.y) > 0.99` | `docs/shadows.md:187-191`, `Renderer3D.cpp:3199,3384` |
| Light budget | 32 local lights, 16 shadowed at most; a light whose range reaches nothing in view is skipped before the budget picks (v1.0); *Candlewick*'s `LightLod` keeps in-view lights under the budget so none pops | `Graphics/Renderer3D.h:339,354`, `docs/performance.md:53`, `examples/Candlewick/src/LightLod.h:14-20` |
| Text and HUD | **DingoUI** (§1.3) for every screen and the HUD. `Font::MeasureText` wraps (E3 done). *Marionette* now builds its HUD and screens on DingoUI: `Overlay::HudRoot`/`MenuRoot` root presets, a title root, a world root over heads, a row of bars | `docs/game-ui.md`, `Graphics/Font.h:76`, `examples/Marionette/src/Overlay.cpp:36-48`, `Screens.cpp:77`, `Hud.cpp:132,234`, `MarionetteLayer.cpp:30,71` (`SetDefaultFont`) |
| Audio | `AudioEngine::Play(clip, SoundPlayParams{ Looping, Spatialized, Position, ... })`, `AudioSourceComponent`; clips load `.wav`, `.ogg` or `.mp3`. The examples' sounds are synthesized by a per-example `scripts/generate_audio.ps1` (PowerShell; the cloud has no `pwsh`, so Hearthvale's generator is Python) | `examples/Candlewick/src/Audio.cpp:80-106`, `Scene/Components.h:822`, `docs/audio.md:15`, `examples/Marionette/scripts/generate_audio.ps1` |
| Particles | `ParticleEffectParams` (shape, rate, bursts, lifetime, gravity, drag, noise, size, spin, 4 colour keys, blend, flipbook, soft distance); *Candlewick*'s `Flames.h` owns its effects and spawns emitters | `Graphics/Particles.h:47`, `examples/Candlewick/src/Flames.h:12-26` |
| Fixed step | **Landed (v1.0 P9)**: `ScriptableEntity::OnFixedUpdate(step)` runs before `OnUpdate` from the scene's own accumulator; `Scene::SetFixedTimeStep` (default 1/60, clamped to [1/1000, 1/4]), `GetFixedTimeAlpha`, at most `k_MaxFixedStepsPerUpdate` = 8 steps an update (the rest is dropped). Physics stays on the variable step. *Marionette*'s `--fixed-dt` still steps the `SceneManager` `StepsPerFrame` times itself | `Scene/ScriptableEntity.h:43-46`, `Scene/Scene.h:94-113`, `docs/scenes-and-ecs.md:802`, `examples/Marionette/src/MarionetteLayer.cpp:156-175` |
| Checks | `CheckReport.h`: `Check(bool, name)` logs `[PASS]`/`[FAIL]` and counts; suites log a summary; the exit code is unchanged; CI fails on any `[FAIL]` | `examples/Marionette/src/CheckReport.h:9-32`, `scripts/ci/linux-smoke-test.sh:128` |
| Launch options | A struct plus `ParseInt`/`ParseSeed`/`ParseSeconds`/`ParseRange`/`ParseFlag` over `ApplicationCommandLineArgs`, `Parse` cached once; `ParseVSync` in `main.cpp`; scripted runs set `UpdateInBackground` | `examples/Marionette/src/LaunchOptions.cpp:16-246` (`Parse` at `:140`), `main.cpp:33,59-75` |
| Engine flags | The engine itself reads `--graphics=vulkan\|dx11\|dx12\|headless`, `--window=WxH`, `--no-vsync`, `--log-here`/`--log-dir`, `--save-dir=<path>`, `--vulkan-validation`; the game does not parse these | `docs/application-and-layers.md:170-185` |
| Active flag | `Entity::SetActive/IsActive/IsActiveInHierarchy` (v1.0 P9) switch a subtree off without destroying it | `docs/scenes-and-ecs.md:51,812` |

### 1.2 What models bring with them

- **Base colours: E1 landed (v1.0 P1).** `SubMesh::BaseColor` is glTF's `baseColorFactor`, else
  the diffuse colour; **nothing applies it**: the game passes it as `MeshRendererComponent::Color`
  (one entity, or one `Color`, per submesh). Vertex colours stay out (48 B vertex frozen).
  `include/DingoEngine/Graphics/Model.h:15-30`, `docs/asset-pipeline.md:115`, `docs/migrating-to-1.0.md:285`.
- **Textured packs**: a material pointing at an **external PNG** loads it into `SubMesh::DiffuseTexture`.
  *Marionette* makes one lit material per PNG (`Renderer3D::CreateLitMaterial`, texture in slot 0;
  `examples/Marionette/src/GameAssets.cpp:42-60`). Kenney's Fantasy Town, Survival and Cube Pets GLBs
  reference `Textures/colormap.png` beside them: one material per pack, one batch key.
- **Nature Kit is colour-only**: its GLBs carry no image, and several materials per model
  (`crop_carrot`: `grass` + `leafsFall`; `tree_oak`: `leafsGreen` + `woodBark`), so each submesh
  needs its own `BaseColor` as its `Color` on the default lit material.
- **Bone-less animation is dropped**: a model without bones loads its meshes pre-transformed and
  drops its clips with a warning (`docs/asset-pipeline.md:120-129`; a clips-only load of one fails,
  `docs/animation.md:184`). Kenney Cube Pets carry node animations (`static`, `idle`, `walk`, `run`,
  `eat`, ...) and no skin, so **animals animate procedurally** (bob, hop, turn) in game code.
- **Characters and clips**: KayKit `Rig_Medium` characters and the clip libraries load, retarget by
  joint name, and play in *Marionette* (`2026-10-03-marionette-plan.md:160-174`). Only **two
  Adventurers** are on disk (`Barbarian.glb`, `Knight.glb`, with `barbarian_texture.png`,
  `knight_texture.png`), plus `Skeleton_Minion`/`Skeleton_Warrior`; the farmer and four villagers
  share them (tint, or a recoloured texture copy; see §2.7). Clips: `Rig_Medium_General` (`Idle_A/B`,
  `Interact`, `PickUp`, `Use_Item`, `Throw`, `Hit_A/B`), `Rig_Medium_MovementBasic` (`Walking_A/B/C`,
  `Running_A/B`, jumps), plus `MovementAdvanced`, `CombatMelee`, `Special`.

### 1.3 DingoUI (workstream U, U0–U6 landed)

`docs/game-ui.md`, `include/DingoEngine/UI/DingoUI.h` (static class `DingoUI`). Immediate mode,
declared every frame from `OnUpdate`:

- `DingoUI::BeginRoot(name, UIRootParams)` / `EndRoot()`; `BeginBox(UIBoxParams)` / `EndBox()`
  (flexbox: `Direction`, `Gap`, `Padding`, `Justify`, `Align`, `UISize::Fit/Fixed/Grow/Percent`);
  `Text` (wraps by default), `Image(texture, UIImageParams)`; widgets `Button`, `Toggle`,
  `Slider`, `SliderInt`, `ProgressBar`, `Selectable`, `BeginScrollArea`, each returning
  `UIInteraction { Hovered, Pressed, Down, Clicked, Changed, Focused }`; `BeginPanel`/`EndPanel`;
  `UIStyle` themes with `PushStyle`/`SetTheme` and `UINineSlice` (`Surface`, `Panel`) for the Kenney
  UI pack's frames; `Tween`, root `UITransition`s; `BeginWorldRoot` over an entity or point.
- `UIScale::ScaleWithScreen()` (1920x1080 reference) for resolution independence.
- **Focus (U5)**: arrows, D-pad and left stick move focus; Enter/Space/pad A accept; Escape/pad B is
  `DingoUI::IsBackPressed()`. Only `SetModal(true)` roots or `BlocksInput` roots with
  `SetNavigable(true)` take focus; `UIWidgetParams().SetDefaultFocus()`; `DingoUI::SetFocus(label)`
  (next frame, call in the widget's container). The game checks `DingoUI::IsCapturingInput()` before
  reading the stick or clicks for the farmer. `docs/game-ui.md:236-291`.
- Font: `DingoUI::SetDefaultFont(font)` after the device exists; unset before deleting.
- **Off headless**: every call returns at once, `BeginRoot` returns `false`.
- **No public way to drive focus from a check**: simulated pads (`InputBackend::Simulate*`) are
  private to the engine's test app, and there is no focused-id query. See E7 (§10).

### 1.4 Culling, instancing and static geometry (v1.0 P2–P5)

`docs/performance.md`:

- **Culling** is automatic (`:31-55`); `MeshRendererComponent::CullDistance` for small far props
  (a mesh with one is never baked, `:56-72`). `Renderer3D::SetCullingEnabled(false)` is `--no-cull`.
- **Instancing** is automatic for ≥ `InstanceThreshold` (8) copies of one `Mesh` with one material
  and shadow setting (`:212-250`); `Renderer3D::SetInstancingEnabled(false)` is `--no-instancing`.
  **Baked (`Static`) meshes are never instanced** (`:238-239,512`): each kind is either baked or
  instanced, a choice P3 records.
- **Static** is `MeshRendererComponent::Static = true` (`Scene/Components.h:519-525`). The scene's
  bake uses a **fixed 32-unit cell grid** (`StaticBatchParams::CellSize` default, not settable for a
  scene's `Static` entities: `Scene/Systems/StaticGeometry.cpp:58`); a mesh belongs to the cell its
  bounds' centre falls in (`docs/performance.md:136-140`). So the 96 × 96 valley is 3 × 3 engine
  cells, and a placement rebakes a 32 × 32 m cell. A hand-held `StaticBatch` with
  `SetCellSize(16)` is the alternative (`:182-210`), drawn with `SubmitStaticBatch` between
  `BeginScene`/`EndScene`.
- **Change detection (E2 landed)**: every frame the scene compares each `Static` entity's draw state
  (mesh and mesh id, `Color`, material and id, shadows, shadow groups, surface override) **bitwise**
  with its bake (`StaticGeometry.cpp:12-25,91-92`). A colour change rebakes quietly; moving one warns
  once. Write the same float values for "unchanged", or every frame rebakes.
- Baked meshes cost 52 B a vertex and 4 B an index of GPU memory each, copies included (`:176-180`).
- Stats: `Renderer3D::GetStatistics()` (`StaticMeshes`, `StaticCellsBaked`, `InstancedMeshes`,
  `CulledMeshes`, `DrawCalls`), complete after `EndScene` (`:420-428`). None headless.

### 1.5 Saves and settings (v1.0 P8)

`docs/saves-and-settings.md`, `Core/SaveData.h`, `Core/Settings.h`:

- `SaveData`: JSON tree, `operator[]`/`Push` to write, const `Get(key, fallback)`/`As*` to read (read
  through a const ref), sorted keys so the same data writes the same file; `WriteFile` is atomic with
  `.tmp` and `.bak`, `ReadFile` falls back to `.bak` and logs it.
- `SaveSlots::Write/Read/Exists/Delete/List` in `Application::GetSaveDirectory()`; the engine's own
  `--save-dir=<path>` redirects it. Slot names `A-Z a-z 0-9 _ -`.
- `Settings` (`Display`, `Audio.MasterVolume`/`BusVolumes`, `Input.GamepadDeadzone`, `Game` as a
  `SaveData`): `Load()`, `Apply()` (create the buses first), `Save()`;
  `ApplicationParams::SettingsSlot` opens the window in the saved mode.
- **No save versioning in the engine**: the game writes its own `schema` integer and migrates.

### 1.6 Headless (`--graphics=headless`)

`docs/application-and-layers.md:285-315`: no window, renderers, ImGui or DingoUI; `Input` reads all
up; `Run()` calls `OnUpdate` about **60 times a second in real time**; `AssetManager`, `AudioEngine`,
scenes, scripts and physics work. Loading any GPU asset (texture, model, font, shader, or the
renderer's `GetBoxMesh`) **crashes** (issue #248, open). So the headless `--check` runs only the
simulation suites (grid, calendar, crops, economy, board, saves, determinism), and a fast `--days`
run steps the simulation many times per `OnUpdate` in its own loop: `OnFixedUpdate` caps at 8
steps an update, which at 60 updates a second is far too slow for a 12-minute day.

### 1.7 Scale reference

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

**On disk since `7f4c1e0`** (2026-10-15), each pack with Kenney's own `License.txt` saying **CC0**
(`game-icons/license.txt` lower-case), under `examples/Hearthvale/assets/kenney/<pack>/` (22 MB). The
2026-10-08 candidates that were not downloaded (KayKit Hexagon, Furniture and Restaurant Bits,
Quaternius Farm Animals, Fantasy UI Borders, the OpenGameArt music and ambience, Fredoka) are out
unless the owner adds them.

| Pack | Path and format | What Hearthvale uses (real file stems) |
|---|---|---|
| **Nature Kit 2.1** | `nature-kit/Models/*.glb` (329, flat folder, **colour-only, no texture**: needs `SubMesh::BaseColor`) | **Crops**: `crop_carrot`, `crop_melon`, `crop_pumpkin`, `crop_turnip` (ripe, single model each); staged `crops_cornStageA`–`D`, `crops_wheatStageA`/`B`, `crops_bambooStageA`/`B`, `crops_leafsStageA`/`B` (generic sprout/young); soil `crops_dirtSingle`, `crops_dirtRow*`, `crops_dirtDoubleRow*`. **Fences**: `fence_simple`, `fence_simpleCenter`, `fence_simpleHigh`/`Low`, `fence_planks`, `fence_planksDouble`, `fence_corner`, `fence_bend*`, `fence_gate`. **Trees**: `tree_default`, `tree_oak`, `tree_fat`, `tree_simple`, `tree_small`, `tree_tall`, `tree_thin`, `tree_cone`, `tree_detailed`, `tree_plateau`, `tree_blocks`, each with `_dark` and `_fall` variants (autumn swap; note the typo `tree_fat_darkh`), pines `tree_pine{Default,Ground,Round,Small,Tall}*`. **Clearables**: `stump_old`, `stump_round`, `stump_square*`, `log`, `log_large`, `log_stack*`, `rock_small{A..I}`, `rock_large{A..F}`, `rock_tall{A..J}` (and `stone_*` twins), `grass`, `grass_large`, `grass_leafs*`, `plant_bush*`, `plant_flat*`, `flower_{purple,red,yellow}{A,B,C}`, `mushroom_*`. **Paths**: `path_stone*`, `path_wood*`, `ground_path*`, `ground_grass`; **water**: `ground_river*`, `lily_large`/`small`, `bridge_*`. **Props**: `sign`, `campfire_*`, `pot_large`/`small`, `tent_*`, `statue_*`, `cliff_*` (valley rim) |
| **Fantasy Town Kit** | `fantasy-town-kit/Models/GLB format/*.glb` (167), texture `GLB format/Textures/colormap.png`; alternate `Models/Textures/variation-a.png` | Modular houses: `wall-*`, `wall-wood-*` (doors `wall-door`, windows `wall-window-glass`/`-shutters`/`-round`/`-small`), `roof-*`, `roof-high-*`, `chimney*`, `stairs-*`, `planks*`, `pillar-*`, `overhang`. Village: `stall`, `stall-green`, `stall-red`, `stall-bench` (bench), `stall-stool`, `lantern` (lamp post), `fountain-*` (square), `windmill`, `watermill`, `wheel`, `cart`, `cart-high`, `banner-green`/`-red`, `hedge*`, `fence`, `fence-gate`, `fence-curved`, `fence-broken`, `road-*`, `rock-large`/`-small`/`-wide`, `tree`, `tree-crooked`, `tree-high*` |
| **Survival Kit** | `survival-kit/Models/GLB format/*.glb` (80), `GLB format/Textures/colormap.png` | **Tools**: `tool-hoe`, `tool-axe`, `tool-pickaxe`, `tool-shovel`, `tool-hammer` (+ `-upgraded`); **no watering can or scythe** (use `bucket` for the can; the scythe is a primitive or `tool-shovel` stand-in). `chest`, `barrel`, `box*`, `bucket`, `workbench*`, `signpost`, `signpost-single`, `fence`, `fence-doorway`, `fence-fortified`, `resource-wood`, `resource-planks`, `resource-stone*` (drops), `tree`, `tree-tall`, `tree-autumn*`, `tree-log*`, `tree-trunk`, `rock-a/b/c`, `patch-grass*`, `campfire-*` |
| **Cube Pets** | `cube-pets/Models/GLB format/animal-*.glb` (24), `Textures/colormap.png` | **Animals (HD3 resolved: CC0)**: `animal-chick` (coop), `animal-cow` (barn), `animal-pig`, `animal-hog`, `animal-bunny`, `animal-dog`, `animal-cat`. Node-animated, **no skin**: the loader drops their clips, so they move procedurally |
| **UI Pack** | `ui-pack/PNG/{Blue,Green,Grey,Red,Yellow,Extra}/{Default,Double}/*.png` (434), `Font/Kenney Future.ttf`, `Kenney Future Narrow.ttf`, `Sounds/{click,switch,tap}-{a,b}.ogg` | 9-slice faces `button_rectangle_*`, `button_square_*`, `button_round_*`, `check_*`, `slide_*`, `arrow_*`, `icon_*`, `star*`, `input_*`, `divider*`. Kenney Future is a CC0 heading-font candidate (replaces Fredoka) |
| **Game Icons** | `game-icons/PNG/{Black,White}/{1x,2x}/*.png` | Menu glyphs only: `gear`, `home`, `pause`, `save`, `musicOn/Off`, `audioOn/Off`, `basket`, `cart`, `trashcan`, `wrench`, `star`, `trophy`, `information`, `warning`, `exitLeft/Right`, pad prompts `buttonA/B/X/Y`, `buttonL1/R1`, `buttonStart/Select`, `gamepad`. **No item icons** (crops, seeds, tools, resources): inventory slots use colour swatches plus text until P14 picks a source |
| **Interface Sounds** | `interface-sounds/Audio/<name>_00N.ogg` (100) | `click`, `select`, `back`, `open`, `close`, `confirmation`, `error`, `drop`, `toggle`, `switch`, `tick`, `scroll`, `pluck`, `bong`, `glass`, `maximize`/`minimize` |
| **UI Audio** | `ui-audio/Audio/*.ogg` (50) | `click1`–`5`, `rollover1`–`6`, `switch1`–`38`, `mouseclick1`, `mouserelease1` |
| **RPG Audio** | `rpg-audio/Audio/*.ogg` (51) | `chop` (axe), `footstep00`–`09`, `handleCoins`, `handleCoins2` (sell/buy), `doorOpen_1/2`, `doorClose_1`–`4`, `creak1`–`3`, `metalPot1`–`3`, `metalClick`, `metalLatch`, `cloth1`–`4`, `bookFlip1`–`3` (board), `dropLeather` |

Still from elsewhere:

| Need | Source | Status |
|---|---|---|
| Farmer and villagers | KayKit Adventurers `Barbarian.glb`, `Knight.glb` (+ `Skeleton_*`), clip libraries `Rig_Medium_*.glb` in `examples/Marionette/assets/{characters,animations}`, licences `LICENSE-KayKit-*.txt` there (CC0) | Copy what is used, with its licence file. Only two Adventurers: the five people share them, told apart by tint or a recoloured copy of the texture |
| Body font | Inter (OFL 1.1): `Inter-Bold.ttf` in every example (`examples/Marionette/assets/fonts/`), `Inter-Regular.ttf` in `test/assets/fonts/`; licence `examples/Marionette/assets/LICENSE-Inter.txt`, `scripts/notices/licenses/inter.txt` | Copy `Inter-Regular.ttf` and `Inter-Bold.ttf` with `LICENSE-Inter.txt` (no SemiBold on disk) |
| Music, ambience | Synthesized placeholders by a Python generator (no `pwsh` in the cloud), as the examples' `generate_audio.ps1` do | HD4 tracks wait for the owner |

Two traps found in the research:

- **Variable fonts**: `Font.cpp` gives msdfgen the raw file with no variation axes, so a variable TTF
  bakes at its default instance: Fredoka at Light (300), Nunito at ExtraLight (200). *Hearthvale*
  ships static instances only.
- **Mixed styles**: Kenney and KayKit are both low-poly gradient-atlas styles but not identical.
  KayKit's characters are chunkier. P14 puts them side by side and P18 (owner, GPU) judges it (HD2).

**Licence handling**: every pack's own `License.txt` (or `OFL.txt`) is kept beside its files under
`examples/Hearthvale/assets/<pack>/`, and the credits screen and `README.md` list every author. The
v1.0 `THIRD-PARTY-NOTICES.md` gathers them into the release archive through
`scripts/notices/components.txt` and `scripts/notices/licenses/` (P15). Packaging (P15) packs only the
files the art registry names, not all 877 UI-pack files.

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

- **Ground**: 16 × 16-tile cells, each a handful of merged quads per ground state, `Static`. The
  16 × 16 cell is the game's own merge and dirty unit; the engine bakes a scene's `Static` meshes in
  fixed 32 m cells by bounds centre (§1.4), so one engine cell holds up to four game cells and a
  change rebakes all of that 32 m cell. P3 records whether that is good enough or a game-held
  `StaticBatch` with `SetCellSize(16)` replaces scene `Static`.
- **Decoration and buildables**: one entity each, on the default material (Nature Kit, colour per
  submesh from `BaseColor`) or the pack's atlas material (Fantasy Town, Survival, Cube Pets: one
  `colormap.png` each, one material and one batch key per pack). Baked meshes never instance (§1.4),
  so each kind is either `Static` (buildables, few copies, change rarely) or left dynamic to instance
  (thousands of copies of a few decoration meshes); P3 picks per kind and records it.
- **Crops**: one entity each, `Static`. A stage change at the day roll swaps the mesh; the rebakes
  all happen behind the sleep fade.
- **Watered soil**: each tilled tile is its own entity (one shared quad or `crops_dirtSingle`), and
  watering darkens its `Color`. E2 landed: the scene compares colour bitwise, so the change rebakes
  its cell quietly, and the same two constant colours must be written every time (§1.4).
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
| `--load=<slot>` | Loads a slot. `--save-dir=<path>` (tests never touch a player's saves) is the engine's own flag, not parsed by the game |
| `--layout=demo` | A pre-built late-game farm: every buildable placed, every crop planted (captures, perf) |
| `--perf` | Mean frame, `Scene::OnUpdate`, render and GPU timers over 600 frames, as `[PERF]` lines |
| `--freeze` | Stops time and animation (captures) |
| `--no-post`, `--no-shadows`, `--no-particles`, `--no-fog` | Feature before/afters |
| `--no-instancing`, `--no-static`, `--no-cull` | The v1.0 throughput before/afters: `Renderer3D::SetInstancingEnabled(false)`, `Static = false` on every game entity, `Renderer3D::SetCullingEnabled(false)` |
| `--graphics=`, `--vsync=` | `--graphics` is the engine's (`headless` runs the simulation suites only, §1.6); `--vsync=on\|off` parsed as *Marionette*'s `ParseVSync` |

Scripted runs (`--check`, `--autoplay`, `--days`, `--perf`) set `UpdateInBackground`, as *Marionette*
does, so an unfocused window doesn't pause them. Under `--days` the director runs its own loop of
fixed sim steps (`--steps-per-frame` per `OnUpdate`), not `OnFixedUpdate`, whose 8-step cap would
make a headless 12-minute day take minutes (§1.6).

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
| **H5** | Life: villagers (schedules, dialogue, gifts), animals (HD3), the coop and barn, the greenhouse and pond, lamps and windows, mist, particles, music and ambience on buses | 4.5 d | P6 | A night in the square stays in the light budget with no popping; the greenhouse grows in winter |
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


### 4.1 Cloud or GPU machine

The labels and the cloud environment are as in the v1.0 plan's §3.2: **Cloud**, **Cloud, GPU
sign-off**, or **GPU machine**. A game is judged by looking at it, listening to it and playing it,
so more of *Hearthvale* needs the GPU machine than the engine phases do.

| # | Where | What needs the GPU machine (or a person) |
|---|---|---|
| H0 | **Web access, then GPU machine** | Downloading the packs and reading their licences needs kenney.nl, itch.io and opengameart.org, which the planning container's network blocked. That means the owner, or a cloud environment with those hosts allowed. The Kenney and KayKit side-by-side frame is an art call on real hardware. Scaffold, premake and the asset checks are Cloud |
| H1 | Cloud, GPU sign-off | The "Release ≥ 120 fps" target; checking that the stepped sun doesn't make cascades crawl, on a real GPU |
| H2 | Cloud, GPU sign-off | "500 placements without a dropped frame" is a timing |
| H3 | Cloud | — (growth and the day roll are deterministic and checked by `--days`) |
| H4 | Cloud, GPU sign-off | The pad-only check is simulated; one pass with a **real pad** |
| H5 | **GPU machine** | Lights, mist, bloom, particles and the light budget at night are tuned by eye; music and ambience levels by ear. The cloud can wire it all; it can't tune it |
| H6 | Cloud, GPU sign-off | Saves, options, packaging and the ending are Cloud. Windows packaging runs in the release job. The itch.io page (cover, screenshots) needs captures from the GPU machine and the owner's account |
| H7 | **GPU machine** | `--perf` on three backends, the 60-minute soak, and a person's full-year playthrough on a pad. Balancing from `--days=56` logs is Cloud |

**Sign-off rhythm**:

- H1–H4 join the engine's batches (S2 and S3).
- H5 and H7 are GPU-machine work in their own right: about 2 and 3 days of the owner's time, plus
  whatever the cloud prepares for them.

---

## 5. Checks (`--check`)

Every suite logs a summary line (`Placement checks: N passed, M failed`). CI's smoke test runs
`Hearthvale --check` explicitly, as it does *Marionette*'s (`scripts/ci/linux-smoke-test.sh:128`), so
any `[FAIL]` fails CI. The smoke test also launches every `examples/*/` directory plainly
(`:123-126`), so `examples/Hearthvale` must build a binary before anything reaches master.

**Headless or windowed**: Generation, Placement (grid side), Growth, Economy, Save and Determinism
are plain C++ and run under `--check --graphics=headless`. Assets, Static (reads
`Renderer3D::GetStatistics()`) and UI (DingoUI is off headless) need a window (`xvfb-run` on
llvmpipe in the cloud).

| Suite | Checks |
|---|---|
| Assets | Every model loads with a texture (or E1 colours), every clip resolves on the farmer's skeleton, every sound and font loads, every def references assets that exist |
| Generation | The same seed gives the same valley (hash of the grid); no decoration on the farm plot, the village paths or the pond |
| Placement | Placing on a free tile succeeds and charges the cost; on a blocked, occupied or out-of-bounds tile fails and charges nothing; removing refunds the right share; a 2 × 2 building checks all four tiles; rotation changes the footprint |
| Static | Placing one object rebakes exactly one cell; removing it rebakes the same cell; a day roll rebakes only cells with crops (reads the renderer's stats) |
| Growth | A crop watered every day is ripe on its def's day and not before; an unwatered day delays it by one; out of season it withers at the season change, except in the greenhouse; regrowing crops come back on schedule; rain waters everything |
| Economy | Every price is positive; selling pays at the day roll, not before; the shop refuses what you can't afford; a full inventory refuses a pickup and leaves the item on the ground |
| Save | A seeded game played 7 days with `--fixed-dt`, saved, loaded and played 7 more matches a straight 14-day run (grid hash, coins, inventory, board); a truncated save falls back to `.bak` with a warning; an older schema version migrates |
| UI | On every screen, every interactive widget is reachable with pad focus moves and returns focus where it came from; nothing overflows its panel at 1280 × 720 and 3840 × 2160. The game cannot inject pad input or read the focused id (E7), so this suite checks the game's own screen model (each screen's widget list, default focus, back target) plus layout sizes from `Font::MeasureText`; a real pad pass is P18 |
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

- `examples/Hearthvale/premake5.lua` as *Marionette*'s (`examples/Marionette/premake5.lua`):
  `NOMINMAX` on Windows, `WindowedApp` in Distribution, then `distributionSymbols()`,
  `copyAssimpRuntime()`, `exeMetadata { Name, Description, Company, Copyright }` and `embedPackKey()`
  (root `premake5.lua:150,194,268`), with `icon.ico`/`icon.png` from `scripts/icons/make-icons.sh`
  (a row in `scripts/icons/icons.txt`). Packed assets: `ApplicationParams.Assets` with
  `SetPackageKey(Dingo::Generated::GetPackageKey())` (`examples/Marionette/src/main.cpp:59-75`).
- Root `premake5.lua` includes it in the Examples group (`:644-655`).
- `.github/workflows/build-release.yml`: an `example-game-hearthvale` job and its upload step (as
  Marionette's `:202-207`, `:650-689`). Linux packaging, the smoke test and the tester bundle find it through
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
| A chosen pack's licence turns out not to be CC0 (Quaternius' licence change) | **Resolved 2026-10-15**: every pack on disk carries Kenney's CC0 `License.txt`; Quaternius is not used |
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
| HD3 ✅ | Animals | **Conditional**: chickens and cows only if a pack's own licence says CC0 in H0, with E1 for material colours; otherwise cut, and the coop and barn go with them. **Resolved 2026-10-15**: Kenney Cube Pets (CC0) `animal-chick` and `animal-cow`, moved procedurally (their node clips don't load) | Always (model them from primitives), or never |
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
| E1 ✅ | The model loader dropped material base colours | **Landed** (v1.0 P1): `SubMesh::BaseColor` (`include/DingoEngine/Graphics/Model.h:15-30`, `docs/asset-pipeline.md:115`). The game passes it as `MeshRendererComponent::Color`; vertex colours stay out |
| E2 ✅ | Static change detection compared only transforms and meshes | **Landed** (v1.0 P5): the whole draw state, bitwise (`src/DingoEngine/Scene/Systems/StaticGeometry.cpp:12-25,91-92`, `docs/performance.md:148-162`) |
| E3 ✅ | No word wrap | **Landed** (DingoUI U0): `Font::MeasureText` (`Graphics/Font.h:76`, `docs/rendering-2d.md:134`); DingoUI `Text` wraps by default |
| E4 ✅ | No body-to-entity lookup | **Landed** (#149): `Scene::GetEntityFromBody3D` (`Scene/Scene.h:298`, `docs/physics-3d.md:235`). Use it to pick a villager or animal from a `Physics3D::RayCast` hit |
| E5 ✅ | No fixed-step hook | **Landed** (v1.0 P9): `ScriptableEntity::OnFixedUpdate(step)`, `Scene::SetFixedTimeStep` (`Scene/ScriptableEntity.h:43-46`, `Scene/Scene.h:94-113`, `docs/scenes-and-ecs.md:802`). The `ValleyDirector` advances the simulation in `OnFixedUpdate` at normal speed; `--days` runs its own loop (§3.6) |
| E6 | Cascades are stable for a still light only | Game-side mitigation (§8); an engine fix is post-1.0 unless P18 shows the stepped sun isn't enough |
| E7 | No public way for a game's `--check` to drive DingoUI focus: simulated pads are private (`Core/Input.h:186-190`, reached only through `friend struct Internal::InputBackend`), and no focused-widget query exists | File as an `enhancement`; until then the UI suite checks the game's own screen model (§5) |
| E8 | A bone-less model's node animations are dropped (`docs/asset-pipeline.md:126-129`), so Kenney Cube Pets don't animate | File as an `enhancement` (rigid node clips); game-side procedural motion until then |
| E9 | A scene's `Static` bake has a fixed 32 m cell (`StaticGeometry.cpp:58` uses the default `StaticBatchParams`) | File as an `enhancement` (a scene-level cell size) only if P3/P17 show the 32 m rebake costs; else note it |
| #248 | Loading a GPU asset headless crashes instead of failing | Open `enhancement`. Hearthvale's sim code never loads GPU assets, and the headless `--check` skips the world, assets and UI |

---

## 11. As built

### 11.1 Cloud build phases (P0–P18)

The game is built in cloud sessions (Linux, GCC, no GPU; llvmpipe under Xvfb), one phase per agent,
in this order. Each phase appends its "As built" note below; those notes override §2–§3. Game code
lives only in `examples/Hearthvale`; an engine gap is a note for an issue, never an engine patch.
P0–P15 only have to compile warning-free in the files they touch and write the `--check` suites for
their area; P13 and P17 run them.

| Phase | Name | Scope |
|---|---|---|
| P0 | Refresh plan | Re-verify anchors against master, mark landed engine work, this phase list |
| P1 | Scaffold | `premake5.lua` (§7), `main.cpp`, `HearthvaleLayer`, `LaunchOptions` per §3.6, `CheckReport.h`, `GameTuning.h`, the art registry, the Inter font (§2.7) |
| P2 | Sim data core | `TileGrid` + grid hash; def tables: 12 crops, 20 buildables, items, 4 villagers, boards 4 × 4; the seeded 96 × 96 valley; `Calendar` 6:00–2:00 in 12 real minutes, 4 seasons × 14 days; the fixed-step `ValleyDirector` per §3.2 |
| P3 | ValleyWorld | Grid → entities, 16 × 16 cells, `Static`, a dirty-cell queue, colour-only watered soil, a rebake counter, `--no-static` / `--no-instancing` / `--no-cull` |
| P4 | Farmer, camera, sky | Controller and animation, the three-quarter camera rig, sun/moon/ambient/fog curves |
| P5 | Build and clear | Target tile, ghost, placement rules and costs, place/remove/rotate, axe, pickaxe, scythe |
| P6 | Crops, weather, day roll | Tilling, seeds, watering, growth, regrowth, seasons, weather, sleep and the day roll, sprinklers |
| P7 | Economy logic | Inventory, prices, shipping bin, shop, board requests and the festival condition |
| P8 | DingoUI HUD | HUD, hotbar, inventory, build palette |
| P9 | Menus | Title, save slots, pause, options, dialogue, shop, board, credits |
| P10 | Villagers and animals | Schedules, dialogue, gifts; animals in pens, daily produce |
| P11 | Lights, effects, audio | Lamps and windows under a light LOD, particles, buses, music and ambience (synthesized placeholders) |
| P12 | Saves | Schema, version and migration, slots, autosave at the day roll, `.bak` fallback |
| P13 | Autoplay and checks | The farmer bot (§3.5), `--days`, every §5 suite wired and run |
| P14 | Real art | Swap the registry's primitives for the Kenney and KayKit files of §2.7 |
| P15 | Packaging and docs | §7 checklist, README, credits, notices, release workflow |
| P16 | Fresh review | One fresh review agent over the whole diff; fix High and Medium |
| P17 | Cloud test run and balance | `--check` headless and under Xvfb, `--days=56` logs, tune `GameTuning.h` |
| P18 | Owner GPU sign-off | The owner runs the §6 GPU, pad and playthrough checks on Windows |

**Placeholder art first.** Every visual goes through an **art registry**: an id maps to either a
model path (with its material kind: atlas or `BaseColor`) or an engine primitive plus a colour. Until
P14 every id is a primitive (`Renderer3D` box, sphere and the like) with a colour, so P3–P13 never
depend on a model. P14 changes the registry's entries, not game code.

**GPU-free simulation.** Grid, valley generation, calendar, crops, economy, board and saves are plain
C++ that never touches the renderer, a texture, a model, a font or DingoUI, so `--check
--graphics=headless` runs their suites (§1.6, issue #248). Anything that creates entities with
meshes, reads renderer statistics or declares UI checks `Application::Get().IsHeadless()` and stays
off.

### 11.2 P0 (2026-10-15): plan refreshed

- Re-read every anchor in §1, §3, §5, §7 and §10 on `feat/1-0-hearthvale` (master `f414ba0` plus
  the Kenney packs, `7f4c1e0`) and fixed the stale ones in place. Added §1.3 DingoUI, §1.4 culling,
  instancing and static, §1.5 saves and settings, §1.6 headless; renumbered the scale reference
  to §1.7. Rewrote §2.7 from the files on disk.
- **Landed and marked**: E1 (`SubMesh::BaseColor`), E2, E3, E4 (`GetEntityFromBody3D`), E5
  (`OnFixedUpdate`), U5 (DingoUI focus navigation, `docs/game-ui.md:236-291`), v1.0 P9 (the API pass:
  `OnFixedUpdate`, `Entity::SetActive`, `DirectionalLightComponent::Ambient` deprecated to 0,
  `EnableImGui`/`OnImGuiRender`, `CreateApplication` no longer calls `Initialize`;
  `.claude/plans/2026-10-08-v1.0-plan.md:1746`).
- **Assumptions that changed** (later phases must know):
  - The scene's static bake cell is a fixed 32 m, not the game's 16 × 16 (§1.4, E9).
  - Baked meshes never instance: per kind, `Static` or instanced (§3.3).
  - Static change detection is bitwise: write identical colour values (§1.4).
  - Nature Kit is colour-only and multi-material; Cube Pets' clips don't load (E8); only two
    KayKit Adventurers; no watering-can, scythe or item-icon art (§2.7).
  - Headless: no DingoUI, renderers or GPU assets; 60 real updates a second, so `--days` steps its
    own loop (§1.6, §3.6). `--save-dir` and `--graphics` are engine flags.
  - The UI pad check can't drive focus (E7); it checks a screen model (§5).
  - No `pwsh` in the cloud: the audio generator is Python.
  - Animals are in (HD3 resolved by Cube Pets, CC0).

### 11.3 P1 (2026-10-15): scaffold

- `examples/Hearthvale/premake5.lua` (modelled on Marionette's: `distributionSymbols()`, `copyAssimpRuntime()`,
  `exeMetadata`, `embedPackKey()`), its own `dpak.key`, `icon.png`/`icon.ico` (green tile "H"; the row was
  added to `scripts/icons/icons.txt`), and `include "examples/Hearthvale"` in the root `premake5.lua`.
  Not yet done (P15): `build-release.yml`, `.vscode` entries, packaging.
- `src/main.cpp`: `HearthvaleApplication`, `ParseVSync` (as Marionette; default off under `--autoplay`/`--perf`),
  asset root by `FindDirectoryUpward("assets")`, `UpdateInBackground = IsScripted(options)`. No hot-reload flags.
- `src/LaunchOptions.{h,cpp}`: every §3.6 flag except the engine's `--graphics`/`--save-dir`: `Check, Autoplay,
  Days, FixedDt, StepsPerFrame, Seed, Day, Season/HasSeason, TimeHours (negative = morning), Weather, Load,
  LayoutDemo, Perf, Freeze, NoPost/NoShadows/NoParticles/NoFog/NoInstancing/NoStatic/NoCull`. `--days` implies
  `--autoplay` and `--fixed-dt=1/60`; `--steps-per-frame` needs a fixed delta; `--perf` needs `--autoplay`.
  `LaunchSeason`/`LaunchWeather` are launch-only enums; P2's calendar maps them to its own.
- `src/GameTuning.h`: constants only (sim step, 96 grid, 16 tile cell, day 6:00-26:00 in 720 s, 14 days x 4
  seasons, camera, sun/ambient). Scene name `SCENE_VALLEY`.
- `src/Look/ArtRegistry.{h,cpp}`: `Art::Find(id)` and `Art::Spawn(scene, id, name, position)`; entries are
  `ArtEntry{Id, Primitive, Color, Size, ModelPath}` (ids `ground, farmer, tree, rock`, all primitives; P14 fills
  `ModelPath`). `Spawn` returns an empty entity headless.
- `src/HearthvaleLayer.{h,cpp}`: one `Valley` scene (ambient, shadowed sun, ground, farmer box, perspective camera
  at 45 degrees); the scene is empty headless. `OnUpdate` steps `m_SceneManager` with `FixedDt`/`StepsPerFrame`
  and renders. `--check` runs the (empty) suite list, logs `[Checks] N passed, M failed`, and closes when headless.
  Suites go where `m_ChecksDone` is set.
- `src/Checks/CheckReport.h` is Marionette's (the plan's `Debug/` folder is `Checks/`: .gitignore ignores any `Debug/`; Autoplay goes there too). `assets/fonts/Inter-Bold.ttf` + `assets/LICENSE-Inter.txt` copied.
- Verified: Debug build has no warnings in these files; the empty scene runs 20 s under xvfb with no VUID/LOG ERROR
  (one Vulkan "shader validation cache missing" info warning, environmental); `--check --graphics=headless` exits 0.

### 11.4 P2 (2026-10-15): sim data core

All plain C++ (no renderer, model, font or UI); headless-safe. Paths are under `examples/Hearthvale/src/`.

- `Sim/Rng.h`: `SplitMix64`, `HashMix(a, b)`, xoshiro256** `Rng` (`NextInt(lo, hi)` inclusive, `Chance(permille)`), integer-only so every platform agrees. `Sim/SimTypes.h`: `Season`, `Weather` (Clear/Rain/Snow) and `ToString`; `LaunchSeason`/`LaunchWeather` are mapped to these in `MakeDirectorConfig`.
- `World/TileGrid.{h,cpp}`: `Tile{GroundState, Occupant id, CropRecord{Def, Stage, DaysWatered, PlantedDay}}`; `Ground` = Grass/Tilled/Watered/Path/Water/Blocked (watered is a ground state); `Occupant{Id, Kind(Decoration|Building|Landmark), DefId, X, Y, W, H, Rotation, Variant}` stored by ascending id (ids never reused). `AddOccupant` enforces the player rule (`CanOccupy`: Grass/Tilled/Watered/Path, no crop, in bounds, all tiles empty); `AddOccupantUnchecked` is for generation. A crop lives in the tile record, not as an occupant. `Hash()` is FNV-1a over explicit little-endian fields; an empty 4x4 grid is `0x70F76B4CC90685D5` (cross-checked in Python).
- `World/ValleyLayout.h`: all layout constants. Farm clearing 40x40 at (10,28), plot 12x12 at (24,42), square 16x16 at (64,40), main road y 47-48 from x 36 to 64, pond ellipse at (76,74), village zone, `k_Landmarks` (FarmHouse, ShippingBin, Shop, Carpenter, MayorHouse, Cottage, NoticeBoard) with footprint and door tile. Villager homes and later waypoints reference these.
- `World/Valley.{h,cpp}`: `GenerateValley(grid, seed)`: rim (random-walk depth 6-9 on each edge, `Blocked`), paths, jittered pond, landmarks, then row-major decoration scatter by zone (rim, overgrown clearing, village, meadow; mixes and per-mille densities in `GameTuning.h`). Three independent RNG streams from `HashMix(seed, stream)`. About 4,000-4,100 decoration objects per seed. Decoration may sit on `Blocked` rim ground; cosmetic kinds (`Grass`, `Flower`) are meant to be cleared for free by building or tilling (P5/P6 must do this).
- `Defs/`: `CropDefs` (12, `GrowthDays` = watered days to ripe, `RegrowDays` > 0 for strawberry and tomato, `CropStageFor`), `ItemDefs` (`ItemId` layout: 5 tools, wood/stone/fibre, egg/milk/honey, 12 seeds, 12 harvests; `SeedItem`/`HarvestItem`; seeds and harvests derive prices from the crop table), `BuildDefs` (20; `Cost{Coins, Wood, Stone, Fibre}`, `RefundPermille`, `PlanPrice` at the carpenter, `GroundOnly` for the two paths, `Blocking`, `WaterRadius`, `RotatedFootprint`), `DecorDefs` (tool and yield per kind), `VillagerDefs` (Mira, Bram, Odette, Pip; home landmark, 4 liked-gift slots padded with `ItemId::Count`), `BoardDefs` (one board per season, 4 requests each: Ship/Build/Gift/EarnCoins, `InSquare` flag, coin reward). Prices live in the tables; global knobs (refund share, weather per-mille, densities) in `GameTuning.h`.
- `Farm/Calendar.{h,cpp}`: integer ticks only: `DAY_TICKS` = 43200 (12 min at 60 ticks/s), `TICKS_PER_MINUTE` = 36, clock 06:00-02:00. 14 days x 4 seasons; absolute day 0 = spring 1 of year 1. `WeatherForDay(seed, day)` is a pure function of `HashMix(seed, day)` (day 0 always clear; winter snows, other seasons rain). `ForceWeather` lasts one day.
- `ValleyDirector.{h,cpp}` (at the src root): owns the `TileGrid` and `Calendar`; `Advance(dt, maxSteps)` turns real time into whole 1/60 s steps (`Step()` is one). Step order: Input, PlayerIntent, ToolsAndPlacement, [sleep request or clock tick, day roll], Calendar-stage handlers, Villagers, Animals, Sky, VfxAudio, Camera, Ui. Hooks: `OnStage(SimStage, fn)`, `OnDayRoll(DayRollStep, fn)`. Day roll order: Crops, Sprinklers, Shipping, (calendar rolls the day and weather), Weather, Board, Autosave; reason `Sleep` or `PassOut`. Crops have no per-step stage (they grow only in the day roll). `RequestSleep()`, `SetTimePaused()`. `MakeDirectorConfig(LaunchOptions)` maps `--seed/--day/--season/--time/--weather/--freeze` (a time before 06:00 means after midnight). `HearthvaleLayer` owns it and calls `Advance` inside its fixed-step loop.
- Checks (`Checks/Checks.{h,cpp}`, `DefChecks.cpp`, `GenerationChecks.cpp`, `CalendarChecks.cpp`): `RunSimChecks` is called by the layer's `--check` runner; each suite logs `<Name> checks: N passed, M failed` through `SuiteScope`. Defs 16, Generation 64 (seeds 1, 2, 3, 4242 plus grid rules and hash), Calendar 30 (incl. Director order, time, config, determinism). `--check --graphics=headless` logs 110 passed, 0 failed.
- Not done by design: crops, economy, villager schedules, save schema (later phases). The director has no `Script`/scene tie yet; it is a plain object owned by the layer.

### 11.5 P3 (2026-10-15): ValleyWorld

Paths under `examples/Hearthvale/src/`. GPU side only: the layer creates the world when not headless.

- `World/WorldCells.{h,cpp}` (plain C++, headless-tested): `GroundLook` (Land = grass, tilled and watered; Path; Water; Rim = Blocked), `GroundTop(look)`, `IsSoil`, `MergeGroundRects` (greedy row-major merge of a cell into one-look rectangles; seed 1 gives 327 for 9,216 tiles), and `DirtyCells`: the 16 x 16-tile cell math (`CellOfTile`, `CellOfOccupant` = the footprint's centre tile, `CellRect`) plus a queue that holds each cell once (`MarkTile/Area/Cell/All`, `MarkFarmCells` = cells with a crop or tilled/watered soil, `Drain` ascending).
- `World/ValleyWorld.{h,cpp}`: owns no grid; reads the director's `TileGrid`. `MarkTile/MarkArea/MarkAll` queue cells; `OnDayRoll()` sets a flag; `Sync()` (the layer calls it once a frame after `Advance`, before `OnRender`) drains the queue and rebuilds each cell once. A rebuild touches only what changed: ground rectangles are recreated only when the cell's merge differs; per tile a soil entity (created/destroyed, or recoloured in place) and a crop entity (recreated when def or stage changes); occupants by id (recreated when the occupant record changes, destroyed when gone). So rebuilding an unchanged cell writes identical draw state and the engine rebakes nothing (checked). A day roll queues `MarkFarmCells` plus cells that still hold crop/soil entities (a crop that vanished). `GetLastSync()` (`WorldSyncStats`: cells, created, destroyed, recoloured) and `GetCellsRebuiltTotal()` are the game-side counter; the engine side is `Renderer3D::GetStatistics().StaticCellsBaked`.
- **Static vs instanced, per kind**: ground rectangles, soil, crops, buildables and landmarks are `Static` (few copies, change at edits or the day roll). **Decoration is dynamic** so it instances: ~4,000 copies of a few meshes, and baking them would store each copy's vertices again (a primitive sphere is ~289 vertices x 52 B). Ground and soil cast no shadows. `--no-static` clears `Static` on all of them (`ValleyWorldOptions::Static`); the layer applies `--no-cull`/`--no-instancing` through `Renderer3D::SetCullingEnabled`/`SetInstancingEnabled`.
- **Scene `Static`, not a hand-held `StaticBatch(16)`**: the world maps tile (x, y) to world (x, z) from the origin, so a 16-tile game cell nests in one 32 m engine cell (`static_assert` in `ValleyWorld.cpp`), and every slab's bounds centre is above y = 0 (`GROUND_TOP` etc. in `GameTuning.h`) so ground and what stands on it share an engine cell. A placement rebakes one 32 m cell (about 4 game cells: ~40 ground boxes plus that area's statics). E9 stays a note unless P17 measures the rebake.
- **Colour-only watering, verified**: `StaticGeometry.cpp:91` compares `Color` bitwise; a mismatch calls `StaticBatch::Update` (`StaticBatch.cpp:150-158`), whose `Detach` marks the cell dirty and re-queues the mesh into the same cell, so `PrepareStaticBatch` bakes it once (`Renderer3D.cpp:2846-2861`, `++StaticCellsBaked`). Measured under Xvfb: watering rebakes 1 cell; re-marking an unchanged cell bakes 0. Dry and watered soil are two registry entries (`Art::SOIL`, `Art::SOIL_WATERED`) that must differ only in colour (a World check enforces it).
- `Look/ArtRegistry.{h,cpp}`: entries for ground looks, soil, 8 decoration kinds, 20 buildables, 7 landmarks, 3 growing stages plus 12 ripe crops; lookups `Art::ForGround/ForDecor/ForBuild/ForLandmark/ForCrop(crop, stage)`; `Spawn(..., quarterTurns, scale)` and `SpawnOnGround(..., x, z, groundY, ...)` (centre = ground + half the scaled height). Decoration variants scale by `DECOR_VARIANT_SCALE`. The old `ground`/`tree`/`rock` ids are gone.
- `HearthvaleLayer`: the world replaces the single ground box; farmer at the farmhouse door, camera aimed at the farm plot (P4 replaces both). The day-roll hook is registered by the layer (`DayRollStep::Crops`) and forwards to the world only while it exists. `--check` runs the sim suites, then (windowed) the Static suite one step a frame; `[Checks]` is logged when it finishes, and headless still closes after the sim suites.
- Checks: `Checks/WorldChecks.cpp` (World suite, headless, 16: cell math, queue dedup, farm cells, merge coverage/uniformity/stability, tilling and watering keep the merge, art coverage, soil entries colour-only). `Checks/StaticChecks.{h,cpp}` (`StaticCheckRun`, windowed only, 15): first frame bakes all 9 engine cells and settles; place a scarecrow (1 game cell, 1 engine cell); remove it (same cell, 1); till (1); water (colour only, 1); re-mark unchanged (0); plant in two engine cells (2); a sleep day roll with stages bumped as a stand-in for P6 growth rebuilds only the crop cells (2 of 9); cleanup restores the grid. Skipped with a log line under `--no-static`. Results 2026-10-15: headless 126 passed; Xvfb 141 passed, 0 failed, no VUID or LOG ERROR.
- For P5/P6: every grid edit must call `MarkTile`/`MarkArea` on the world (if it exists); crop growth and drying need no marking (the day roll covers farm cells). Soil drying on cells without crops is still covered (soil counts as a farm cell).

### 11.6 P4 (2026-10-15): farmer, camera, sky

Paths under `examples/Hearthvale/src/`. Everything marked GPU is built by the layer only when not headless; the pure files are in `--check`.

- **Intent** (`Life/PlayerIntent.h`, `PlayerInput.{h,cpp}`): `PlayerIntent{Move (x right, y forward, camera-relative), pulses Use/Interact/HotbarPrevious/HotbarNext/HotbarSlot/RotatePlacement/ToggleInventory/ToggleBuild/Pause/CameraSteps/CameraZoomWheel, held CameraZoomAxis}`. `PollPlayerIntent` runs once a frame before `Advance` (not under `--autoplay`; P13's bot fills the same struct). Pulses are or-ed in and cleared by the `Ui` stage handler after every sim step, so a press in a frame with no step survives. `CombineMove` merges keys and stick (pure).
- **Mapping deviation**: plan 2.3 gave E to both interact and camera rotate. Keyboard interact is **F** (also right click, pad X); camera is Q/E and D-pad left/right. Use = left click/Space/pad A, rotate placement R/pad Y, inventory Tab/pad Back, build B, pause Esc/Start, hotbar LB/RB or wheel or 1-9, zoom = Ctrl+wheel or right stick Y.
- **Player** (`Life/Player.{h,cpp}`, GPU): entity "Farmer" = Transform3D + CharacterController3D (tuning in `GameTuning.h`, `PLAYER_*`). `Step(intent, grid, cameraYaw, dt)` runs in the `PlayerIntent` stage: camera-relative direction, `PlayerMath::ResolveMove`, hand-kept gravity (Candlewick pattern, clamped at `PLAYER_TERMINAL_Y`), `ApproachAngle` yaw, `SetRotation`. Placeholder body = `Art::FARMER` box + `FARMER_HEAD` + `FARMER_FACE` children; model mode = SkinnedMeshRenderer + Animator on the same entity, scaled to `PLAYER_MODEL_HEIGHT` (1.5 m) from the vertex max y, Idle_A / Walking_A switched with `PLAYER_ANIM_FADE`, walk playback speed `PLAYER_WALK_ANIM_SPEED` (untuned, no GPU here). `--farmer=model` selects the model (default box); a failed load falls back to the box with a warning.
- **Walkability is grid-based, not collider-based**: ValleyWorld has no colliders, so `Player::SpawnFloor` adds one static box under the whole valley (top at `GROUND_TOP`) and `Life/PlayerMath.{h,cpp}` (pure) keeps the capsule out of water, the rim, trees/pines/bushes/rocks/stumps, blocking buildings and landmarks: `ResolveMove` tests the x and z parts of one `PLAYER_BLOCK_LOOKAHEAD` step separately (wall sliding) and lets a disc that already overlaps a blocked tile walk out. A buildable's `Blocking` flag (BuildDefs) is what P6 changes. Camera-relative maths: forward `(-sin yaw, -cos yaw)`, right `(cos yaw, -sin yaw)` in (x, z).
- **KayKit assets** (`assets/kaykit/`): `characters/Barbarian.glb` + `barbarian_texture.png`, `animations/Rig_Medium_General.glb` (Idle_A) and `Rig_Medium_MovementBasic.glb` (Walking_A), both loaded clips-only; the two LICENSE files. `Look/FarmerAssets.{h,cpp}` (GPU) loads them, makes the lit material (freed in its destructor; the layer frees it after the scene is cleared) and exposes `IsUsable()`.
- **CameraRig** (`Look/CameraRig.{h,cpp}` GPU, `Look/CameraMath.{h,cpp}` pure): runs in the `Camera` stage. Pitch 45, yaw steps of 90 degrees eased at `CAMERA_YAW_SPEED_DEG`, distance 14 (8..26) from the held axis and wheel notches (positive = in), focus lag `CAMERA_LAG` on the farmer's feet + 0.7. Post (Soft tone map, bloom, AO) on the camera entity unless `--no-post`. It reads the controller position of the previous scene update (one frame behind, accepted). The AudioListener moved here from the layer.
- **Sky** (`Look/SkyCurve.{h,cpp}` pure, `Look/Sky.{h,cpp}` GPU): six `SKY_KEYS` (hours 6, 9, 12, 16, 20, 26; elevation 10, 38, 65, 40, -4, -40) linearly interpolated, elevation capped at `SKY_ELEVATION_MAX_DEG` 65, azimuth east to west across 6..20. `SkyBlend` quantizes both angles to 0.25 degrees and eases each new step in over 1 s (so cascades do not crawl); everything else (colours, intensity, fog) is smooth. Sun, moon (opposite the sun, intensity from `MOON_INTENSITY` fading in below the horizon, no shadows), `AmbientLightComponent` entity (not the deprecated field), a Linear `FogComponent` with `UseClearColor` (skipped under `--no-fog`), clear colour = sky colour. The sun stops casting shadows below `SKY_SHADOW_MIN_ELEVATION_DEG` and fades out over `SKY_SUN_FADE_DEG`. Weather scales sun and fog (rain 0.55/0.45, snow 0.75/0.65); morning mist (6:00 to 9:00, 0.7 weight when clear, 1 in rain) shortens fog. The `Sky` stage calls `Update(dt, calendar hours, weather)`. All numbers are in `GameTuning.h`; the old `COLOR_SKY`, `SUN_*` and `AMBIENT_*` constants are gone.
- **Clock** (`Look/ClockHud.{h,cpp}`, GPU): a DingoUI top-left root "spring 3  06:00" with `fonts/Inter-Bold.ttf`; temporary, P8 replaces it.
- **Layer**: registers the four stage handlers in `OnAttach` (`RegisterStages`); `BuildValleyScene` builds sky, world, floor, farmer at the farmhouse door (`DoorY + 1.5`), camera, clock. `OnDetach` releases the pieces, clears the scene, then frees the farmer assets (material before the renderer).
- **Checks**: `Checks/SkyChecks.cpp` (Sky, 26: key table, 0.25 quantization, monotone stepped elevation each side of noon, cap incl. an 80 degree key, 10/65/negative anchors, azimuth, blend half/full time and no retarget on the same step, a whole day never moves more than a step's worth per tick, weather and mist ordering, moon, bounds for every minute and weather) and `Checks/PlayerChecks.cpp` (Player, 20: camera-relative move, yaw, `CombineMove`, wall slide, escape, spawn clear, rim/pond/farmhouse blocked, camera step/ease/zoom/eye). `--check --graphics=headless`: 172 passed, 0 failed (was 126).
- Verified (Linux, llvmpipe): Debug build without warnings in these files; windowed 20 s with the box and with `--farmer=model --time=12:00 --weather=rain`, and windowed `--check --farmer=model` (187 passed), with no VUID/LOG ERROR/crash. Not verified: how anything looks, walk-cycle speed vs ground speed, shadow stability, performance, gamepad.
- For later phases: the farmer is `m_Player` on the layer (`GetFeet()`, `GetYaw()`, `IsMoving()`, `Teleport`); the intent is `m_Intent`; add handlers to the existing `PlayerIntent`/`ToolsAndPlacement` stages after `RegisterStages`' ones. Pulses clear in the `Ui` stage handler that `RegisterStages` adds first, so any later `Ui`-stage handler sees them already cleared; read pulses in earlier stages (or move the clear to the end of `Step`).

### 11.7 P5 (2026-10-15): build and clear

Paths under `examples/Hearthvale/src/`. Rules and targeting are plain C++ (in `--check`); `BuildGhost` and `BuildMenuHud` are GPU/DingoUI and built only when not headless.

- **Folder is `Placing/`, not `Build/`**: the repo `.gitignore` ignores any `[Bb]uild/` directory, so a `Build/` source folder would never be committed.
- **Inventory** (`Placing/Inventory.{h,cpp}`): placeholder counters (`Count/Add/Remove`, `GetCoins/AddCoins/SpendCoins`). P7 replaces the body and keeps the surface. The starting inventory is empty.
- **Rules** (`Placing/Placement.{h,cpp}`): `FootprintOf` (rotation swaps width/height), `CheckPlacement` (no side effects; order: bounds, ground, occupied/crop, cost -> `PlaceResult`), `PlaceBuilding` (clears cosmetic decoration for free, charges, adds an occupant, or sets `Ground::Path` with `Tile::Built = BuildId + 1`), `RemoveBuilding` (refund = floor(cost * permille), refuses generated paths, landmarks, decoration and crops). Every edit returns its dirty rect; the caller marks `ValleyWorld`, so a fence queues one 16x16 cell and a barn across a cell corner queues four, with one rebake in `Sync()`.
- **`Tile::Built`** (`World/TileGrid.{h,cpp}`): added so a dirt path and a stone path refund differently. Hashed only when non-zero, so the pinned hash is unchanged. Path refunds floor to 0 at the current prices.
- **Tools** (`Placing/Tools.{h,cpp}`): `UseClearingTool(grid, inventory, tool, x, y) -> ToolOutcome{Nothing, NotMine, WrongTool, Cleared; Kind, Needed, Yield, YieldCount, Dirty}`. Axe: trees/pines/stumps give wood; pickaxe: rocks give stone; scythe: grass/fibre plants give fibre and also clears cosmetic grass and flowers for nothing (`Needed = Scythe` for cosmetic decor). Swings are instant (no animation, no durability or energy).
- **Targeting** (`Placing/Targeting.{h,cpp}`): `Targeting::Resolve(AimInput)` -> `Aim`. Facing aim = the tile in front of the farmer, yaw snapped to a cardinal step. Mouse aim = `Ray::IntersectGroundPlane`, valid within `TARGET_MOUSE_REACH_TILES` (3); mouse aim wins only while the mouse moves, then facing takes over again. A multi-tile footprint is placed in front of the farmer with facing aim and centred on the hovered tile with mouse aim; a property check confirms every buildable, rotation and facing covers the target tile and never the farmer's own tile (`OriginInFront`, `OriginCentred`). A blocking building can still be placed on the farmer's tile when mouse-aimed (`PlayerMath` walks out of it).
- **BuildController** (`Placing/BuildController.{h,cpp}`): modes `Tools`/`Build`; `BUILD_TOOLS = {Hoe, WateringCan, Axe, Pickaxe, Scythe}` (default Axe); `Step(intent, aim)` reads Use, Interact (remove in Build mode), RotatePlacement, ToggleBuild and HotbarPrevious/Next/Slot (slots 1-5 select tools, standing in for P7's hotbar; in Build mode they pick the buildable). A dirty callback tells the layer which area to `MarkArea`. `GetMessage()/GetMessageSerial()` feed the HUD. Direct `Place()/Remove()/UseTool()` are what the checks call. Hoe and watering can are not wired (P6).
- **Ghost** (`Look/BuildGhost.{h,cpp}`, GPU): translucent material (`SetTranslucent(true)`, freed in the destructor after the scene is cleared), a target-tile marker (`Art::TARGET_TILE`) and a buildable ghost entity respawned when the buildable changes, tinted `GHOST_COLOR_VALID/INVALID`. `Release()` before the scene clear.
- **Menu** (`Look/BuildMenuHud.{h,cpp}`, DingoUI, temporary, P8 replaces it): bottom-left text panel with resources, mode/tool line, nine buildable rows with costs, preview status and a fading message.
- **Layer**: the `ToolsAndPlacement` stage handler calls `UpdateMouseAim` then `BuildController::Step`; marks dirty areas on `ValleyWorld`; `Player::SetYaw` was added for remove-facing. Teardown order: ghost, HUD, scene clear, then material.
- **Not done / for later**: `PlanPrice` gating is not enforced (P7 should gate unlocked buildables). There is no 2x2 buildable, so multi-tile checks use the coop, barn and greenhouse.
- **Checks**: `Checks/PlacementChecks.cpp`, suite "Placement" (86): footprints and rotation, bounds/ground/occupied/crop/cost rejections, multi-tile, ground-only paths, refund shares, decoration clearing, dirty-rect counts, one rebake per edit, tools and yields, targeting (facing, mouse, reach, footprint property), controller flow. `--check --graphics=headless`: 258 passed, 0 failed (was 172).
- Verified (Linux, llvmpipe): Debug build with no warnings in these files; headless `--check` 258/0; windowed `--check` under xvfb 273/0 with no VUID/LOG ERROR; a throwaway build-mode ghost smoke run showed no errors. Not verified: how the ghost, tints or menu look, mouse aim with a real mouse, gamepad. No engine gaps hit.

### 11.8 P6 (2026-10-15): crops, weather, day roll

Paths under `examples/Hearthvale/src/`. The sim is plain C++ (in `--check`); `FadeOverlay` is GPU/DingoUI and built only when not headless.

- **Farm actions** (`Farm/Crops.{h,cpp}`): `TillTile`, `WaterTile`, `PlantSeed`, `HarvestTile`, `PickSeed`, each returning `FarmOutcome{Result, Dirty, ...}` (the caller marks `ValleyWorld`; watering is colour-only through `MarkTile`). Tilling clears cosmetic decoration and works on a greenhouse floor. Planting needs tilled or watered soil, a seed in the inventory and the crop's season (greenhouse tiles accept any season). Harvest of a regrowing crop resets `DaysWatered = GrowthDays - RegrowDays`.
- **Growth** (`GrowCrops(grid, endingDay, weather)`): a crop grows when its tile is `Watered`, or it is raining and the tile is outdoors. Snow on an outdoor tile grows nothing (counted as `Snowed`). A crop withers (the tile stays tilled) when `!CanGrowIn(def, SeasonOfDay(endingDay + 1), greenhouse)`. Greenhouse footprint tiles (`IsGreenhouseTile`) are exempt from season, rain and snow; they need the can or a sprinkler. `DrySoil` turns watered soil back to tilled; `WaterWithSprinklers` waters tilled tiles within `WaterRadius` of each sprinkler's footprint; `WaterOutdoorSoil` is the rainy-dawn watering.
- **Day-roll handlers** (`Farm/FarmSim.{h,cpp}`): Crops step = pass-out penalty, `GrowCrops` with the ending day's weather, `DrySoil`. Sprinklers step = `WaterWithSprinklers` (so they water for the new day, after the soil dried). Weather step = if the new day rains, `WaterOutdoorSoil`. `GetLastRoll()` returns the counts for HUD and checks.
- **Pass-out**: the 10% coin loss (`PASSOUT_COIN_LOSS_PERMILLE`) is taken at the Crops step, so it lands BEFORE P7's Shipping payout. P7 should keep that order or decide otherwise and record it.
- **Sleep and fade** (`Farm/SleepFlow.{h,cpp}`, `Look/FadeOverlay.{h,cpp}`): `SleepFlow` is a pure state machine `Idle -> Out -> Hold -> In` (`FADE_OUT/HOLD/IN_SECONDS`); time is paused during it and input is blocked (`BlocksInput()`; the layer feeds an empty intent). The roll fires at the end of Out via `ValleyDirector::RequestSleep()`. A 2:00 pass-out enters Hold directly. `FadeOverlay` is a stretched DingoUI root (`FADE_UI_LAYER` 100, input not blocked) drawn from `GetAlpha()`.
- **Controls** (`Placing/BuildController`): Hoe tills, Can waters, Scythe harvests a ripe crop (else clears fibre/grass); Interact (F) in Tools mode harvests a crop, plants the selected seed in tilled soil, or sleeps at the farmhouse footprint/door. `T` / DPad Up cycles the seed (`PlayerIntent::CycleSeed`). The can has no capacity or energy.
- **Weather visuals** (`Look/WeatherFx.{h,cpp}`): a stub for P11. `SetWeather` returns true on change and logs `[Weather] <name>`; the layer calls it from a VfxAudio stage handler. The clock HUD appends the weather name.
- **Deviations**: `Greenhouse` is now `Blocking = false` (so the farmer can enter and till inside). `STARTER_SEED_COUNT` (10 of each in-season seed) is put in the inventory in `OnAttach` as a placeholder; P7 should remove it when seeds are sold. The `Static` suite's day-roll check now waters its two tiles instead of bumping `Stage` by hand. The Placement hoe check now expects a tilled tile.
- **Not done / for later**: P14 greenhouse art must hide or show crops inside (the footprint is walkable). Fade overlay and the seed line in `BuildMenuHud` unverified visually.
- **Checks** (`Checks/GrowthChecks.cpp`, suite "Growth", 91): actions and refusals, all 12 crops ripe exactly on their `GrowthDays` and not before, a dry day delays by one, rain, rainy dawn, snow, season withering and greenhouse exemption, regrowth, sprinklers (3x3 and 5x5), pass-out, the sleep state machine, controller flow, determinism (same seed twice, handler order C-S-W). The fixture forces the dawn weather to Clear (`Farm::Dawn`) via a Weather-step hook registered before `FarmSim`, so a natural rainy dawn cannot water the plot; tests of rainy dawns `reset()` it. No wall-clock or `std::rand`.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 349/0; windowed `--check` under xvfb 364/0, no VUID/LOG ERROR. Not verified: how the fade, rain or crops look, gamepad. No engine gaps hit.

### 11.9 P7 (2026-10-15): economy logic

Paths under `examples/Hearthvale/src/`. All plain C++ (in `--check`); no screens, no save format.

- **Inventory** (`Placing/Inventory.{h,cpp}`, rewritten): `INVENTORY_SLOTS` = 33 stack slots (`HOTBAR_SLOTS` 9, then the 6 x 4 backpack), per-item `StackMax` from `ItemDef`. `Add` returns how many fitted (partial stacks first, then empty slots); tools and seeds prefer the hotbar, everything else the backpack, each spilling into the other. `Remove` empties the last slots first. `CanAdd`, `CanAddAll(span)` (stacks that compete for slots) and `Room` let callers refuse before they change anything. Hotbar: `SelectSlot`, `CycleSelection` (skips empty slots), `GetSelected`, `MoveSlot` (merge or swap). Coins (`AddCoins`/`SpendCoins`) and known building plans (`KnowsPlan`/`LearnPlan`/`LearnAllPlans`) live on it too, so existing call sites keep their signatures. `GiveStarterLoadout()` applies `STARTER_COINS` and `STARTER_LOADOUT` (`GameTuning.h`: 500 coins, the five tools, parsnip 8, radish 4, strawberry 2) and selects the axe. `STARTER_SEED_COUNT` is gone.
- **Full pack refuses, item stays**: `FarmResult::InventoryFull` (harvest, crop stays), `ToolResult::InventoryFull` (clear, the decoration stays), `RemoveOutcome.InventoryFull` (a refund checked with `CanAddAll`, the building stays), `TradeResult::InventoryFull`. Coins are not slots, so payouts never fail.
- **Plan gating**: `PlaceResult::PlanLocked` from `CheckPlacement` (after occupancy, before cost) when the plan is unknown; `PlanPrice == 0` buildables are known from the start.
- **BuildController**: tools are real hotbar items now. `GetHeld()` is the selected slot's item (`ItemId::Count` when empty), `GetTool()` only if it is a tool. Hotbar slot keys and next/previous drive `Inventory::SelectSlot/CycleSelection`; `SyncSeedToHeld()` plants the held seed. `BUILD_TOOL_COUNT` is gone; `BUILD_TOOLS` stays as the tool list. Not added: an Interact-at-the-bin hook (P8/P9 call `ShippingBin::Ship`).
- **Shop** (`Farm/Economy.{h,cpp}`): `ShopStock(Merchant)` (shopkeeper: everything with `BuyPrice > 0`; carpenter: every `PlanPrice > 0` plan), `ShopBuyItem`, `ShopBuyPlan`, `ShopSellItem`, all-or-nothing, returning `TradeResult` (`CannotAfford`, `InventoryFull`, `NotForSale`, `AlreadyKnown`, ...). The shop pays `SHOP_SELL_BACK_PERMILLE` (50%, min 1) of the ship price at once; the bin pays 100%.
- **Shipping bin**: `ShippingBin::Ship/Retrieve/Settle`. Goods wait in the bin; `Settle` (called by `EconomySim` at the Shipping step) pays and empties it.
- **Day-roll order**: Crops (pass-out loss of 10% of coins in hand, `FarmSim`) -> Sprinklers -> Shipping (`EconomySim`: bin pays, `NoteShipped` + `NoteEarned` with the ending day) -> calendar rolls -> Weather -> Board (`EconomySim`: `CommunityBoard::Settle` for the new day, rewards banked) -> Autosave. The pass-out loss is taken before the payout, so the night's sales are never taxed (decided here, as P6 asked).
- **Board** (`Farm/Board.{h,cpp}`): `CommunityBoard`, 4 boards x 4 requests from `BoardDefs`. A board opens when its season starts (`day >= board * DAYS_PER_SEASON`) and stays open until done; every board is open once in the sandbox. Progress: Ship and EarnCoins by delivery at the Shipping step (EarnCoins counts coins the bin paid, not shop sales), Gift via `GiveGift(inventory, board, villager, item, day)` (consumes the item, counted only when a request wants exactly that), Build by recounting standing buildings in `Settle` (`InSquare` counts occupants whose origin is inside `VILLAGE_SQUARE`; ground-only buildables count by `Tile::Built`), capped at the target. Rewards are paid once, by `Settle`, in `BoardReport`. All 16 done -> `FestivalPhase::Pending` (once). `AcknowledgeFestival(grid)` -> `Held` (sandbox), tier +1, every request re-armed: count and reward x (1 + tier), Build baseline = buildings already standing. The festival scene itself (and calling `AcknowledgeFestival`) is for P15/H6; there is no "before the end of winter" deadline in the sim (decided: boards stay open until done).
- **GameState** (`Farm/GameState.{h,cpp}`): `GameState{Pack, Bin, Board}` (the layer owns one, `m_State`; `FarmSim`, `EconomySim` and `BuildController` take its pieces) and `HashGameState(director, state)` -> `GameStateDigest{Grid, Pack, Bin, Board, Time, All}` for the Save and Determinism suites. `Sim/Fnv.h` is the `Fnv1a` helper moved out of `TileGrid.cpp` (the empty-4x4 hash is unchanged). No save format yet.
- **Existing checks changed**: the Placement and Growth fixtures hold the five tools, `LearnAllPlans()` and 297 of each resource (500 of each no longer fits 33 slots with 12 seed stacks); the controller block selects the axe explicitly and compares wood and stone against baselines. `BuildMenuHud` shows `GetHeld()` ("empty hands" when none).
- **Checks**: `Checks/EconomyChecks.cpp` (suite "Economy", 56: slots, stacking, max stack, hotbar/backpack fill, `CanAddAll`, loadout, prices all positive, shop refusals/purchases/sell-back, plan gate, full-pack refusals for harvest/clear/refund, selling pays at the day roll not before, pass-out order, hash determinism and sensitivity) and `Checks/BoardChecks.cpp` (suite "Board", 33: opening, ship/earn/build/square/gift progress, paid once, festival trigger, sandbox scaling and baseline, hash). `Checks/EconomyRig.h` is the shared headless director + `GameState` rig.
- **For later**: the only starter seeds are spring ones, so `--season=summer` starts without in-season seeds until the shop screen exists (P8/P9); a screen for the bin, shop, inventory and board; `AcknowledgeFestival` caller; the board `Hash` and `Pack.Hash` fields are what the save must round-trip.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 438/0; windowed `--check` under xvfb 453/0 (stays open until the timeout), 15 s windowed run with no VUID/LOG ERROR. Not verified: how the HUD line looks, gamepad. No engine gaps hit.

### 11.10 P8 (2026-10-15): DingoUI HUD and screens

Paths under `examples/Hearthvale/src/`. The logic is plain C++ in `--check`; only the drawing needs the renderer. `Look/ClockHud.*` and `Look/BuildMenuHud.*` are deleted, and so are the `HUD_*` constants in `GameTuning.h` (now `UI_*`: layers, colours, `UI_TOAST_SECONDS`). Pixel sizes live in `UiMetrics` (`Ui/ScreenLayouts.h`), not GameTuning, because the pure layouts and the drawing share them.

- **Pure layer** (headless): `Ui/UiLayout.{h,cpp}` (rects, text estimate, `TextFitsBox`, `SplitName`, `CostLines`, `ItemSwatch`, `UiDesignSize` = DingoUI's `ScaleWithScreen` maths), `Ui/ScreenStack.{h,cpp}` (open screens, each remembering the widget label that opened it), `Ui/ScreenLayouts.{h,cpp}` (the inventory, palette and HUD as data: widget labels and panel-relative rects in declaration order, the detail and caption texts, `BuildHudContent`, and a mirror of DingoUI's directional navigation, `FindNavTarget`, `FindTabTarget`, `CountReachable`), `Ui/ScreenHost.{h,cpp}` (the behaviour).
- **ScreenHost** is what P9's screens plug into: add a `ScreenId`, a case in `Update`/`GameUi::Draw`, a layout. `Update(intent, blocked)` runs once a frame after the intent is polled: Tab/View toggles the inventory, B opens the palette in Tools mode (in Build mode B still reaches `BuildController`, which returns to Tools), the bumpers swap inventory and palette, digits equip a hotbar slot. While a screen is open (or changed this frame) every pulse is cleared, and the layer's `ActiveIntent()` returns the empty intent, so the farmer, the placement, the camera and the hotbar stop. Time stops for HD5 through `ValleyDirector::SetTimePaused` on the first open and is restored to its earlier value on the last close (so `--freeze` survives). One open/close change per frame (`m_Changed`) because Esc arrives both as the `intent.Pause` pulse and as `DingoUI::IsBackPressed()`.
- **Inventory**: 9 hotbar slots, 6 x 4 backpack placed under hotbar slots 1..6 (`GRID_START_COLUMN`) so Up/Down between the rows stays in a column. A on a stack picks it up, A on another slot moves it (`Inventory::MoveSlot` merges or swaps), A on the same slot or B cancels. After a move the focus goes to the destination slot (`TakeFocusRequest`).
- **Build palette**: replaces the text menu. 7 category rows in `BuildCategory` order, each cell a button plus a cost caption ("Plan needed, N coins" for a locked plan; red when it cannot be afforded). A on a cell enters Build mode with it and closes; a locked plan is refused with the toast "Buy the plan at the carpenter". Tabs: Inventory, Build, Use tools (back to Tools mode).
- **Navigation**: DingoUI's own focus (D-pad/stick, arrows, Tab, A/Enter/Space, B/Esc through `IsBackPressed`). Roots are Modal and navigable at `UI_SCREEN_LAYER` (10); the HUD roots are `BlocksInput(false)` at layer 0. Tab buttons are as wide as the cells below them and line up with them: otherwise a Right press on the first hotbar slot or palette cell hit the tab above (the nearest by DingoUI's score, found by the pure suite). Tab cannot close a screen, because a modal root takes Tab as a navigation key; Esc, pad B and Start close a screen, and the pad's View button toggles the inventory.
- **Stable widget ids**: a widget id hashes the whole label, so slot labels are `Name##slotN` and the count is a separate Text, and build cells are `Name##buildN`. `SetFocus(label)` is called inside the widget's own container just before its button.
- **GPU side** (non-headless only): `Ui/GameUi.{h,cpp}` owns the font (`fonts/Inter-Bold.ttf`), sets DingoUI's default font and theme (and unsets the font in its destructor), builds the HUD content, shows the newest of the controller's and the host's messages as a toast (`UI_TOAST_SECONDS`), draws the open screen. `Ui/Hud.*` (clock + day bar top-left, coins and materials top-right, toast, 9 hotbar tiles with a gold border on the selected one, two hint lines), `Ui/InventoryScreen.cpp`, `Ui/BuildPaletteScreen.cpp`, `Ui/UiKit.*`, `Ui/ScreenParts.*`. Icons are placeholders: a colour swatch per item (`ItemSwatch`) and the name.
- **Layer**: `HearthvaleLayer` holds `m_Screens` (always) and `m_Ui` (non-headless; released in `ReleaseScenePieces`, before the font's owner goes). `PlayerInput.cpp` gained pad B for `ToggleBuild`.
- **Checks**: `Checks/UiLayoutChecks.cpp` (suite "UI layout", 69, headless): maths, stack, host behaviour (pause, pick-up, palette choice), layout containment/overlap/fit at 7 resolutions including 1280x720 and 3840x2160, mirrored navigation (every widget reachable, round trips along rows and columns, tab wrap), HUD text fit. `Checks/UiChecks.{h,cpp}` (`UiCheckRun`, 18, windowed only, after the Static suite): the same layouts measured with the real `Font::MeasureText` (full pack, every selection, every buildable), then the live host opened, a stack moved and put back, palette choice, time stopped and restarted, `IsCapturingInput`/`IsPointerOverUI` while a screen is up, toast.
- **E7, what cannot be driven**: a game cannot press pad buttons or read which widget has the focus, so no check moves the real DingoUI focus or confirms where the ring is. The pure suite mirrors DingoUI's score (`DingoUINav.cpp`); if the engine changes it, that mirror is stale. The widget rects the drawing produces are assumed to match the pure layout (Fixed sizes everywhere, panel padding set explicitly), and nothing reads them back. A real pad pass belongs to P18.
- **Not done / for later**: item icons (P14); the shop, bin, board and pause screens (P9); a keyboard shortcut to close with Tab; an animated open/close (no `UITransition`); text is unverified visually.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 507/0; windowed `--check` under xvfb 540/0 (stays open until the timeout), no VUID, no LOG ERROR, no DingoUI warnings. Not verified: how any of it looks, the pad, Windows.

### 11.11 P9 (2026-10-15): menus

Paths under `examples/Hearthvale/src/`. No engine changes.

- **Design: a screen is data.** `Ui/PageLayout.{h,cpp}` defines `PageSpec` (title, header, rows of `PageCell` = text, button or slider, with a `PageAction` and an int arg). `Ui/MenuPages.{h,cpp}` builds a page from a `MenuView` with the pure `BuildMenuPage(ScreenId, view)` (no GPU, no host). `LayoutPage` turns it into the same `ScreenLayout` the inventory uses, so overflow, overlap, unique-label and reachability checks run over every page. Behaviour is only in `ScreenHost::Act` / `SetSlider` (`Ui/ScreenHostMenus.cpp`); one generic `DrawPage` (`Ui/PageScreens.cpp`, called from `GameUi::Draw` for the nine new `ScreenId`s: Title, Slots, Pause, Options, Credits, Shop, ShippingBin, Board, Dialogue) draws any page. A new menu is a page builder plus actions.
- **Title / slots / pause**: Title is New game, Continue (disabled until a slot is used), Options, Credits, Quit; Back and the pause input do nothing on it. `Ui/SlotProvider.h` is the P12 hook: `SlotInfo Describe(slot)`, `StartNew/Load/Save(slot, message&)`, `SLOT_COUNT` = 3. `HearthvaleLayer::m_Slots` is a `NullSlotProvider` (starts a new game, refuses load/save with a message). A used slot asks "Overwrite?" on a second press when starting or saving. Pause: Resume, Save game, Options, Quit game; the pause input toggles it. The title opens only on an interactive, non-headless, non-scripted launch.
- **Options** (`Ui/OptionsModel.{h,cpp}`, pure): five sliders (master on `Audio.MasterVolume`; buses "Music", "Ambience", "SFX", "UI" in `Audio.BusVolumes`, created in `SetUpMenus` when missing), camera speed 0.5..2.0, VSync, fullscreen with a borderless/exclusive kind (disabled while windowed), hold-to-place. Mapped to `Settings` by `OptionsToSettings` / `OptionsFromSettings`; game keys in `Settings::Game`: `cameraSpeed`, `holdToPlace`, `exclusiveFullscreen` (the kind is kept while windowed). Changes apply live through `ApplyOptions`; the file is saved once when the screen closes (`OnOptionsClosed`). Scripted runs (`--check`, autoplay, perf, fixed dt) never apply or save settings.
- **Credits**: one table in `Defs/CreditDefs.{h,cpp}` (Kenney packs, KayKit, Inter, DingoEngine rows), paged by `PaginateText`-style row slices. A check compares it with the pack folders under `assets/`.
- **Shop / bin / board / dialogue**: Shop and workshop use `ShopStock`, `ShopBuyItem`, `ShopSellItem`, a Buy/Sell tab and an amount button (x1/x5/x25), and page long lists. The bin uses `ShippingBin::Ship/Retrieve` (Ship / Take back tabs); goods stay in it when the screen closes (the day roll pays). The board shows one of the open boards (Next / Prev board), a Give button per gift request, and "Hear the mayor" when the festival is Pending. Dialogue wraps with the pure `WrapLines` / `PaginateText` (`Ui/UiLayout`), one page per `DIALOGUE_LINES` lines. `ScreenHost::SetVillagerLines(fn)` is the P10 hook (default `PlaceholderVillagerLine`); `OpenDialogue(script)` plays any script.
- **Festival flow**: the mayor's speech (`FestivalScript()`) is a dialogue over the board; only closing its last page calls `CommunityBoard::AcknowledgeFestival` (Pending to Held, tier 1), then a toast. Leaving early or replaying it acknowledges nothing.
- **Interactions**: `Placing/Interactions.{h,cpp}` `LandmarkAt(grid, x, y)` finds the landmark under a tile; `BuildController::SetInteractHandler` is called from the Interact path in `InteractTool` (the farmhouse door still sleeps). Mapping through `ScreenHost::OpenLandmark`: Shop -> shop, Carpenter -> workshop, ShippingBin -> bin, NoticeBoard -> board, MayorHouse -> Odette dialogue, Cottage -> Pip dialogue. Pulses are swallowed while a menu is open.
- **Hold to place**: `PlayerIntent::UseHeld` (the non-pulse button), `BuildController::SetHoldToPlace`. With the option on, holding use after a press in Build mode places on each new tile the aim reaches; a tool swing never arms it. `CameraRig::SetSpeedScale` scales the ease.
- **Checks**: `Checks/MenuChecks.cpp` (suite "Menus", 130, headless): every page variant (about 3100: packs empty/starter/full, every status string, every board state and tier, dialogue scripts including 40 sentences and blank lines) inside its panel, no overlap, unique labels, text fits, 7 resolutions including 1280x720 and 3840x2160, all widgets reachable from the default focus; wrap/paginate; options clamp, Settings save-data round trip and window-mode mapping; credits; every screen's actions and focus return; festival; interactions; hold-to-place. Shared helpers moved to `Checks/LayoutAsserts.h` and `Checks/BoardFill.h`. `Checks/UiChecks.cpp` gained (windowed, 20): all page variants measured with the real font (`ForEachMenuPage`), and each of the nine screens opened on the live host, drawn for 3 frames, checked for held input and a stopped clock, then closed.
- **Skipped / known**: the camera speed scale has no headless test (CameraRig is renderer-side); no pad pass (P18); the focus-ring size of a slider and the DingoUI layout rects are not readable by the game, so a slider's cell uses the assumed 4 px ring; the pagination uses the 0.62 per-character estimate, so a text of all-wide letters could overflow the dialogue box with the real font (ordinary text was measured and fits). Fullscreen / display-mode effects and audio levels are unverified.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 637/0; windowed `--check` under xvfb 690/0, no VUID, no LOG ERROR. Not verified: how any menu looks, audio, GPU, Windows, D3D.

### 11.12 P10 (2026-10-15): villagers and animals

Paths under `examples/Hearthvale/src/`. No engine changes.

- **Schedules** (`Life/Schedule.{h,cpp}`, pure): each villager has a table of `ScheduleEntry{Minute, Spot}` per `ScheduleVariant` (Fair, Wet, Snow; `VariantFor(Weather)`). Spots: Inside (door tile), Doorstep, SquareWest/Centre/East, BoardFront, RoadWest, PondShore. `ResolveSpotTile` falls back to the nearest walkable tile; `BuildDayPlan` routes every stop with a BFS over walkable tiles (`FindRoute`). `PlaceOnPlan(plan, minute)` is stateless (1.5 tiles per game minute from the stop's minute), so a load or a skipped hour needs no replay. A stop that cannot be reached keeps the previous tile (a villager walled in stays on the door tile). Wet and Snow share a table except Pip, who plays in the snow.
- **Sim**: `Life/VillagerSim.{h,cpp}` registers a `SimStage::Villagers` handler, rebuilds plans when the weather variant changes or `InvalidatePlans()` is called (the layer does so from the BuildController dirty callback). `PickNearby` (cone in front of the farmer, outdoors only) and `PickNearPoint` are pure.
- **Friendship** (`Farm/Friendship.{h,cpp}`, in `GameState::Bonds` and `GameStateDigest::Bonds`): 100 points per heart, 10 hearts, a talk pays once a day, a liked gift 80, any other non-tool gift 20, one gift a day; tools are refused. Tuning in `GameTuning.h` (`FRIENDSHIP_*`, `VILLAGER_*`).
- **Dialogue**: `Life/VillagerLines.{h,cpp}` (16 ASCII lines each, gated by season, weather and hearts, chosen by `HashMix(day, villager, talk)`; gift replies per outcome) and `Life/VillagerDialogue.{h,cpp}` (`Talk`, `Gift` over GameState). Plumbing: `ScreenHost::SetVillagerLines` and the new `SetVillagerGift`; `DialogueScript::GiftTo` adds a "Give <item>" button (`PageAction::DialogueGift`) on the last page, enabled for a non-empty, non-tool selected slot.
- **Picking**: `Life/VillagerWorld.{h,cpp}` gives each villager a kinematic sensor box on query layer 2, so the farmer never bumps into one and `Physics3D::RayCast` (layer filter, sensors included) plus `Scene::GetEntityFromBody3D` finds them. Indoor villagers are inactive. `HearthvaleLayer::FindTalkTarget`: the mouse ray within `VILLAGER_PICK_REACH` of the farmer, else Interact with `PickNearby`. A hit faces the farmer, opens the dialogue and swallows the press before the build controller sees it. Visuals are the placeholder primitives from the art registry (`villager_*`, `animal_*`, `produce_*`); no KayKit character yet (P14).
- **Animals** (`Farm/Animals.{h,cpp}`, `Life/AnimalWorld.{h,cpp}`) behind `GameTuning ANIMALS_ENABLED`: `Herd` in `GameState::Animals` (hashed: kind, home, happiness, fed, produce; positions are never hashed). The day roll (`DayRollStep::Sprinklers` slot, ahead of the calendar roll) prunes animals whose pen is gone, makes produce for fed animals, lowers happiness for unfed ones and moves one new animal into each pen with room (decision: animals move in by themselves; feeding is free). Interact on a coop or barn (`Placing/PlaceInteract.{h,cpp}`, reached through `BuildController::SetTileInteractHandler`) collects produce that fits the pack and feeds everyone. Wander is `AnimalSim` at `SimStage::Animals` with a per-animal `Rng`, `ResolveMove` and a short leash. With the switch off nothing is registered, no entity or state exists, and `InteractWithPlace` leaves pens alone; `AnimalSim` and `RollAnimalsDay` also take a runtime `enabled` so the checks cover the off path in one binary.
- **Well and pond**: the well refills the watering can by message only (placeholder, no water meter yet); a Water tile answers with a fishing stub message.
- **Checks**: `Checks/LifeChecks.cpp` (suite "Life", 109, headless: schedules over 4 seeds x 3 variants, determinism, blocked tiles, a director-driven day, picks, friendship, 16-line pools and the P9 wrap function, gifts, animals, switch off, place interactions, hash) and `Checks/LifeWorldChecks.{h,cpp}` (8, windowed only, between Static and UI: a ray from above finds each outdoor villager, indoor ones are not found, animal entities follow the herd).
- **Skipped / known**: no pet or fishing mechanic; no gift taste beyond liked/other; the villagers do not react to the player's fences except by re-routing; hit boxes and look of the placeholder bodies untested on a GPU. Engine gap: none.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 746/0; windowed `--check` under xvfb 807/0, no VUID, no LOG ERROR. Not verified: how anything looks, GPU, Windows, D3D.

### 11.13 P11 (2026-10-15): look and sound wiring

Paths under `examples/Hearthvale/src/` unless noted. No engine changes. Every value in `GameTuning.h`'s "Look and sound (P11)" block is a first guess: nobody has seen or heard any of it.

- **Audio assets**: `scripts/generate_hearthvale_audio.py` (stdlib Python, no pwsh in cloud; deterministic, same md5 on two runs) writes 30 mono 22.05 kHz WAVs (2.6 MB) to `examples/Hearthvale/assets/audio/`: `ui_*` (6), `tool_*` (5), `place remove plant harvest shop_bell bin_thunk footstep_{grass,dirt,path} chicken pond_loop`, `amb_{birds,crickets,wind,rain}` loops, `music_{spring,summer,fall,winter}` loops. Committed output; re-run the script to regenerate.
- **Pure** (headless, "Look" suite, 32 checks): `Look/NightCurve` (Darkness, Gloom, LampAmount, FireflyAmount, MistAmount, ChimneyAmount), `Look/LampLod` (Candlewick's LightLod minus gameplay lights: fade weights, stickiness, snap-off so in-view lights never exceed capacity), `Audio/AudioMix` (ambience layers by hour/season/weather, music envelope 6.0-6.4 in / 19.5-20.0 out, equal-power `FadeIn/FadeOut`, `Approach`, `MusicMixer` per-season crossfade, `FootstepClock`, `SurfaceFor(Ground)`, `CluckTimer` on `Rng`).
- **Device side, never built headless** (constructed in `HearthvaleLayer::BuildValleyScene`, torn down first in `ReleaseScenePieces`: lights, vfx, audio): `Look/Vfx` owns every particle effect (rain and snow boxes that follow the camera focus, fireflies, mist, chimney smoke and festival sparks emitters it hands out, bursts for harvest/dust/chips/spray) and does nothing under `--no-particles`; `Look/NightLights` makes a point light and emissive bulb per lamp post and festival lantern, a porch light, two window panes (`Art::WINDOW`) and a chimney emitter per house, rescanned when `ValleyWorld::GetCellsRebuiltTotal()` changes, lights through `LampLod` with capacity `GetLocalLightBudget() - LIGHT_LOD_HEADROOM`; windows and bulbs are non-Static so a glow change never rebakes a cell; `Audio/GameAudio` finds or creates the four option buses, loads the 30 clips, runs 4 music loops and 4 ambience loops at volume 0 (set each update) and a positional pond loop, and plays one-shots (`Play` 2D on UI/SFX bus, `PlayAt` spatial, culled beyond its far distance).
- **Wiring**: the `SimStage::VfxAudio` handler calls `HearthvaleLayer::UpdateVfxAudio` (weather into `WeatherFx`, then lights, vfx, audio). `BuildController::SetFeedbackHandler(ActionFeedback, x, y)` fires on every till/water/plant/harvest/chop/mine/cut/place/remove/denied; the layer maps each to a sound and a burst. `MenuServices::OnClick` (called from `ScreenHost::Act`) plays `ui_click`. Screen open/close, coin gains and hotbar slot changes are polled in `PollUiSounds`. Shop and bin plays a bell/thunk at the door. Footsteps follow the farmer's horizontal travel (jumps over `FOOTSTEP_MAX_JUMP` ignored). Clucks pick a chicken from `AnimalSim::GetMotions()`.
- **Flags**: `--no-post` (CameraRig, from P4), `--no-shadows` and `--no-fog` (Sky), `--no-particles` (Vfx) are honoured; post (Soft tone map, bloom 0.3, AO) already lived in `CameraRig`.
- **Decisions**: the listener is the camera, so sfx distances measure from the camera, not the farmer. Night amount is one curve (`LampAmount`) for lights, bulbs and windows; rain/snow glow them by day. Fireflies need clear weather and summer/spring/fall. ui_focus is played on hotbar changes because the engine exposes no focus-change query for DingoUI (candidate engine gap).
- **Not verified**: how any of it looks or sounds, the light LOD against the real budget on a GPU, bloom on bulbs and windows, particle counts and cost, D3D. Headless `--check` 778/0; windowed `--check` under xvfb 839/0 (audio device absent, clips still load 30 of 30); 20 s runs at 21:00 summer clear, 19:30 winter rain and snow, and 06:00 with all four `--no-*` flags: no VUID, no LOG ERROR, no crash.

### 11.14 P12 (2026-10-15): saves

Paths under `examples/Hearthvale/src/` unless noted. No engine changes: everything goes through `SaveData` (`WriteFile`'s `.tmp`, `.bak` rotation and rename; `ReadFile`'s `.tmp`/`.bak` fallback) and `SaveSlots::GetDirectory()`, so the engine's `--save-dir` moves the saves.

- **Schema** (`Save/SaveGame.{h,cpp}`, `SAVE_SCHEMA = 1`): `schema`, `meta.playSeconds`, `seed`, `calendar{day, tick, weather name}`, `grid{width, height, ground[96 rows of g t w p ~ #], built["x y id"], crops["x y def stage days planted"], occupants["id kind def x y w h rot var"], nextOccupant}`, `pack{coins, selected, slots[{slot, item name, count}], plans[build ids]}`, `bin{item name: count}`, `board{festival, tier, boards[4][4][progress, baseline, done]}`, `bonds[4]{points, lastTalk, lastGift, giftsOnLastGift}`, `animals{nextId, list[{id, kind, home, happiness, fed, produce}]}`. The grid holds tiles and occupants, never entities; `ValleyDirector::Restore` swaps the grid and clock in and the layer rebuilds the world (`ValleyWorld::MarkAll`, `AnimalSim::Reset`, `VillagerSim::InvalidatePlans`, farmer to the farmhouse door) in `HearthvaleLayer::OnGameReplaced`. Items are saved by name; crops, builds, decor and landmarks by number, so those tables are append-only (or bump the schema). `friend struct SaveCodec` on Inventory, ShippingBin, CommunityBoard, Friendship and Herd; TileGrid gained `RestoreOccupant` (ids ascending, no overlap) and `Get/SetNextOccupantId`.
- **Migration**: `MigrateSave` runs `k_Migrations[n]` (schema n to n+1) up to the current one and refuses a newer or non-numeric schema. v0 is a hand-written first layout (top-level day/tick/weather number/coins, sparse `grid.tiles` [x, y, ground], no bonds, animals or meta), fixture `examples/Hearthvale/assets/test/saves/hearthvale-v0.json` (it ships in the package; 2 KB).
- **Loading is strict and atomic**: `ReadSave` migrates a copy and checks every field (types, ranges, known names, row lengths, duplicate tiles and slots, occupant order and overlap, stack maxima, ids below their next id) into a `LoadedGame`; the running game changes only through `ApplyLoadedGame` after a full success.
- **Store** (`Save/SaveStore.{h,cpp}`): `slotN.json` in one directory; `Read` returns Empty / Ok (with `FromBackup`) / Damaged. A primary that parses but fails `ReadSave` is treated like an unparseable one: its `.bak` is tried, with a warning. `Delete` removes the file, `.bak` and `.tmp`.
- **Slots** (`Save/GameSaves.{h,cpp}`, the P9 `SlotProvider`): 3 slots. The game saves at the day roll only (`DayRollStep::Autosave`): each roll snapshots the new morning and writes it to the active slot; sleeping and passing out both roll, so "save on sleep" (HD7) is this autosave. The pause menu's Save writes that morning snapshot to the chosen slot and makes it active. New and Load make their slot active; deleting the active slot leaves none, and no autosave runs until one is chosen. Summaries: "Spring 5, year 1 - 812 coins - 2h 15m played" (+ " (from its backup)"), "Empty", "Damaged: no copy of this save can be read". Play time counts steps outside the title screen.
- **UI**: `SlotInfo::Damaged`; `SlotProvider::Delete`; each slots row gained a Delete button (`PageAction::SlotDelete`, "Delete" then "Sure?" for confirmation, enabled for used or damaged slots); a damaged slot cannot be loaded or continued but can be deleted or overwritten after the usual question.
- **Flags**: `--load=1..3` (or `slot1..slot3`) loads at start and skips the title; a bad name or an empty/damaged slot warns and starts as before. `--seed` now also seeds New game (`LaunchOptions::HasSeed`); otherwise each new game rolls its own seed. A new game is day 1 of spring at 06:00 whatever `--day`/`--season` say.
- **Checks**: `Checks/SaveChecks.cpp` (suite "Save", 52, headless): a scripted farmer (till, plant, water, harvest, ship, buy, talk, gift, build and tend a coop, place and later remove a fence, two pass-outs) proves 7 days + save + load into another director + 7 days equals 14 straight days on every digest part; in-memory round trip per part plus tree and JSON identity; corrupt fields refused with the target untouched; newer and non-numeric schemas refused; store: truncated primary to `.bak`, invalid primary to `.bak`, no backup is Damaged, a partial `.tmp` (killed write) ignored, a complete `.tmp` (killed before rename) recovered, delete leaves nothing; the v0 fixture migrates field by field; the provider's new/autosave/save/load/delete/damaged paths and `ParseSlot`. Files go to `<exe dir>/check-saves`, removed at the end. `Checks/MenuChecks.cpp` gained the delete and damaged-slot screen checks.
- **Skipped / known**: autosave runs on the sim thread inside the day roll (a synchronous write of a few thousand JSON lines); no save thumbnails; a save keeps only the morning, so quitting mid-day loses that day.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 833/0; windowed `--check` under xvfb 894/0, no VUID, no LOG ERROR; `--load=1` with the migrated v0 fixture runs headless and windowed for 15-20 s without errors. Not verified: GPU, Windows, D3D, how the slots screen looks.

### 11.15 P13 (2026-10-15): autoplay and checks

Paths under `examples/Hearthvale/src/` unless noted. No engine changes.

- **Autoplay bot** (`Checks/Autoplay.{h,cpp}`, `AutoplayFarmer`): runs in the `ToolsAndPlacement` stage, reads no clock or random number. Each day it plans a stop queue (shop, field, bin, fence run, lamp, gift, bed) and acts only through the sim's own entry points: farm actions are a `PlayerIntent` into `BuildController::Step`; the shop calls `ShopBuyItem`, the bin `Bin.Ship`, a gift `Evaluate`/`GiveGift`/`Bonds.Give`. **How it walks**: it needs no `Player`; it keeps its own feet and yaw and moves as tile hops along `FindRoute` at `PLAYER_SPEED`. A windowed run steps the real `Player` with that heading for the walk animation and then teleports it to the bot's feet, so headless and windowed runs play the same days (same hashes, checked at `--days=2`). It tills/plants/waters a field of crop rows, skipping fields that need nothing, picks the seed by profit per tile-day (idle late in a season when the growth no longer fits), builds a ten-fence run (`FindFenceRun` searches a window of rows for ten placeable tiles, because the clearing is dense with trees and rocks) and a lamp, sleeps, and delivers a board gift when a villager is reachable: the Gift stop polls (`GIFT_POLL_STEPS`) while the villager is indoors and gives up at 80% of the day.
- **`Checks/AutoplayRig.h`**: the sim the layer wires, minus the window (director, farm, economy, villagers, animals, sleep, build controller, bot). `--days` in the layer, the Autoplay suite and the Determinism suite all play it. `FormatDayLine` writes `[Day] n season dayOfSeason coins C crops N bin B board D/T hash <16 hex>`; `FormatAutoplayStats` the end-of-run line.
- **`--days=N`** (forces Autoplay and `--fixed-dt`, runs the layer's own stepping loop): one `Director.Step()` per step, a whole day per update headless (`DAY_TICKS`), `DAYS_STEPS_WINDOWED` (60) per frame windowed unless `--steps-per-frame` is given; headless skips the scene update. It logs one `[Day]` line per day, then `[Autoplay] N days: ...` and closes. `--steps-per-frame` is applied only with a fixed dt (`LaunchOptions::HasStepsPerFrame`) and warns above 1 without one.
- **`--perf`** (needs Autoplay): `Sim/PerfWindow` (no clock; the layer times each stage with the engine `Timer`) skips `PERF_WARMUP_FRAMES` (180) and averages `PERF_WINDOW_FRAMES` (600): frame, sim, scene, sync, DingoUI, render, draw calls, plus `Renderer::GetGpuTimers()` windowed. It logs `[PERF]` lines, then closes. The numbers are the owner's on the GPU; llvmpipe's are meaningless.
- **`--freeze`** now also stops the villagers and animals stages (`ValleyDirector::m_Frozen`), and scene and sync dt are 0; time was already paused. **`--layout=demo`** (`Farm/DemoLayout.{h,cpp}`, applied after a load would have been, never over one): learns every plan, fills the 12-column plot with one crop kind per column and growth by row, a fence ring (wood N/S, stone W/E, gates), scarecrow, sprinklers, chest, four lamps, a stone path from the farmhouse door and one of every building kind scattered with `FindSpot`. 50 buildings, 120 crops. Its out-of-season crops wither at the first day roll, so a frozen shot is the intended use.
- **Suites**: all nine of plan 5 now log a summary and `CheckReport` records each (`AddSuite`, merged by name; the windowed Static, Life world and UI window runs register theirs). The last lines are one `[Checks] <suite> N passed, M failed` per suite and the total `[Checks] N passed, M failed`; the exit code is unchanged. **Assets** (`Checks/AssetChecks.cpp`, new): every art id the game asks for resolves, ids are unique, every model/clip/sound/font path (`Art::All()`, `GameAudioPaths()`, `FarmerAssetPaths()`, `GameUiFontPath()`) exists under the asset root; windowed, `FarmerAssets` is usable (clips resolve on the skeleton), the font loads, every art model path loads, every sound decodes. **Autoplay** (28 days at seed 1: 28 lines, the bot works every stage, 10 fences, a lamp, a gift, ends on the map; plus the `PerfWindow` checks). **Determinism**: two rigs at seed 1 give equal 7-day lines and hash, seed 2 differs.
- **Decisions**: the stage is a bot step, not a scripted input stream, so it cannot desync from the sim; the walk animation is cosmetic. Games with a `Player` in the loop (windowed `--days`) cannot be blocked by a screen: the bot never opens one.
- **Observed**: 28 days at seed 1 end `[Day] 28 summer 14 coins 6319`, 105 harvested, 2 gifts; the bot idles in late summer when no crop fits. Not verified: GPU, Windows, D3D, the demo layout's look, real `--perf` numbers.
- Verified (Linux, llvmpipe): Debug build with no warnings; headless `--check` 856/0; windowed `--check` under xvfb 921/0 (no `[FAIL]`, no VUID, no LOG ERROR; 7 Assets checks, sounds decode); `--days=7 --seed=1 --graphics=headless` twice gives identical `[Day]` lines; `--days=28 --autoplay --graphics=headless` takes 2.0 s; windowed `--days=2` matches headless hashes; `--layout=demo --freeze --autoplay --perf` windowed closed after the `[PERF]` block.
