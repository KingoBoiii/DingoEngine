# P8 — *Candlewick*: example-game plan

Drafted 2026-10-01 on branch `claude/v0-7-lighting-planning-cad5a1` @ `1ac5ee3` (v0.7 P0–P7 done).
Every `file:line` below was read on that date. Scope source: `ROADMAP.md:138-144` and §6 of
`.claude/plans/2026-09-26-v0.7-lighting-plan.md`.

**Status**: decisions D1–D4 (§9) settled 2026-10-01, each on the recommended option. Building
starts only after the user's go-ahead.

---

## 1. Where we start (verified facts)

| Area | Today | Anchor |
|---|---|---|
| Light components | `PointLightComponent{Color, Intensity=1, Range=10, Enabled}`, `SpotLightComponent{…, InnerConeAngle=20, OuterConeAngle=30, Direction(0,0,-1) local, Enabled}`, `AmbientLightComponent{Color, Intensity=0.1}` | `Components.h:174-216` |
| Spot aim | `spot.Position = transform->Position; spot.Direction = transform->Rotation * light.Direction` | `LightSystem.cpp:95-96` |
| Spot setup | outer clamped to [1,179], inner to [0,outer]; `coneScale = 1/max(cos(inner) - cos(outer), 1e-4)` | `Renderer3D.cpp:404-416` |
| Shader weight | `falloff = 1 - d²/r²`; `cone = saturate(dot(dir, axis)·scale + offset)`; light × `N·L × falloff² × cone²`; nothing past `Range` | `Renderer3D_Lit.glsl:124-134` |
| Budget | 4 directional + 32 point/spot (`GetLocalLightBudget`). Frustum cull, then score, nearer-first tie-break; overflow warns once, `Statistics::DroppedLights` | `Renderer3D.h:22-42,120,157-174`, `docs/lighting.md:137-187` |
| Light cost | Debug, 1600×900: sun only 1.97 ms, 32 lights 2.07 ms (CPU-bound) | v0.7 review `:61-66` |
| Meshes | Only `CreateBox`, `CreateSphere`; `Mesh::Create(vertices, indices)` for procedural shapes; no mutation | `Mesh.h:19-23` |
| Lit materials | `CreateLitMaterial(MaterialParams)` with emissive, roughness, specular and `FillMode`; emissive is per **material** | `Renderer3D.h:150`, `Material.h:15-46,120-123` |
| Casts | `RayCast(Ray, maxDist, hit)`, `ShapeCastSphere`, `OverlapSphere`. No layers, filters, ignore-body or triggers | `Physics3D.h:110-121` |
| Ray inside a body | The plain `CastRay` reports a hit at fraction 0 when the ray starts inside a convex shape | `JoltPhysics3D.cpp:461-464` |
| Player vs rays | A character controller is not a body, so no query ever hits the player | `JoltCharacterController3D.cpp:31` |
| Controller rotation | The Scene overwrites `Transform3D.Rotation` with the controller's every frame; use `SetRotation` | `PhysicsSync.cpp:127`, `CharacterController3D.h:68` |
| Hierarchy | None until v0.8; carried lights are separate entities a script moves (DC3D's lantern) | `DungeonCrawler3D/src/GameScripts.cpp:344-362` |
| Script order | Unspecified (`unordered_map`); scripts run before the physics step | `ScriptSystem.h:49` |
| HUD | Second, orthographic primary camera in the same scene; `SpriteRendererComponent` quads and `TextComponent` | `EchoVault/src/GameScripts.cpp:20-61,641-698` |
| Closest template | EchoVault: `CharacterController3D` player, sentry ray-cast line of sight, positional audio, gamepad, Menu/Game/Win scenes | `EchoVault/src/*` (≈1,300 lines) |

**Pitfalls the templates already hit** (avoid in Candlewick):

- EchoVault's sentry cone is tested in xz after a 3D normalise, so the cone shrinks with height
  (`EchoVault/src/GameScripts.cpp:608-611`). Candlewick tests in 3D with the renderer's formula.
- EchoVault casts from inside the sentry's own box (`:599`). Candlewick's wardens have no body.
- EchoVault's "face the move direction" write is a no-op (`:480`, see Controller rotation above).
- EchoVault's ambient loop is never stopped (`:120`). Keep the `AudioSoundId` and stop it in `OnDestroy`.
- A light without a `Transform3DComponent` is skipped (review B2), and removing the last light
  component brings the default sun back (review B12). Every light gets a transform, and the keep
  holds one permanent `AmbientLightComponent`.

---

## 2. The game

A stealth crawl through four rooms of a dark keep. Every light is a gameplay object.

**Loop**: cross a room in the dark, read the wardens' cones on the floor, slip past, reach the
room's brazier, light it (checkpoint, oil refill, the room becomes visible), move on. Lighting the
Chapel altar wins.

**Controls**

| Action | Keyboard | Gamepad |
|---|---|---|
| Move (world axes) | WASD / arrows | Left stick / d-pad |
| Snuff / relight lantern | Q | X |
| Light a brazier (hold 1 s) | E | A |
| Pause / title | Esc | Start |

### 2.1 The lantern: radius is the resource

- `Oil` 0–100. While lit it burns 1/s, so a full lantern lasts 100 s.
- `Range = mix(2.5, 7.0, Oil / 100)`. Intensity 1.3, warm colour. The lantern mesh's emissive
  strength follows the same ratio.
- **Snuff** (Q): `Enabled = false`, no burn. **Relight**: a 0.8 s strike that costs 3 oil, during
  which you can't move. At 0 oil the lantern gutters out and can't be relit until refuelled.
- Oil flasks: +35 on touch (distance test, 1 m).
- The light rides on its own entity at hand height, beside the body, outside the player's mesh
  (DC3D's lesson: a light inside a mesh leaves the mesh unlit).

### 2.2 Wardens: moving lights that see

Each warden is three entities kept in sync by game code (no hierarchy until v0.8):

| Entity | Components | Placement |
|---|---|---|
| Body | `MeshRendererComponent` (box stack), **no rigid body** | feet on the patrol path |
| Lamp | `PointLightComponent` (cold, range 3.5, intensity 0.9) + emissive mesh | hand height, beside the body |
| Eye | `SpotLightComponent` (range ≤ 8, inner 14°, outer 24°, intensity 1.4) | head height (1.7 m), aimed along the facing, pitched 25° down |

- Patrol: a loop of tile waypoints at 1.6 m/s; **Investigate**: walk to the last-seen point by BFS on
  the room's tile map at 2.4 m/s, look around for 3 s, walk back.
- **No body**, on purpose: rays never start inside a warden, and a warden never shoves the player.
  Catching is a distance and detection test, not a collision.
- **Range clamp**: every frame the eye casts a ray along its axis; the spot's `Range` becomes
  `min(8, hit distance + 0.5)`. The cone no longer shines through the wall it faces, and detection
  reads the same clamped `Range`.

### 2.3 Detection: the drawn cone is the tested cone

Detection evaluates the renderer's own weight at three sample points on the player (feet 0.1 m,
chest 1.0 m, head 1.6 m):

```
weight(spot, p) = falloff² · cone²            // the shader's term, without the surface's N·L
  v = p − spot.Position;  d² = |v|²;           0 if d² ≥ Range²
  falloff = 1 − d² / Range²
  cone    = saturate(dot(v / |v|, axis) · scale − cosOuter · scale)   // Renderer3D.cpp:404-416
```

Two channels feed one **suspicion** value `s` ∈ [0, 1] per warden (starting values, tuned in C4/C6):

1. **Cone** (always on): any sample with `weight ≥ SEEN_WEIGHT` and a clear line of sight →
   `s += dt · (1.5 + 3 · weight)`. Standing in the visible pool means being seen.
2. **Beacon** (only while you are lit): within `1.6 × lantern Range` of the warden, inside a 160°
   field, with line of sight → `s += dt · 0.8`. "Lit" means the lantern is lit, or you stand in a
   lit brazier's or sconce's light at `weight ≥ 0.25`. A bigger lantern sees more and is seen from
   further. Snuffed in the dark, only the cone can find you.

- Otherwise `s -= dt · 0.35`. At `s ≥ 0.4` the warden investigates; at `s ≥ 1` you are caught.
- **Line of sight**: a ray from the eye toward the sample. Clear if nothing is hit before the
  sample's distance minus 0.3 m, since the player is never hit (EchoVault's rule).
