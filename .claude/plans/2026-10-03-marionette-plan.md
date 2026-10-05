# P13 — *Marionette*: example-game plan

Drafted 2026-10-03 on branch `claude/v0-8-animation-character-fidelity-7209eb` @ `368caff` (v0.8
P4–P12 done). Every `file:line` below was read on that date. Scope source: §6 of
`.claude/plans/2026-10-01-v0.8-animation-plan.md`, which this plan replaces as the design.

**Status**: planned. D1–D4 settled by the user on 2026-10-04 on the recommended options; D5 is
proposed. A fresh Sonnet review (2026-10-04) found 2 High, 6 Medium and 4 Low in the animation
mechanics, all folded in (§12). M0–M6 are done (§11): P13 is complete; v0.8 is ready to release.

---

## 1. Where we start (verified facts)

| Area | Today | Anchor |
|---|---|---|
| Animator | `Play(state, fade, layer)`, `PlayOneShot(clip, in=0.1, out=0.2, layer)`, `IsOneShotPlaying`, `Stop`, `SetLayer`/`SetLayerWeight`, `SetFloat`, `GetEventsThisFrame`, `IsEventActive`; `AnimationState::Clip`/`Blend1D` | `Animator.h:55-196` |
| One-shot over a one-shot | The second keeps the first one's `Resume`, so a hit reaction during an attack returns to locomotion, not to the attack | `Animator.cpp:385-401` |
| One-shot tail | A one-shot pushes its `Resume` back `fadeOut` seconds before its end: `IsOneShotPlaying` goes false then, and its marks in those last seconds never fire. A one-shot always returns, so it can't hold a final pose | `Animator.cpp:642-657`, `docs/animation.md:517-541` |
| States loop by default | `AnimationState::m_Loop = true`; a non-looping state ends with `IsFinished(layer)` and holds its last frame | `Animator.h:61,81,167` |
| Events | `ScriptableEntity::OnAnimationEvent(const AnimationEvent&)` with `Name`, `Time`, `Type` (instant / range begin / range end), `Clip`, `Layer`. Per layer only the dominant contribution fires: an incoming state takes over at fade weight ≥ 0.5, so the old state's ranges stay open until then. A layer above 0 fires only while its `SetLayer` weight (not its fade) is ≥ 0.5. A non-looping state catches up from its start when it first leads, so a mark at t ≈ 0 still fires | `ScriptableEntity.h:44`, `Animator.h:19-29`, `Animator.cpp:731`, `docs/animation.md:630-679` |
| Sidecar | `<model stem>.events` beside a model with clips: `<clip> <s> <event>` or `<clip> <a>..<b> <event>` | `docs/animation.md:548-585` |
| Components | `SkinnedMeshRendererComponent{Model*, Color, Material*, Visible}`, `AnimatorComponent{DefaultClip, PlayOnStart, Speed, Enabled}` | `Components.h:365-393` |
| Sockets | `SetParent(parent, "joint", keep)`; world = parent × joint frame × local; follows the animator's pose | `Entity.h:84-111`, `docs/animation.md:685-734` |
| Clip libraries | A file with a skeleton and clips but no meshes; any character with the same joint names plays them, retargeted by name (same names and rest orientations; rotations from the clip, root translation scaled by the rest-offset ratio) | `docs/animation.md:165-172,735-769` |
| Limits | 128 joints a draw; `MaxSkinnedInstances` 64 a frame (≤ 256); 4 states a layer; skinned draws after static ones; **colliders follow the rest pose**; no root motion | `docs/animation.md:856-884`, v0.8 plan §2.9 |
| Casts | `RayCast`, `ShapeCastSphere`, `OverlapSphere`. No layers, filters, ignore-body, sensors or triggers | `Physics3D.h:120-131` |
| Controllers | `CharacterController3D` is a Jolt `CharacterVirtual`, not a body, so no cast or overlap ever hits a fighter. No character-vs-character collision is set up (no `CharacterVsCharacterCollision`, no inner body), so two fighters walk through each other. `SetRotation` exists (the Scene writes the controller's rotation back every frame) | `JoltCharacterController3D.cpp:48-62`, `CharacterController3D.h:66-81` |
| Assets | `AssetManager::Load`/`LoadAsync` + `GetModel(handle)`; models reload in place with their `.events` file when hot-reload is on | `AssetManager.h:104-159`, CLAUDE.md "Model hot-reload" |
| Closest template | Candlewick: one director script owning update order, `LaunchOptions`, `GameTuning.h`, `Hud`, synthesized audio (`scripts/generate_audio.ps1`), Title/Game/End scenes | `examples/Candlewick/src/*` |
| Test asset | Only the Fox (CC-BY 4.0, 24 joints, Survey/Walk/Run, no combat clips) | `test/assets/models/Fox/` |

**What this means for the design**

- A fighter can't be hit by a physics query (controllers aren't bodies), and bodies on sockets
  would shove the other fighter's controller (no sensors). So hits are **game-side spheres on
  sockets** (D3), not the shape casts §6 sketched.
- Every combat window has to come from the clip, because the engine fires it from the clip: that
  is the point of the game (§2.2).
- The v0.8 out-of-scope list holds: no root motion, no IK, no state-machine assets. The fighter's
  state machine is game code calling `Play`.

---

## 2. The game

A one-arena melee duel. The player fights three opponents in a row, each faster and smarter than
the last. Every opponent uses the same clips as the player on the same rig template. What differs
is proportions, animation speed and the AI.

**Loop**: read the opponent's wind-up, then parry it, block it or dodge it, and punish the recovery.
Win a bout by draining the opponent's health; lose and the bout restarts. Beat the third for the
win screen.

**Controls**

| Action | Keyboard + mouse | Gamepad |
|---|---|---|
| Move (camera-relative) | WASD | Left stick |
| Light attack (chains) | Left mouse / J | X |
| Heavy attack | Right mouse / K | Y |
| Block (hold) / parry (tap at the right moment) | Shift / L | Right bumper |
| Dodge | Space | A |
| Pause / title | Esc | Start |

### 2.1 Fighters share one body of code

`Fighter` is one class for the player and the AI. A *brain* (`PlayerBrain` or `AiBrain`) writes a
`FighterIntent { move, light, heavy, block, dodge }` each frame, and the fighter turns intents into
animator calls. So the AI can do nothing the player can't, and `--autoplay` (AI vs AI) exercises
the same paths a person does.