- **Caught**: a 1 s fade, respawn at the last lit brazier with that checkpoint's oil, wardens reset.
- A small marker above each warden shows its state (calm / suspicious / alert), drawn as three
  shared emissive materials swapped on the marker.

### 2.4 Braziers: checkpoints and the only real light

- Unlit at start (light `Enabled = false`, a dark core material), except the Gatehouse's.
- Hold E/A for 1 s within 1.5 m with the lantern lit: the light turns on (range 9, intensity 1.1),
  the core swaps to a lit emissive material, a crackle loop starts, the checkpoint is saved and oil
  refills to 100.
- Flicker: each lit brazier's light intensity varies ±8 % with its own phase. Emissive is per
  material, so the cores share one material with one flicker.
- The Chapel altar is the final brazier; lighting it wins.

### 2.5 Snuffing blinds you

The keep has no sun: one `AmbientLightComponent` at about 0.03, slightly blue. With the lantern out,
the room is black except for sconces, lit braziers and the wardens' own lights and pools. You
navigate by the light of the things hunting you.

### 2.6 The keep: four rooms and their lights

| # | Room | Tiles | Decorative flames | Braziers | Wardens | Oil | Local lights when all on |
|---|---|---|---|---|---|---|---|
| 1 | Gatehouse (tutorial: lantern, snuff) | 10×8 | 4 sconces | 1 (pre-lit start) | 0 | 2 | 5 |
| 2 | Great Hall (first warden) | 16×12 | 8 sconces | 2 | 1 | 2 | 12 |
| 3 | Gallery (two crossing wardens) | 26×6 | 24 candles | 1 | 2 | 1 | 29 |
| 4 | Chapel (altar = exit) | 12×12 | 8 candles | 1 | 1 | 1 | 11 |

Plus the lantern: **58 local lights** in the keep. The Gallery alone, with the lantern and the
Hall's edge in the frustum, passes the 32 budget. That is the honest stress case: many small static
flames and a few moving ones. Decorative flames are small (range 2.5, intensity 0.8) so the rooms
stay dark until their braziers are lit.

**Out of scope**: hearing/noise, combat, doors and keys, saving, mouse look, more than one level,
shadows (v0.9), animated characters (v0.8).

---

## 3. Technical design

### 3.1 Project layout

```
examples/Candlewick/
  premake5.lua                  copy of EchoVault's, project name changed; calls copyAssimpRuntime()
  scripts/generate_audio.ps1    synthesises the WAVs (EchoVault's approach)
  assets/audio/*.wav            crackle loop, ambient drone, footstep, warden step, strike, snuff,
                                flask, alert sting, caught, ignite, win
  assets/fonts/arialbd.ttf      same file the other examples ship
  src/main.cpp                  CreateApplication, --graphics parsing (EchoVault's)
  src/CandlewickLayer.{h,cpp}   SceneManager: Title, Keep, End
  src/GameTuning.h              every number in §2
  src/KeepMap.{h,cpp}           the four ASCII room maps, warden routes, tile queries, BFS
  src/KeepDirector.{h,cpp}      the controller script: builds the keep in OnStart, owns update order
  src/Player.{h,cpp}            controller, body visual, lantern
  src/Wardens.{h,cpp}           rigs, patrol/investigate, range clamp
  src/Detection.{h,cpp}         weight(), line of sight, suspicion
  src/Braziers.{h,cpp}          lighting, flicker, checkpoints
  src/LightLod.{h,cpp}          decorative-flame budgeting (D4)
  src/Hud.{h,cpp}               oil meter, prompts, fade, title/end text
```

### 3.2 One owner of update order

Script order is unspecified, so the keep runs from **one** `KeepDirector` script whose `OnUpdate`
calls plain subsystem classes in a fixed order:

`Input → Player → Lantern → Wardens → Detection → Braziers → LightLod → Hud`

Entity scripts are not used for gameplay. The player moves through its controller, which the Scene
steps after scripts, so the lantern trails by one frame (about 5 cm at walking speed, invisible).

### 3.3 Level authoring

ASCII maps in `KeepMap.cpp`, one per room, 1 tile = 1 m:

`#` wall · `.` floor · `S` start · `B` brazier · `A` altar · `s` sconce (on the nearest wall) ·
`c` candle · `o` oil flask · `>` doorway to the next room

- Walls are 2 m tall. Horizontal runs of `#` merge into one box with one static body, so the keep
  is about 120 wall boxes, not 600 tiles.
- One shared material per role (stone, floor, wood, brass), so batching stays per-material.
- Warden routes are tile lists in code next to the map they walk.

### 3.4 Camera

D1: a three-quarter follow camera, pitch about 55°, fixed yaw, lagged like EchoVault's. If the
south walls hide the player in playtests, C2 adds a cutaway that hides wall boxes between the
camera and the player (`MeshRendererComponent::Visible`).

### 3.5 Light LOD (D4)

Game code, every frame:

1. Gameplay lights are never touched: the lantern, every warden's lamp and eye, and lit braziers.
2. Decorative flames are ranked by distance to the player, ties by index (stable).
3. The first `GetLocalLightBudget() − gameplay lights in range − 2` stay on. The rest fade to 0
   over 0.3 s, then `Enabled = false`.