States: `Locomotion`, `Attack`, `Block`, `Dodge`, `HitReact`, `Stagger`, `Dead`. A one-shot state
lasts its clip's length minus its fade-out (§1, "One-shot tail"); `Dead` is a non-looping `Play`
that holds its last frame. The fighter keeps no timers for them; it checks `IsOneShotPlaying`,
`IsFinished` and events. The state, not the animator, says whether a fighter can still hit: a
cancelled attack's `hitbox` stays open for the few frames its fade-out lasts, so `Combat` only
counts a hit while the attacker is still in `Attack`.

### 2.2 Combat windows are clip events

Each move's `.events` lines are the whole combat design. Changing one and saving it changes the
game while it runs (P9 hot-reload).

| Event | Kind | Meaning |
|---|---|---|
| `windup` | range | The telegraph: the weapon glows, and the AI reads it to decide on a parry |
| `hitbox` | range | Active frames: the weapon's spheres can hit |
| ~~`lunge`~~ | — | Dropped in M3: moving the capsule by the clip's travel double-counted it (the pose already moves the hips). Instead the capsule catches up with each move's net hips travel over its fade-out (§11 M3) |
| `dash` | range | A dodge also moves the capsule `DODGE_EXTRA_DISTANCE` (0.9 m) in the dodge's direction over this window |
| `combo` | range | A light input inside it chains to the next light attack |
| `iframes` | range | Dodge invulnerability |
| `parry` | range | At the start of the block raise (`Melee_Block`, t ≈ 0): a hit landing inside it is parried |
| `step_l`, `step_r` | instant | Footfalls → positional footstep sounds; walk and run share one phase per foot (the engine fires only the blend's heavier side) |

Every window in a one-shot must end before `duration − fadeOut`, or it never fires (§1). `Moveset`
holds each move's fade-out, and `Combat` warns at load about any event that ends later.

Rules:

- A hit lands when a weapon sphere touches a hurt sphere during `hitbox`, once per swing per target.
- **Parry**: the target was inside `parry`. The attacker staggers (one-shot); the parrier takes
  nothing.
- **Block**: the target was blocking past `parry`. Chip damage and a short push back.
- **Dodge**: the target was inside `iframes`. Nothing happens.
- Otherwise: damage, then `HitReact` (a one-shot with fade-in 0 that cancels the attack and returns
  to locomotion, `Animator.cpp:385-401`).
- **Death**: health 0 → `Stop` layer 1, then `Play(Clip(death).SetLoop(false), 0.1)` on layer 0, so
  the body stays down (a one-shot would stand back up).
- **Hit-stop**: both fighters' `AnimatorComponent::Speed` drops to 0.05 for 70 ms, then returns to
  the fighter's own pace (§2.4), kept apart in `Fighter`. This is the one timer, and it is feel,
  not a window (it uses `Speed`, not `Enabled`, so KNOWN-BUGS K23 can't repeat events).

### 2.3 Animation set-up

- **Locomotion**: layer 0, one `Blend1D` on the parameter `Move` (idle 0, walk, run), synced on
  phase, so feet don't pop. Facing the opponent (below), backing off plays `Walking_Backwards` and
  sidestepping plays `Running_Strafe_Left/Right`, picked by the move direction relative to facing
  and cross-faded with `Play`.
- **Block**: layer 1 masked from `spine` up, `SetLayer` weight 1, so blocking while walking keeps the
  legs walking. Raising plays `Clip(Melee_Block).SetLoop(false)` (looping would lose its `parry`
  mark at t ≈ 0); on `IsFinished(1)` it switches to the looping `Melee_Blocking`. A blocked hit is
  `PlayOneShot(Melee_Block_Hit, …, 1)`, which returns to `Melee_Blocking` through its `Resume`.
  The riposte after a parry `Stop`s layer 1, then plays `Melee_Block_Attack` as a layer-0 one-shot.
  Leaving `Block` for any reason `Stop`s layer 1.
- **Attacks, dodge, hit reaction, stagger**: `PlayOneShot` on layer 0 with short fades (0.05–0.1 s;
  fade-in 0 for `HitReact` and `Stagger`). Telegraph readability comes from the clip and the
  opponent's pace (§2.4), not from long fades.
- **Facing**: a fighter faces the opponent whenever one is in range (locomotion included), so
  dodges, strafes and backpedals read relative to them; with no opponent in range it faces its move
  direction. Turns go through `CharacterController3D::SetRotation` at a rate in `GameTuning.h`.
- **Dodge**: the clip is the one of `Dodge_Forward/Backward/Left/Right` nearest the stick direction
  relative to facing; its `dash` range moves the controller (no root motion). M1 prints the hips'
  translation range in every clip the game uses, so a clip whose hips travel (a dodge, the jump
  chop) gets a `dash` or `lunge` that matches it instead of sliding the body off its capsule.
- **Weapons**: socketed to `handslot.r` (shield to `handslot.l`). Weapon spheres and hurt spheres
  are child entities socketed to joints, so the hit test reads `GetWorldPosition()` of entities the
  renderer also draws in `--debug-hitbox`. Weapons are static models whose submesh textures the
  game puts in a lit material with the pack PNG on slot 0, as it does for the fighters.

### 2.4 Three opponents

| # | Name | Proportions | Pace (`AnimatorComponent::Speed`) | Moves | AI |
|---|---|---|---|---|---|
| 1 | *the Recruit* (Skeleton Minion) | scale 0.9 | 0.85 | light only, single hits | attacks on a slow rhythm, never blocks |
| 2 | *the Veteran* (Barbarian, axe) | scale 1.1 | 1.0 | light + heavy | blocks a third of the time, punishes whiffs |
| 3 | *the Champion* (Skeleton Warrior) | scale 1.15 | 1.15 | full chains, heavy, dodge | parries telegraphed heavies, baits, dodges |

The clips come from clip-library files, so every fighter's clips go through the retargeting path (a
library's skeleton is never the character's, `docs/animation.md:735-769`). True limb-length
differences need a second rig of the same template (§2.6, D1).

### 2.5 The arena

A round stone floor about 16 m across, a low wall ring, four braziers (point lights), a dim
ambient. Static boxes and lit materials only; no new engine work. A spectator ring is optional (D4).

### 2.6 Assets

**Recommended: KayKit by Kay Lousberg** (D1). Read on 2026-10-03, pages only, nothing downloaded:

| Pack | Free tier | Licence (the pack's own page) | Holds |
|---|---|---|---|
| [Character Animations](https://kaylousberg.itch.io/kaykit-character-animations) | 1.1, 14 MB, FBX + glTF | "Creative Commons Zero v1.0 Universal" | 100+ clips for `Rig_Medium`, 25+ for `Rig_Large`, as clip-library files per category |
| [Adventurers](https://kaylousberg.itch.io/kaykit-adventurers) | 2.0, 12 MB | "no attribution required. (CC0 Licensed)" | Knight, Barbarian, Rogue, Mage, Ranger; 25+ weapons and shields as separate meshes |
| [Skeletons](https://kaylousberg.itch.io/kaykit-skeletons) | 1.1, 7.7 MB | "no attribution required. (CC0 Licensed)" | Warrior, Rogue, Mage, Minion; 10+ weapons |

Verified in M0 (2026-10-04) from the downloaded files (each pack's `License.txt` says CC0; credit
"Kay Lousberg, www.kaylousberg.com" is optional):

- **One rig.** Knight, Barbarian, Skeleton Minion and Skeleton Warrior all have the `Rig_Medium`
  skin: 23 joints (`root, hips, spine, chest, head`, `upperarm/lowerarm/wrist/hand/handslot.l|r`,
  `upperleg/lowerleg/foot/toes.l|r`), no fingers, no IK joints. Every joint's rest rotation and
  translation is identical to the clip libraries', so retargeting is exact (ratio 1).
- **Clip libraries are GLBs per category**, and each also holds the 6-mesh mannequin (so they load
  as skinned models, not mesh-less clip libraries; the game never draws them). Lengths in seconds:
  - *CombatMelee* (22): `Melee_1H_Attack_Chop` 1.07, `_Jump_Chop` 1.33, `_Slice_Diagonal` 1.00,
    `_Slice_Horizontal` 1.37, `_Stab` 1.60; `Melee_2H_Attack_Chop` 1.63, `_Slice`, `_Spin`,
    `_Spinning`, `_Stab`; `Melee_2H_Idle`; `Melee_Block`, `Melee_Blocking`, `Melee_Block_Hit`,
    `Melee_Block_Attack` (1.07 each); dual-wield and unarmed attacks; `T-Pose`.
  - *General* (15): `Idle_A` 1.07, `Idle_B` 2.13, `Hit_A` 0.67, `Hit_B` 0.87, `Death_A` 0.80,
    `Death_B` 2.63, poses, spawn, interact.
  - *MovementBasic* (11): `Walking_A/B` 1.07, `Running_A/B` 0.80, jumps.
  - *MovementAdvanced* (13): `Dodge_Forward/Backward/Left/Right` 0.40, `Running_Strafe_Left/Right`,
    `Walking_Backwards`, crouch, sneak.
  - *Special* (15): `Skeletons_Taunt` 1.03, `Skeletons_Taunt_Longer` 3.00, skeleton awaken and
    death.
  No roll, no cheer, and no ready-made light/heavy chains.
- **Textures**: each character GLB embeds its PNG, and the engine skips embedded textures
  (`Model.cpp:98`, §10). The same PNGs ship beside the GLBs (`knight_texture.png`,
  `barbarian_texture.png`, `skeleton_texture.png`), so the game gives each fighter a lit material
  with its PNG on slot 0 (`SkinnedMeshRendererComponent::Material`). The Model 3D Test loads
  `Knight.glb` (9 skinned submeshes, `tex=no`) and `Rig_Medium_CombatMelee.glb` (6) on Vulkan.
- **Weapons** are `.gltf` + `.bin` that point at the shared PNGs: `sword_1handed`, `shield_round`,
  `axe_1handed`, `axe_2handed` (Adventurers), `Skeleton_Blade`, `Skeleton_Axe`,
  `Skeleton_Shield_Small_A` (Skeletons). They load as static models.
- **Free bonus**: the animation pack's `Mannequin_Large.glb` sits on `Rig_Large`: the same 23 joint
  names, a bigger body (hips at 1.04 m against 0.41 m), but six leg joints rest up to about 3°
  differently. `Rig_Medium` clips on it would bend its legs slightly; its own `Rig_Large` clips
  (16 melee) fit. Not used in the slice.

**Repo footprint**: 4 character GLBs (1.4 MB), 5 clip libraries (4.2 MB), 3 PNGs, about 7 weapon
glTFs and the three `License.txt` files: about 6.5 MB, under the 10 MB budget.

**How the slice uses it**

Shared by every fighter: idle `Idle_A`, the `Move` blend `Idle_A` → `Walking_A` → `Running_A`,
`Walking_Backwards`, `Running_Strafe_Left/Right`, the four `Dodge_*`, `HitReact` = `Hit_A`,
`Stagger` = `Hit_B`, and the block set (`Melee_Block` → `Melee_Blocking`, `Melee_Block_Hit`,
riposte `Melee_Block_Attack`; all 1H shield clips, so a fighter without a shield blocks with its
weapon raised, which reads well enough on these toy proportions).

| Fighter | Weapons (`handslot.r` / `.l`) | Light chain (`combo` ranges) | Heavy | Death | Bout intro |
|---|---|---|---|---|---|
| Knight (player) | `sword_1handed` / `shield_round` | 1H diagonal → horizontal → chop | 1H jump chop | `Death_A` | — |
| Skeleton Minion | `Skeleton_Blade` / — | 1H diagonal (single hits) | — | `Skeletons_Death` | `Skeletons_Taunt` |
| Barbarian | `axe_2handed` (two-handed) | 2H slice → chop | 2H spin | `Death_B` | `Skeletons_Taunt_Longer` |
| Skeleton Warrior | `Skeleton_Axe` / `Skeleton_Shield_Small_A` | 1H diagonal → horizontal → stab | 1H jump chop | `Skeletons_Death` | `Skeletons_Taunt` |

The winner of a bout taunts (`Skeletons_Taunt`) over the loser's held death pose; there is no cheer
clip. `Moveset` holds this table, keyed by fighter.

**Proportions**: the free characters all share one body on `Rig_Medium`, so the three opponents
differ by silhouette, weapon and uniform scale (0.9 / 1.1 / 1.15, capsule and reach scaled to match),
not by limb length. Every clip still plays through the retargeting path, because it comes from a
clip-library file. True proportion retargeting needs a second body on the same joint names:
`Barbarian_Large` (Adventurers *Extra*, $7.95) or the free `Mannequin_Large`, both on `Rig_Large`,
whose leg rest rotations differ by up to 3° (M0). Not part of the slice.

**Rejected**

- **Quaternius** (Universal Animation Library and Base Characters): realistic proportions and
  finger joints, but quaternius.com/license.html now shows a "Quaternius Asset License v1.0"
  (updated 2026-08-28). It forbids redistributing the assets as standalone files, which is what a
  public repo does, while its itch pages still say CC0. Not usable without written permission from
  the author.
- **Kenney** (Animated, Blocky and Mini Characters): CC0, but idle/run/jump or a single attack.
  Placeholders at most.
- **Mixamo**: its terms forbid redistributing the raw files.

---

## 3. Technical design

### 3.1 Project layout

```
examples/Marionette/
  premake5.lua                  copy of Candlewick's, project name changed; calls copyAssimpRuntime()
  scripts/generate_audio.ps1    synthesizes the WAVs (Candlewick's approach)
  assets/characters/            the four fighter GLBs and their PNGs
  assets/animations/            the five Rig_Medium clip libraries and our .events sidecars
  assets/weapons/               the weapon glTFs, .bins and the PNGs they name
  assets/LICENSE-KayKit-*.txt   each pack's own License.txt
  assets/audio/*.wav            footstep, swing, hit, block, parry, dodge, win, lose
  assets/fonts/arialbd.ttf      the file every example ships
  src/main.cpp                  CreateApplication, --graphics parsing
  src/MarionetteLayer.{h,cpp}   SceneManager: Title, Arena, End
  src/GameTuning.h              every number in §2
  src/LaunchOptions.{h,cpp}     §3.4
  src/ArenaDirector.{h,cpp}     the one controller script: builds the arena, owns update order, bouts
  src/Fighter.{h,cpp}           controller, animator driving, states, health
  src/Moveset.{h,cpp}           clip lookup by role, per-move tuning (damage, lunge speed)
  src/Combat.{h,cpp}            hit and hurt spheres, hit resolution, hit-stop
  src/PlayerBrain.{h,cpp}       input → FighterIntent
  src/AiBrain.{h,cpp}           the three AI tiers → FighterIntent, seeded
  src/CameraRig.{h,cpp}         the arena camera (D2)
  src/Hud.{h,cpp}               health bars, bout banner, prompts
  src/Audio.{h,cpp}             event → sound
```

### 3.2 One owner of update order

As in Candlewick, one `ArenaDirector` script runs everything in a fixed order:

`Brains → Fighters (intents → animator) → Combat (last frame's poses) → Camera → Hud`

The Scene then runs `AnimationSystem::Update`, which poses the fighters and delivers events, and
then physics, which moves the controllers. So Combat reads the poses and event state from the end
of the last frame. That is one frame (16 ms) late, the same for both fighters, and stays
deterministic. Events reach the director through `OnAnimationEvent` on each fighter's script, which
only records them; the director acts on them in its next update.

### 3.3 Determinism

- `AiBrain` takes its randomness from one seeded generator (`--seed`).
- `--autoplay` replaces the player's brain with tier-3 AI, so a whole bout runs without input.
- `--fixed-dt=<s>` makes the layer pass a constant delta to `SceneManager::OnUpdate` instead of the
  measured one. With a seed, that makes an `--autoplay` bout repeat exactly, for captures and logs.
  Perf runs leave it off.
- AI strength is judged over several seeds (M4: tier 3 beats tier 1 in at least 8 of 10), not one.
- Captures use `--freeze`: fighters hold an idle pose and take no damage, and the camera is fixed.

### 3.4 Capture and debug flags

| Flag | Effect |
|---|---|
| `--graphics=vulkan\|dx11\|dx12` | backend |
| `--bout=1..3` | start at that opponent, skip the title |
| `--freeze` | no AI, no damage, fixed camera; `--pose=<clip>@<s>` freezes both fighters on a frame |
| `--overview` | fixed camera over the arena |
| `--autoplay` | AI vs AI |
| `--seed=N` | the AI's random seed |
| `--fixed-dt=<s>` | a constant scene delta, for repeatable runs |
| `--debug-hitbox` | hit and hurt spheres drawn, coloured by state (idle / active / parried) |
| `--hot-reload` | AssetManager hot-reload on, so a saved `.events` or model changes the running game |
| `--vsync=off` | uncapped frame rate, for timings |

---

## 4. Engine changes

None planned. Gaps found while building go to §10 as backlog candidates, unless one blocks the game,
in which case it is raised with the user before any engine change.

---

## 5. Phases

Each phase ends with a fresh-agent review of its diff, fixes, and a before/after section on the
Animation Log page. Commits happen only when the user says "commit"; one commit per phase or fix.

| # | Phase | Est. | Tier | Done when | Page section |
|---|---|---|---|---|---|
| **M0** | Asset intake (after the download go-ahead): fetch, keep only what the game uses, licence file, load each file in the Animation and Model 3D tests (joint count, clip names, rest orientations match across characters), repo size | 0.5 d | Sonnet; launches by the main session | Every kept file loads; every fighter ≤ 128 skin joints; clip names listed in the plan | The characters side by side in bind pose |
| **M1** | Scaffold: project, root `include`, `.vscode` entries, `build-release.yml` job, Title → Arena → End, the arena and its lights, the assets copied in, all four fighters textured and armed, playing one library clip | 0.75 d | Sonnet | Debug and Distribution build; the exe runs from `examples/Marionette`; `premake5 vs2026` regenerates cleanly; every clip the game names is found; `handslot.r` resolves on every fighter; the four fighters play `Idle_A` at one frozen time and their poses match the library mannequin's joint for joint; the hips' translation range of every clip used is logged | The four fighters side by side |
| **M2** | Movement: controller, `Blend1D` locomotion, facing, camera, `PlayerBrain` (keyboard, mouse, pad), `step` events → positional footsteps | 1.5 d | Sonnet | No foot pop from idle to run; footsteps once per step at every speed | Idle vs mid-blend |
| **M3** | Combat: `Moveset`, the `.events` sidecars, weapon sockets, hit and hurt spheres, damage, hit reaction, block layer and parry, dodge, hit-stop, `--debug-hitbox` | 2.5 d | Opus design, Sonnet build | A scripted exchange (`--autoplay --seed=1`) shows a hit, a block, a parry and a dodge in the log, each from its event | `--debug-hitbox` on a `--pose` frame per window |
| **M4** | AI and bouts: three tiers, health, bout flow, HUD, retry and win (the End scene is rebuilt on leaving, as Candlewick does, since its script starts once) | 2 d | Sonnet (Opus for the AI's read of `windup`) | Tier 1 loses to tier 3 in `--autoplay` runs; a person can beat all three on keyboard and on a pad | One frame per opponent |
| **M5** | Polish: audio, title and end screens, `--hot-reload` (an edit to a `hitbox` range changes the game live), the crowd if D4 | 1.5 d | Sonnet | A live `.events` edit captured before and after | The live edit |
| **M6** | Tuning, perf, DX11/DX12, milestone review of all of P13, fixes, docs | 2 d | Opus reviewer (fresh), Sonnet fixes | Review has no open Critical/High; docs updated (§7) | Final gallery |

**Total: about 11 dev days, so two to three working weeks.**

Suggested commit trail:

1. `build(marionette): add the Marionette example project` (with its assets and licence)
2. `ci: package Marionette in the release workflow`
3. `feat(marionette): move fighters with blended locomotion`
4. `feat(marionette): drive combat from clip events`
5. `feat(marionette): add three AI opponents and bouts`
6. `feat(marionette): add audio, screens and live event tuning`
7. review fixes, one each; then `docs: document Marionette and finish v0.8`

---

## 6. Verification

- **Builds**: Debug and Distribution of `Marionette.vcxproj`, plus the whole `DingoEngine.slnx` in
  Debug once at M1 and M6, so no other example breaks.
- **Backends**: `--freeze --pose` frames on Vulkan, DX11 and DX12 in M6, pixel-diffed.
- **Events drive combat**: M3's check is measured from the log. Every hit, block, parry and dodge
  line names the event range it fell in. A deliberately broken `.events` file (the `hitbox` range
  moved past the clip's end) must produce no hits.
- **Perf**: F4 frame time and `Scene::OnUpdate` for a bout (`--autoplay --seed=1`), Debug and
  Release, VSync off; with the crowd if D4. Recorded in the review as old (P12's Animation Test
  crowd) vs new where comparable.
- **Engine regression**: none expected (§4). If an engine change happens, the Animation Test's
  checks and its 0 px frames against `368caff` gate it.
- `examples/*/imgui.ini` is rewritten by captures: `git checkout --` it.
- **Launches**: never without the user's "go" and the userguard check.

---

## 7. New-executable and docs checklist

New executable (CLAUDE.md "New executable checklist"):

- [ ] `examples/Marionette/premake5.lua` calls `copyAssimpRuntime()` (helper at root
      `premake5.lua:76`).
- [ ] `include "examples/Marionette"` in the `Examples` group (root `premake5.lua:314-324`).
- [ ] `.github/workflows/build-release.yml`: an "Upload Marionette binary" step beside Candlewick's
      (`:183`), and an `example-game-marionette` job copied from `example-game-candlewick`
      (`:590-638`). The release then has 13 assets.
- [ ] `.vscode/tasks.json` "Build Marionette Debug" (after `:235`) and `.vscode/launch.json`
      "Debug Marionette" (after `:89-99`, cwd `examples/Marionette`).

Docs (M6):

- [ ] `README.md`: project list and a Marionette row; the v0.8 status.
- [ ] `ROADMAP.md`: the v0.8 status block (Marionette done).
- [ ] `CLAUDE.md`: the example list in Project structure, and the v0.8.0 roadmap bullet.
- [ ] `docs/animation.md`: point to Marionette as the worked example (events as combat windows,
      block layer, sockets, a clip library shared by several characters).
- [ ] The asset licences: each pack's `License.txt`, kept as `assets/LICENSE-KayKit-<pack>.txt` (done in M1), and a
      credits line in the README ("Kay Lousberg, www.kaylousberg.com"; optional under CC0, given as thanks).

---

## 8. Risks

| Risk | Mitigation |
|---|---|
| The pack's characters don't share rest orientations, so retargeting bends limbs | M0 compares rest orientations per joint before any game code; fall back to one character with per-fighter scale and tint (D1) |
| Joint names with dots (`handslot.r`) are changed by the loader, or the clip names differ from the third-party lists | M0 prints the skeleton's joint names and every library's clip names as assimp reports them; `Moveset` looks clips up by the names M0 records |
| KayKit's stylised arms are posed wide, so weapon spheres sit far from the body | Hurt and weapon spheres are tuned per joint in `GameTuning.h` against `--debug-hitbox` captures |
| A `parry` mark at t ≈ 0 on the block layer never fires | `Melee_Block` plays with `SetLoop(false)`, so it catches up from its start when it takes over; the block layer's `SetLayer` weight stays 1 (§1 Events). M3 checks `parry` fires on every block |
| One frame of combat latency (§3.2) makes parries feel late | It's 16 ms and symmetric; if playtests disagree, Combat moves after the animator through a second director pass (needs no engine change: a late script order) |
| Fighters overlap: controllers don't collide with each other (§1), and lunges close distance fast | Game-side separation in `Combat`: after intents, clamp each fighter's velocity so the pair stays ≥ 2 × capsule radius apart (M2) |
| Hit spheres miss fast swings between frames | Test the swept segment between last and this frame's sphere centres, not just the point |
| Pack size bloats the repo | M0 keeps only used files; budget about 10 MB |
| Skinned draws after static ones: a translucent effect over a fighter hides it | No translucent meshes over fighters |
| `MaxSkinnedInstances` with a crowd | A bout draws two fighters; a crowd (D4, skipped) would need ≤ 62 to stay within 64 |

---

## 9. Decisions (D1–D4 settled 2026-10-04; D5 proposed)

| # | Decision | Recommended | Alternative |
|---|---|---|---|
| D1 | Character pack | **KayKit free tiers** (§2.6): CC0 on each pack's page, one shared rig, clip libraries that match the engine's design, block/dodge/hit/death/taunt clips. Download: Character Animations Free 1.1 (14 MB), Adventurers Free 2.0 (12 MB), Skeletons Free 1.1 (7.7 MB), from kaylousberg.itch.io | KayKit + Adventurers *Extra* ($7.95) for `Barbarian_Large` on `Rig_Large`, so opponent 2 shows true proportion retargeting. Quaternius is out on licence; Kenney lacks combat clips |
| D2 | Camera | **Arena camera**: fixed yaw, frames both fighters, pulls back as they separate. Telegraphs read from the side, no mouse look, the pad is trivial | Over-the-shoulder lock-on: closer, but needs mouse look and camera collision, and hides the opponent's wind-up behind the player |
| D3 | Hit detection | **Game-side spheres on sockets**: controllers aren't bodies and there are no sensors, so physics can't do it without the fighters shoving each other; the drawn spheres are the tested spheres | Kinematic bodies on sockets + `OverlapSphere`: uses more engine, but the bodies push the other fighter's controller |
| D4 | Spectators | **Skip** for the slice; revisit in M5 if time allows | A ring of 40 spectators playing cheer clips at random phases: shows the instance budget and the shared library at scale, +1 d |
| D5 | Event naming | **Plain names** (`hitbox`, `lunge`) with per-move numbers in `Moveset` | Numbers in event names (`lunge_4`): no table, but parsing names is fragile |

---

## 10. Engine gaps found (backlog candidates)

Filled in while building. Already known from §1: no sensors or cast filters, no body → entity
lookup, no character-vs-character collision, no event payloads, no root motion.

- **Embedded textures** (M0): `Model::LoadFromFile` returns no texture for a GLB's embedded image
  (`Model.cpp:98-100`), and most glTF packs embed theirs. Marionette works around it with the PNGs
  KayKit also ships. An engine fix decodes `aiTexture` data (`stbi_load_from_memory`) and keys the
  texture cache by model path + index, so hot-reload still matches it.
- **Clip libraries with a preview mesh** (M0): KayKit's libraries carry the mannequin, so each
  loads its 6 skinned meshes for nothing. A load option for "clips only" would skip them.
- **Retargeting dropped the hips' motion** (M1, **fixed** in `a81727f` with the user's go-ahead):
  KayKit keys a still translation on `root`, which made `root` the "root-most animated joint", so
  every retargeted clip lost its hips motion (M1's pose check: 2.2 cm off at idle). Now only a
  translation that leaves the source rest offset counts; the check matches to 1.3e-7.

---

## 11. As built

**M0** (2026-10-04): the three free packs downloaded with the user's go-ahead
(`KayKit_Character_Animations_1.1.zip` 14.9 MB, `KayKit_Adventurers_2.0_FREE.zip` 13.0 MB,
`KayKit_Skeletons_1.1_FREE.zip` 8.2 MB, from kaylousberg.itch.io), unpacked outside the repo.
Findings are in §2.6 and §10: one 23-joint rig with rest poses identical to the clip libraries,
the full clip list, embedded textures skipped by the engine (PNGs used instead). Nothing is copied
into the repo yet; M1 copies the files listed under "Repo footprint".

**M1** (2026-10-04), built by a Sonnet implementer, reviewed by a fresh Sonnet reviewer:

- `examples/Marionette`: `MarionetteLayer` (Title → Arena → End), `ArenaDirector` + `ArenaWorld`
  (a 12-sided floor and wall ring with static colliders, four braziers), `Fighter`, `GameAssets`
  (owns the lit materials: one per PNG, shared by a fighter and its weapons, freed in the layer's
  `OnDetach`), `Moveset` (§2.6 as data, resolved once into a `ClipSet` across the five libraries;
  a name in two libraries warns), `Checks`, `CameraRig`, `LaunchOptions`, `Overlay`, `TitleScreen`.
  Assets: 31 files, 6.48 MiB, licences as `assets/LICENSE-KayKit-*.txt`. Root premake, `.vscode`,
  and the release workflow job plus upload step (13 release assets) are in.
- A library clip plays on a fighter through `Scene::GetAnimator(entity)->Play(clip)`;
  `AnimatorComponent::PlayOnStart` is false, since `DefaultClip` names a clip on the entity's own
  model. `--freeze` poses each fighter at idle + a stagger and disables its animator.
- `--check` logs `Asset checks: 17 passed`: clips found, hand slots, ≤ 128 skin joints, and per
  fighter a retargeted idle that matches the library's own pose on all 23 rig joints and leaves
  the rest pose (4.5e-2). It found the retargeting bug in §10, fixed in the engine first. The
  Skeleton Warrior has one joint of its own, `Skeleton_Warrior_Helmet` (a rigid node), ignored.
- The hips table (`--check`) shows every attack, dodge, hit and death clip moving the hips 0.1 to
  1.1 m along the ground, which sizes M2/M3's `dash` and `lunge` speeds.
- Lighting: four braziers alone washed the textures orange, so a cool directional "moon" (0.75)
  is the key light; braziers 0.8 over 9 m, ambient 0.18.
- `--fixed-dt` rejects values above the scene's 4/60 s step cap; `--bout=0` is the title.
  `--lineup` (M2), `--autoplay`, `--debug-hitbox`, `--seed` (M3) and bouts 2–3 (M4) parse but do
  nothing yet. The End key jumps to the End scene in Debug builds only.
- The checks are read from the log; a failing `--check` doesn't change the exit code.


**M2** (2026-10-04), built by a Sonnet implementer, reviewed by a fresh Sonnet reviewer, fixed:

- A bout holds the player (Knight) and one opponent (`--bout=1..3`; the opponent only idles and
  faces the player until M4). Each fighter has a `CharacterController3DComponent`, a `Brain`
  (`PlayerBrain`, or `DriveBrain` for `--drive=ramp|circle|strafe|wall`) writing a `FighterIntent`,
  and `Locomotion` states: the `Move` blend (`Idle_A` 0, `Walking_A` 0.86, `Running_A` 3.25 m/s,
  derived from the clips' ankle travel during stance) and idle-to-clip blends for
  `Walking_Backwards` and `Running_Strafe_Left/Right` (the strafes travel ±60° from facing, so the
  body yaws up to 35° to match). Zones Forward/Strafe/Backward by travel direction against facing
  with 10° hysteresis; `SwitchZone` aligns the new clip's step marks with the old one's at the frame
  the new state takes over the events. Facing: the opponent between 7 m (enter) and 8 m (leave),
  else the move direction, at 540°/s. Separation removes the closing speed between the fighters.
- `Move` follows the speed the controller actually achieved, so a fighter at a wall stops its feet;
  it falls at most 22 /s, so a sudden stop eases over ~0.15 s instead of snapping the pose.
- Footsteps: walk and run share one phase per foot (run touchdown − 0.03 cycle: the run sounds
  24 ms early, the walk 76–81 ms late), per the engine's "same fraction" rule; zone clips keep
  their touchdowns. The first version used true touchdowns and a game-side filter, which hid a
  doubled step and could lose one on a run-to-walk handover; both are gone.
- Checks: 44 (17 asset + 27 movement): speeds, directions, sweeps up/down/up-down-up, 32 zone-switch
  runs, the first step after 96 switches, a second-difference foot-pop test that must also catch a
  synthetic 10 cm pop, and `--drive=wall` (Move 0.00, no step in the last second). Drives at fixed
  60 Hz: circle 187 and strafe 147 steps, strictly alternating while moving.
- Known: a zone switch at an arbitrary phase can still drop or double a step about 0.3% of the time
  (offline model; a cross-fade isn't phase-locked), so the checks pick phases away from frame ties.


**M3** (2026-10-04), built by a Sonnet implementer, reviewed by a fresh Opus reviewer, fixed, then re-reviewed (Sonnet) on the fix diff:

- `Combat` (swept weapon spheres against socketed hurt spheres; first contact decides the swing;
  one resolution per swing; ±75° block cone), `HitGeometry` (weapon spheres at 0.35/0.65/0.95 of the
  grip → farthest-vertex axis, radius 0.6 × the blade's half-width clamped to 0.06–0.14 m: sword
  0.094), `DuelScript` (`--drive=duel`: both fighters scripted; Hit, Blocked, Parry + riposte,
  Dodged, a 3-hit chain; `--break-hitbox` moves every hitbox into its one-shot's tail),
  `CombatChecks`. Fighter states Locomotion/Attack/Block/Dodge/HitReact/Stagger/Dead as planned.
- Windows in `Rig_Medium_CombatMelee.events` / `Rig_Medium_MovementAdvanced.events` are the values
  the engine derives from the clips under `--check` (`hitbox`: the span around `handslot.r`'s peak
  speed above half of it; `windup`: the first real hand movement to the hitbox; `combo`: hitbox end
  to the one-shot's return; `dash`: the dodge's ground travel above a quarter of its peak; `parry`
  0–0.2 and `iframes` 0.08–0.30 are tuning). The implementer's offline model disagreed on five;
  the engine wins.
- Travel: v0.8 has no root motion, and moving the capsule by a clip's travel while the pose also
  moves the hips double-counted it. The capsule now stays put during a move and pays the move's
  net hips displacement over its fade-out (attacks 0–0.19 m); a dodge adds 0.9 m over `dash`
  (dodges cover ~1.15 m forward, 1.5 m back, 1.4 m sideways). KayKit's dodges carry their travel on
  `root`, not the hips. `lunge` is gone.
- Input buffer (0.2 s) and riposte window count the fighter's own animation time, so hit-stop
  (exactly 70 ms, from the frame after contact) pauses them; the first version lost the chain's
  second press to hit-stop. A parry counts from the raise's first frame; block push is a decaying
  knockback; chip damage never heals. The Knight's chop aims 24° aside (`AimDegrees`) so its arc
  crosses a target ahead; the duel gap is 1.1 m.
- Scripted runs (`--check`, `--drive`, `--fixed-dt`, `--autoplay`) set `UpdateInBackground`: the
  long checks unfocus the window, and an unfocused app pauses, which hung `--check --drive`.
- Verified: 90 checks (17 asset, 27 movement, 45 combat); the duel PASSes with every outcome inside
  its event window and the chain landing at 7.37/7.92/8.27 s; `--break-hitbox` gives 0 outcomes
  from 7 swings. `ValidateMoveset` must re-run on a model reload (M5).
- Left for M5: unpaid travel is dropped when a move is chained, interrupted or followed during its
  fade-out, so a hit mid-dodge snaps the pose up to 0.64 m (carry the unpaid offset over the next
  move's fade-in, and give HitReact/Stagger a ~0.05 s fade-in to pay it over).


**M4** (2026-10-04), built by a Sonnet implementer whose own fresh Opus reviewer's findings it fixed:

- `AiBrain` (tier rows in `AI_TIERS`, one seeded `mt19937` per bout) perceives the opponent only
  through a `Perception` queue `REACTION_TIME` old (0.45 / 0.30 / 0.18 s). `ReachTable` sweeps each
  move's weapon spheres through its `hitbox` against a standing target's hurt spheres (Combat's own
  geometry), giving reach and the time from hitbox open to first contact, which the parry and dodge
  leads aim at. Recruit: walks in, a light every 1.6–2.4 s, never defends. Veteran: light, 2-link
  chain, heavy 30%; blocks a threatening windup with p 0.33 (a block that would land in `parry`
  becomes a step back), punishes whiffs, strafes. Champion: chains and heavies; parries p 0.6 heavy
  / 0.35 light, else dodges p 0.3 if its iframes can still cover the hit, else blocks; ripostes,
  baits, avoids the wall.
- `BoutFlow`: Intro (opponent taunt one-shot, "BOUT n", title, ~2.5 s) → Fight → Knockout (winner
  taunts, "K.O.", 3 s, fade). Win → next bout; loss or draw → the same bout again; bout 3 won → the
  End scene ("VICTORY"), rebuilt on leaving. `MatchState` lives in the layer; the arena scene is
  rebuilt per bout. `Hud`: a second orthographic camera in the arena scene (names, bars with a
  damage trail, "BOUT n / 3", banners, a controls hint on bout 1, fades).
- `--autoplay` (tier-3 player brain), `--tournament=N` (autoplay, fixed 1/60, 8 steps a frame,
  vsync off, muted, no intro/KO, 90 s limit, seeds `--seed`.., a PASS/FAIL line, then quits),
  `--steps-per-frame=K`.
- Verified: 100 checks (17 asset, 27 movement, 45 combat, 11 AI). Tournaments of 10: tier 3 beats
  the Recruit 10/10 (8–10 s, 86–100 health left) and the Veteran 10/10 (11–15 s, 94–100 left);
  against the Champion (mirror, no bar) 7 wins, 1 loss, 2 timeouts (27–90 s). Intro frames and an
  autoplay fight captured with the HUD.
- For M5: the arena camera frames too tight (heads reach the HUD bars) and a near brazier can sit
  between the camera and the fighters; the unpaid-travel carry (M3). For M6 tuning: the Veteran
  barely lands a hit on a tier-3 player, so bouts 1 and 2 feel alike for it.
- Not verifiable by the main session: "a person can beat all three on keyboard and on a pad".


**M5** (2026-10-04), built by a Sonnet implementer, reviewed by a fresh Sonnet reviewer, fixed:

- Live editing: `GameAssets::PollEventChanges` watches each library clip's `GetEventRevision()`; a
  change re-runs `ValidateMoveset`, invalidates the `ReachTable`, clears the AI's perception and
  plan, and re-evaluates frozen poses (`[Reload] ...` log). `--live-edit-demo` copies `assets/` to
  `%TEMP%/MarionetteLiveEdit/assets` (absolute, no links, neither path inside the other), plays
  from it with hot-reload on, and after 10 s rewrites the copy's slash `hitbox` 0.37..0.47 →
  0.62..0.72 (temp file + rename; a 5 s reload timeout logs an error). Verified: the reload lands
  0.56 s after the write, and a frozen `--debug-hitbox` pose at 0.42 s turns from red to grey
  (1,534 px, all on the sword's spheres).
- Camera: `FollowCamera::Fit` fits feet and head + margin into the band below the HUD;
  `ArenaWorld::UpdateOcclusion` hides a brazier or wall piece on a camera→fighter segment (its
  light stays on) and restores it 0.35 s after the line clears. Non-finite framing is skipped.
- `MoveTravel` carries a move's unpaid travel into the next state's fade-in on chains, interrupts
  (hit, stagger, death, taunt) and moves started during a fade-out; HitReact and Stagger fade in
  over 0.05 s. Checks: a dodge cut at mid-dash (no extra distance) jerks at most 0.031 m carried
  against ≥ 0.121 m uncarried (limit 0.08); distance conservation at three release points; a real
  `Fighter` banks the carry when hit.
- Audio: K.O., win and lose stings; a crackle loop per brazier (stopped with the world); muted in
  tournaments. `Showcase`: the title (four fighters idling) and the End scene (the Knight taunting;
  `--end` opens it with demo values); every scene is rebuilt on leaving.
- Verified: 105 checks (17 asset, 27 movement, 50 combat, 11 AI), the duel PASSes, tournaments
  unchanged (tier 3 beats tiers 1 and 2 10/10). The main session loosened the victory framing
  (the heading overlapped the Knight).
- Left: a full model reload mid-move keeps that move's return step and a skeleton-id change resets
  the animator's layers (rare, dev-time only); the carry after hit-stop is paid over the 0.05 s
  fade, a quick but continuous slide.


**M6** (2026-10-05): a fresh Opus milestone review of all of P13
(`.claude/reviews/2026-10-04-marionette-review.md`: no Critical or High; 2 Medium, 9 Low, packaging and
conventions — all fixed), plus:

- `--freeze` implies a fixed delta (on a real delta, frozen captures differed by 400k px run to run as the
  controllers settled; with it, Vulkan run to run is 0 px and D3D11/D3D12 are 89 px from Vulkan).
- `--perf` (Release: ~0.95 ms a frame, `Scene::OnUpdate` 0.04 ms; Debug: ~4.3 ms) and `--player-tier=1..3`,
  which turns tournaments into a ladder (the stronger tier must win 8 of 10).
- Tuning: the Veteran gained a guard stance and 160 health. Open: a tier-1 AI on the Knight still beats it,
  because the Knight's sword out-ranges the two-handed axe (see the review's runtime section).
- Check-only constants moved to `CheckTuning.h`; `TitleScreen` became `Screens`; the weapon glTFs share the
  characters' PNGs.
- Docs: README (features, Marionette row, credits for KayKit and the Fox), ROADMAP, ROADMAP-BACKLOG §8,
  CLAUDE.md, `docs/animation.md` ("Combat windows from events").
- Verified: 107 checks, the duel, the live edit, the backends, the ladder, perf, the bout-1 hint.

---

## 12. Plan review (2026-10-04)

A fresh Sonnet reviewer, read-only, checked every anchor in §1, §2 and §10 (all correct) and the
design against `Animator.cpp`. Its findings, all folded into the sections above:

| # | Severity | Finding | Where it went |
|---|---|---|---|
| 1 | High | Death as a one-shot stands back up when the one-shot returns | §2.2 Death: a non-looping `Play`, layer 1 stopped |
| 2 | High | The layer-event rule tests the `SetLayer` weight, not the fade; a looping `Melee_Block` loses its t ≈ 0 `parry` | §1 Events, §2.3 Block (`SetLoop(false)`, then `Melee_Blocking` on `IsFinished`), §8 |
| 3 | Medium | A one-shot's marks in its last `fadeOut` seconds never fire | §1 One-shot tail, §2.1, §2.2 (load-time warning) |
| 4 | Medium | A cancelled attack's `hitbox` stays open through the fade | §2.1 (hits gated on the `Attack` state), fade-in 0 for `HitReact`/`Stagger` |
| 5 | Medium | `Melee_Block_Hit` on layer 0 would be hidden by the block layer | §2.3 Block (a layer-1 one-shot; riposte stops layer 1 first) |
| 6 | Medium | Facing the move direction makes every dodge `Dodge_Forward`; travelling hips slide off the capsule | §2.3 Facing, Dodge (`dash`), M1's hips-translation log |
| 7 | Medium | The Knight's heavy was a 2H clip | §2.6 per-fighter table |
| 8 | Medium | Hit-stop overwrote the pace held in `Speed`; two things named "Speed" | §2.2 Hit-stop restores the pace; the blend parameter is `Move` |
| 9 | Low | Weapons need lit materials too | §2.3 Weapons |
| 10 | Low | Varying `dt` made `--autoplay` unrepeatable | §3.3 `--fixed-dt`, AI judged over 10 seeds |
| 11 | Low | Retargeted playback was never run | M1's exit checks |
| 12 | Low | Stale D5 status, one licence file for three packs, a crowd sound and "four fighters" left from D4, `Stagger`/death/taunt clips unassigned | fixed in place |