4. Hysteresis: an enabled flame only fades once it ranks 2 past the cut-off.

`--no-light-lod` turns it off, so the engine's own selection (and K16's popping) shows up. With LOD
on, the Gallery should read `DroppedLights = 0` in F4. This is the rank-fade band the engine
deferred (v0.7 plan §2.4 step 6), done where the game knows what matters.

### 3.6 Capture and debug flags

Every phase's before/after and pixel checks need reproducible frames:

| Flag | Effect |
|---|---|
| `--graphics=vulkan\|dx11\|dx12` | backend (copied from EchoVault) |
| `--room=1..4` | start at that room's checkpoint, skip the title |
| `--freeze` | wardens hold their first waypoint, oil doesn't drain, flicker off |
| `--overview` | fixed camera over the room's centre |
| `--oil=<0-100>` | starting oil |
| `--no-light-lod` | engine-only light selection |
| `--debug-cone` | wireframe cones (a procedural cone mesh per warden, `FillMode::Wireframe`) and the three samples coloured by weight |
| `--light-budget=<1-32>` | *(C3)* lowers `MaxLocalLights`, to stress the light LOD |
| `--spawn=<col>,<row>` | *(C4)* the player's start tile (falls back to the room spawn on a bad or blocked tile) |
| `--no-range-clamp` | *(C4)* the wardens' eyes keep their full range, to show the leak through walls |
| `--all-lit` | *(C5)* every brazier but the altar starts lit |

`--freeze` also stops catches (suspicion and markers still update), so frozen captures stay stable.

---

## 4. Engine change (D3)

The drawn cone and the tested cone should be one code path, not a copy that can drift:

```cpp
// include/DingoEngine/Graphics/Light.h
float GetLightAttenuation(const PointLight& light, const glm::vec3& point); // falloff²
float GetLightAttenuation(const SpotLight& light, const glm::vec3& point);  // falloff² · cone²

// include/DingoEngine/Scene/Components.h
PointLight PointLightComponent::ToLight(const Transform3DComponent&) const;
SpotLight  SpotLightComponent::ToLight(const Transform3DComponent&) const;
```

- `Renderer3D::SubmitLight(const SpotLight&)` and the helper share one internal function for the
  angle clamps and `scale`/`offset` (today inline at `Renderer3D.cpp:404-416`).
- `LightSystem` builds its lights with `ToLight` (today `LightSystem.cpp:95-96`).
- The shader is unchanged. The helper documents that it is the shader's term without `N·L`.
- Lighting Test: new PASS/FAIL checks: 1 at a point light's centre, 0 at `Range`, 1 on a spot's axis
  inside the inner angle, 0 past the outer angle, an unchanged spot when built through `ToLight`.
- Its existing modes must stay pixel-identical (a refactor of the setup math).
- `docs/lighting.md` gains a "Gameplay queries" section.

---

## 5. Phases

Each phase ends with a fresh-agent review of its diff, fixes, and a before/after section on the
v0.7 Lighting Log page. Commits happen only when the user says "commit"; one commit per fix.

| # | Phase | Est. | Tier | Done when | Page section |
|---|---|---|---|---|---|
| **C0** | Scaffold: project, root `include`, `.vscode` entries, `build-release.yml` job, Title → Keep → End scenes, one lit test room | 0.5 d | Sonnet | Debug and Distribution build; the exe starts from `examples/Candlewick`; `premake5 vs2026` regenerates cleanly | First frame (no "before") |
| **C1** | Engine helper (§4) + Lighting Test checks + docs | 0.5–1 d | Opus (renderer boundary) | All Lighting Test checks PASS; its modes are 0 px different from `1ac5ee3` | Check list + identical frames |
| **C2** | The keep: four maps, wall merging, materials, sconces/candles with emissive cores, player controller, body, camera, ambient | 1.5 d | Sonnet | Every room walkable on Vulkan; F4 entity and draw counts recorded | `--overview` of each room |
| **C3** | Lantern: oil, range, snuff/relight, flasks, HUD meter; Light LOD | 1 d | Sonnet | Range visibly tracks oil; Gallery `DroppedLights` 0 with LOD, > 0 without | Oil 100 / 30 / snuffed; LOD on vs off |
| **C4** | Wardens: rigs, patrol, investigate (BFS), range clamp, detection, suspicion, caught → respawn, `--debug-cone` | 2.5 d | Opus design, Sonnet build | Detection boundary within 0.25 m of the visible pool edge along the axis (measured on a `--freeze --debug-cone` capture); no detection through walls | Pool with samples; range clamp before/after |
| **C5** | Braziers, checkpoints, win, audio, gamepad, title/end screens | 1.5 d | Sonnet | A full run from Gatehouse to altar on keyboard and on a pad | A room before/after its brazier is lit |
| **C6** | Tuning, perf, DX11/DX12, milestone review of all of P8, fixes, docs | 2 d | Opus reviewer (fresh), Sonnet fixes | Review has no open Critical/High; docs updated (§7) | Final gallery: one frame per room |

**Total: about 9.5–10 dev days, so two working weeks.**

Suggested commit trail:

1. `build(candlewick): add the Candlewick example project`
2. `ci: package Candlewick in the release workflow`
3. `feat(renderer3d): expose light attenuation for gameplay queries`
4. `test: check light attenuation in the Lighting Test`
5. `feat(candlewick): build the keep and the player`
6. `feat(candlewick): add the lantern and its oil`
7. `feat(candlewick): budget decorative flames in game code`
8. `feat(candlewick): add wardens with spot-light vision`
9. `feat(candlewick): add braziers, audio and the game flow`
10. review fixes, one each; then `docs: document Candlewick`

---

## 6. Verification

- **Builds**: Debug and Distribution of `Candlewick.vcxproj`, plus the whole `DingoEngine.slnx`
  in Debug once at C0 and C6, so no other example breaks.
- **Backends**: every room on Vulkan, DX11 and DX12 in C6 (`--graphics=`).
- **Same data**: the C4 capture check above. It is the claim the ROADMAP makes, so it gets a
  measurement, not a look.
- **Budget**: `--room=3 --freeze`, LOD on and off. F4 numbers and the `Dingo.log` WARN recorded.
- **Perf**: F4 frame time per room, Debug and Release, VSync off, recorded in the review. The v0.7
  review still lacks a Release GPU-bound figure; Candlewick's Gallery provides it.
- **Leaks through walls**: a capture of each warden next to a wall, cone on and off. Light still
  passes walls sideways (no shadows until v0.9); detection must not.
- **Engine regression (C1 only)**: the Lighting Test modes pixel-diffed against a `1ac5ee3` exe
  (hide `Renderer3D_Lit.glsl` while running the old exe, per the capture reference).
- `test/imgui.ini` and `examples/*/imgui.ini` are rewritten by captures: `git checkout --` them.

---

## 7. New-executable and docs checklist

New executable (CLAUDE.md "New executable checklist"):

- [ ] `examples/Candlewick/premake5.lua` calls `copyAssimpRuntime()` (helper at root
      `premake5.lua:76-90`).
- [ ] `include "examples/Candlewick"` in the `Examples` group (root `premake5.lua:314-323`).
- [ ] `.github/workflows/build-release.yml`: an "Upload Candlewick binary" step beside EchoVault's
      (`:169-174`), and an `example-game-candlewick` job copied from `example-game-echo-vault`
      (`:483-531`): robocopy assets, zip, `gh release upload`. The release then has 12 assets, not 11.
- [ ] `.vscode/tasks.json` "Build Candlewick Debug" (after `:217-233`) and `.vscode/launch.json`
      "Debug Candlewick" (after `:77-87`, cwd `examples/Candlewick`).
- `build-examples.yml` is manual-only and lists FlappyBird alone; left untouched.

Docs (C6):

- [ ] `README.md`: `:34` status line, `:46` v0.7 row → shipped, `:81` project list, a Candlewick row
      after `:93`.
- [ ] `ROADMAP.md`: `:128` ("Candlewick … and the review pass are still to come"), `:138` ("still
      to come"). Also check `:131` ("ties go to the earlier submission") against the nearer-first
      tie-break the P7 fixes made.
- [ ] `CLAUDE.md`: project-structure example list, the v0.7.0 roadmap bullet (still says P0–P6).
- [ ] `docs/lighting.md`: point to Candlewick next to DC3D's `--night` (`:350`), plus §4's "Gameplay
      queries".

---

## 8. Risks

| Risk | Mitigation |
|---|---|
| Light passes through walls (no shadows until v0.9): a cone or lamp lights the next room | Short ranges; the eye's range clamp; patrol routes kept ≥ 3 m from walls they face; LoS keeps detection honest; documented as the v0.9 upgrade |
| A warden's cone drops out at the budget edge while it still detects | Gameplay lights are never in the LOD pool (D4); C3 verifies `DroppedLights = 0` |
| Overlapping flames clip to white (K17) | Brazier + lantern + sconce peaks kept near 1.2; checked on captures |
| South walls hide the player | 2 m walls, 55° pitch; cutaway in C2 only if needed |
| Translucent lit meshes are unsorted and write depth | No translucent meshes; markers and cones are opaque or wireframe |
| CPU vertex transform with hundreds of boxes | Wall-run merging, shared materials, perf recorded in C6 |
| SceneManager switches scenes inside its own `OnUpdate` | EchoVault's before/after compare in the layer (`EchoVaultLayer.cpp:59-66`) |

---

## 9. Decisions (settled 2026-10-01)

| # | Decision | Chosen | Rejected |
|---|---|---|---|
| D1 | Camera | Three-quarter follow, fixed yaw, world-axis movement: cones read on the floor, no mouse needed, pad is trivial | Third-person over the shoulder: more "blind", but cones are hard to read from behind, and it needs mouse look and camera collision |
| D2 | Detection | Cone + beacon channels into a suspicion meter (§2.3): snuffing really hides you, and a bigger lantern is a trade-off | Cone only, caught instantly: simpler, but snuffing then changes nothing about being seen |
| D3 | Engine change | Small shared helper (§4): one code path for the drawn and tested cone, with PASS/FAIL checks | No engine change: the game copies the formula, which can drift from a hot-reloaded shader |
| D4 | Light budget | Game-side LOD with fades for decorative flames (§3.5), `--no-light-lod` for the stress view | Engine selection only: a pure stress test, but a warden's cone can pop out while it still sees you |

---

## 10. Engine gaps found (backlog candidates, not built in P8)

- Light priority, so a game can protect gameplay lights without its own LOD.
- Trigger volumes / sensors (Candlewick uses distance tests).
- Ray-cast filters (ignore a body, layer masks) and a body → entity lookup.
- Cone, cylinder and plane primitives.
- A scene-transition callback on `SceneManager` (EchoVault and Candlewick both diff the active scene).
- Already scheduled: transform hierarchy (v0.8) deletes the warden and lantern sync code; shadows
  (v0.9) stop the leaks through walls, and bloom/tone mapping lift the clipping.

---

## 11. As built

*C0 (scaffold):*

- Beyond §3.1: `Overlay.{h,cpp}` (font, overlay camera, text, confirm prompt) and
  `TitleScreen.{h,cpp}` (the Title and End scripts). Scene names and the HUD's ortho size live in
  `GameTuning.h`.
- A scene's scripts start once per attachment, not on every activation. Title and End are built
  once; only the Keep is rebuilt when it is left. An End fanfare needs its own trigger (C5).
- The look so far: a brazier light 0.5 m above the core leaves the stand's sides black (they face
  away from it), and wall tops read as black. C2's room pass owns both.
- From the C0 review, moved to C2: spawn helpers must return the light and core entities (light
  LOD, flicker and checkpoints need them), and decor must not get a collider (a sconce bracket
  would block a warden's line of sight). Moved to C5: Esc pauses instead of leaving the keep.
- Verified: Debug and Distribution build, and so does the whole solution in Debug. Title → Keep →
  Title → Keep rebuilds an identical frame (0 px). Distribution's frame equals Debug's (0 px). Bad
  flag values warn and keep the defaults.

*C1 (engine helper):*

- `GetLightAttenuation(PointLight|SpotLight, point)` in `Light.h`, implemented in
  `src/DingoEngine/Graphics/Light.cpp`. `ToLight(const Transform3DComponent&)` on the point and spot
  components is inline in `Components.h`, after `Transform3DComponent` (which is forward-declared
  above the light components).
- The shared C++ math is `src/DingoEngine/Graphics/LightMath.h` (`Internal::IsUsableLight`,
  `Internal::GetLightCone`). The §1 and §4 anchors `Renderer3D.cpp:404-416` and
  `LightSystem.cpp:95-96` now point at `GetLightCone` and at `ToLight`. Both `SubmitLight` overloads
  share a private `SubmitLocalLight` template.
- A spot never weighs exactly 1: on its axis the weight is falloff², and at the light's own
  position the shader normalises a zero vector. The checks test 0.5625 at half range instead.
- The helper doesn't know the frame's budget. A light dropped past `MaxLocalLights` still has a
  weight, which is why D4 keeps gameplay lights out of the LOD pool. Documented in `docs/lighting.md`.
- Verified: 23 Lighting Test checks PASS (13 existing + 10 new). Disabling the usable-light guard
  makes the rejection check FAIL, and dropping the square on falloff fails five others. The default,
  lights, overbudget, materials, both entities modes and the batch test are 0 px different from
  the build before C1.

*C2 (the keep):*

- The keep is one 79×14 ASCII grid in `KeepMap.cpp` (north = top row = −Z): Gatehouse, Great
  Hall, Gallery, Chapel, joined by three 4-tile corridors, plus three Gallery alcoves. Rooms and
  corridors are rects in code; alcove tiles inherit the room they open off. All 597 floor tiles
  are reachable from spawn 1. Digits `1`–`4` are the per-room spawns.
- `KeepWorld` builds it: 72 wall boxes (greedy rectangles, capped at 4×4 tiles so the cutaway
  stays local), each with a thin cap slab whose faint cold emissive outlines the layout in the dark,
  plus 10 floor slabs. 49 flame lights: 12 sconces and 32 candles (decorative, the LOD's pool),
  4 braziers and the altar, all lit in C2 (C5 makes the braziers unlit at start).
- Decor has no collider. Brazier stacks and the altar do, so they are cover below about 1 m (C4).
- The cutaway is needed: without it a player at a south wall is fully hidden. It hides any wall
  rect (cap and mounted sconce parts too, never lights) that the segment from the eye to the
  player's feet + 0.2 m crosses. In `--overview` it hides the room's south wall instead.
- The player has a faint cloak emissive and a skin-tone face so it reads in the dark and shows
  its facing. Flame cores, brass and wax have faint emissives too: a light directly above never
  reaches a vertical face.
- Flame cores use a game-owned 6×8 sphere: the engine's 16×16 sphere put 49 cores at 77 % of
  one batch's index cap. Gallery overview: 23,388 indices and 8,536 vertices, down from 84,972 and
  19,898; 8 draw calls.
- The Gallery overview has 34 lights in view against the budget of 32 (two dropped) before the
  lantern and wardens exist; a Hall sconce reaches its left edge. The follow camera sees far fewer.
- Moved to later phases from the C2 review:
  - C3: the light LOD changes only `PointLightComponent` intensity/`Enabled`, never flame cores
    (emissive cores cost no light budget), so it cannot fight the cutaway over core visibility.
    `DecorFlame::BaseIntensity` holds the unscaled value. The player needs a movement lock for
    the 0.8 s strike.
  - C4: the eye's range-clamp ray must be cast horizontally along the facing at eye height. A ray
    along the 25°-down axis from 1.7 m always hits the floor slab at about 4 m. `IsWalkable` is
    false on `B`/`A` tiles.
  - C5: brazier cores need a dark "unlit" material and a lit one (cores share one flame material
    today); `KeepWorld`'s brazier list is in raster order, so index it by `Room`, not position.
    The player needs a teleport for respawns.

*C3 (lantern and light LOD):*

- Lantern intensity is 2.0, not 1.3: at hand height 1.3 didn't read. Relighting needs oil > 3,
  so a strike never yields a dead flame; between 0 and 3 the HUD says "Too little oil to strike".
  A Q/X pressed during the strike is queued. The last 5 oil flicker.
- The hand position swings (side → front-left → front, then shortens) by horizontal rays from the
  controller axis, so the light never sits inside a wall; frame, glass and light move together.
- Flasks: 6, no light or collider, +35 oil, and left in place while they would be wasted
  (oil > 65).
- **Light LOD as built** (replaces §3.5's rank hysteresis, which cancelled the headroom):
  capacity = budget − 2. Each frame the gameplay lights that are on and inside the engine's own
  frustum test (`CameraRig::GetViewProjection`, the matrix Renderer3D culls with) are counted;
  flames get the remaining slots, ranked by distance to the camera focus with an index tiebreak
  and a 0.6 m stickiness bonus for lit flames. A flame fades in only when there is room (planned
  with a 2 m inflated sphere so it fades before entering view); if the exact in-view load is still
  over capacity, the lowest-ranked in-view flames snap to 0. Out-of-view flames stay at full
  weight. Gameplay lights register with `LightLod::AddGameplayLight` (point or spot); C5's flicker
  goes through `SetFlicker`.
- Update order: Esc → Player → flasks → Lantern → (C4) → (C5) → Camera → LightLod → cutaway → Hud.
- Verified: a Gallery walk (both legs, snuff and relight on each) logs no budget WARN at the
  default budget and at `--light-budget=12`; the same walk with `--no-light-lod` does (23 lights
  vs 12). With the headroom set to 0 the walks at budgets 8, 12 and 32 still log none, so the
  accounting is exact. Gallery overview F4: 30/32 drawn, 0 dropped with LOD; 32/32, 3 dropped
  without.

*C4 (wardens and detection):*

- Routes (tile loops in `KeepMap.cpp`, expanded by BFS and reduced to corners): Hall
  (21,4)→(27,4)→(27,9)→(21,9) round the central pillars; Gallery A (38,6)→(51,6)→(51,7)→(38,7) and
  B (57,7)→(44,7)→(44,6)→(57,6), passing side by side; Chapel (69,3)→(73,3)→(73,10)→(69,10) in
  front of the altar. Paths avoid candle tiles (wardens have no collider) and never leave the
  warden's room: a sighting outside it resolves to the nearest in-room tile, and an investigating
  warden stops 1 m short of its goal. A warden yields to a lower-index one 0.9 m ahead.
- Eye as built: 1.7 m high, 0.25 m in front of the helm, 25° down, 14°/24°, intensity 1.4, range
  min(8, level-ray hit + 0.5), shrinking at once and growing at 6 m/s so its range sphere never
  jumps into view. Lamp: 3.5 m, 0.9.
- `SEEN_WEIGHT` = 0.1 (0.05 overshot the visible pool by about 0.46 m). Measured in the Hall with
  the eye on minus off: the pool adds ≥ 3/255 from 1.60 to 6.40 m along the axis; the footprint
  dots run 1.625 to 6.375 m. The edges agree within 0.025 m.
- **Design change from §2.3:** the beacon can only make a warden investigate; it is capped at the
  alert threshold, so only the cone drawn on the floor can catch. Candles don't make the player
  "lit" (the Gallery would be lit everywhere and snuffing would mean nothing there); sconces count
  at their base strength and braziers while lit, never through the light LOD's state, so
  `--no-light-lod` and `--light-budget` don't change gameplay. Rates: cone 0.6 + 1.2·w, beacon
  0.5 (field 120°), decay 0.25; a cone catch takes about 1.1 s with the suspicious marker visible
  on the way.
- Caught: movement halts that frame, 1 s fade, respawn at the last room's spawn with at least
  50 oil (C5 replaces both with the checkpoint), wardens reset.
- Found and fixed in the engine as its own commit (`764a09c`): Vulkan never enabled
  `fillModeNonSolid`, so `FillMode::Wireframe` (the debug cone) logged a validation error.
- Verified: snuffed in the candle-lit Gallery lane, facing a warden 3.5 m away outside its cone,
  suspicion stays 0 for 629 frames; lit, the warden holds at 0.75 for 6 s without a catch. Default,
  `--no-light-lod` and `--light-budget=12` give the same suspicion trace (0.4 at 0.81 s, 0.75 at
  1.50 s). Through-wall: a player behind a pillar inside leaked light stays at 0. No budget WARN on
  Gallery walks.

*C5 (braziers, the win, audio, pause):*

- Braziers start cold (light off, an ash core material) except the Gatehouse's. In reach (1.5 m)
  with the lantern lit, a press of E / A then a 1 s hold lights one: light on, core → the shared
  flame material, a spatial crackle loop, oil to 100, `LightLod::AddGameplayLight`, a "Checkpoint"
  toast. The checkpoint is a patrol-floor tile beside the brazier plus the oil then (never below
  what a relight needs). With `--room > 1` or `--spawn` the first checkpoint is the start tile.
- Flicker: lit braziers ±8 %, decorative flames ±5 % through `SetFlicker`, the flame material's
  emissive ±10 %; off under `--freeze`. Gameplay never reads intensity, so flicker can't change it.
- Caught → respawn at the checkpoint with its oil, wardens reset, the catch counted. Lighting the
  altar wins: a 1.2 s linger, a 1.4 s fade, then End with the time and the catches. The End
  scene is rebuilt each time it is left (like the Keep) and reads a layer-owned `RunResult`, since
  a scene's scripts start only once per attachment.
- Audio: `scripts/generate_audio.ps1` synthesises 11 clips (381 KB, 22050 Hz mono, fixed seed):
  crackle loop, drone loop, footstep, warden step, strike, snuff, flask, alert, caught, ignite, win.
  Positional sounds use linear falloff (crackle 12 m, steps 10 m, wardens 14 m); the listener
  follows the player, not the camera. The drone's `AudioSoundId` is kept and stopped when the
  Keep goes (EchoVault's leak avoided).
- Pause: Esc / pad Start toggles; subsystems don't update, the player halts, master volume is
  muted and restored. Pad B or Enter leaves to the Title from the pause (Start can't do both).
- Not verified here: pad hardware (no pad on this machine) and listening to the audio.
