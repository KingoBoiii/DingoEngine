# Code review — P13 *Marionette* (the v0.8 example game)

**Date:** 2026-10-04 · **Branch:** `claude/v0-8-animation-character-fidelity-7209eb` at `ae6d59b` · **Scope:** the
whole example as committed in M1–M5 (`git diff 368caff..ae6d59b -- examples/Marionette`), plus its release job,
the root premake include and the `.vscode` entries. Plan: `.claude/plans/2026-10-03-marionette-plan.md`.

**Method:** one fresh read-only Opus reviewer who wrote none of the code, after each phase (M1–M5) had already had
its own fresh review and fixes (recorded in the plan's §11). This milestone pass looked for what phase reviews
miss: cross-phase interactions, lifetime across the whole Title → bouts → End → Title loop, and what a player
can trigger in a full playthrough. Runtime facts it started from: `--check` 105/105, the scripted duel PASS,
tournaments (tier 3 beats tiers 1 and 2 10/10), the live-edit demo and the screens. Nothing was built or run by the
reviewer.

---

## Summary

**No Critical or High findings.** Nothing outlives a `Scene::Clear`, `OnStop` or `OnStart`: controllers and
animators are looked up every frame; the fighter scripts, fighters, HUD, world and camera die together; the
brazier loops stop with the world; game-created materials, meshes and fonts are freed while the renderer is still
alive; the match state outlives the scenes; stale `RangeEnd`s are dropped. A K.O. or a death during hit-stop is
handled. Packaging is correct (all of `assets/`, the exe finds it beside itself, no `imgui.ini`/`Dingo.log`).
Nothing in the game code is Vulkan-only.

Two Medium: tapping block every ~0.2 s parries almost every attack ([M1](#m1)), and `--hot-reload` pauses while
the editor has focus, so a person's `.events` edit only lands on refocus ([M2](#m2)).

---

## Fix status {#fix-status}

| ID | Severity | Finding | Status |
|---|---|---|---|
| [M1](#m1) | Medium | Tapping block keeps a parry window open almost all the time | fixed: `BLOCK_PARRY_COOLDOWN` 0.4 s; combat check `CheckParryCooldown` |
| [M2](#m2) | Medium | `--hot-reload` isn't live while the editor has focus | fixed: `--hot-reload` sets `UpdateInBackground` |
| L3 | Low | A chained swing can borrow the previous swing's open hitbox when `combo` opens before `hitbox` ends | fixed: `IsHitboxLive` needs the move's own range; `ValidateMoveset` warns; LiveEdit moves `combo` too |
| L4 | Low | Fonts, arena materials, the flame mesh and debug materials are rebuilt on every scene entry (Title → Arena freezes on the key press) | fixed: loaded once in `GameAssets` (font through the AssetManager) |
| L5 | Low | Menu input: F-keys, modifiers and the refocus click count as "any key" (and as a light attack mid-bout); Esc on leaving a bout can also quit from the Title | fixed: F-keys, modifiers and the refocus click ignored; 0.5 s Esc grace on the Title |
| L6 | Low | The input buffer only feeds chains; a press at the end of a hit reaction, stagger or dodge is dropped | fixed: locomotion consumes the buffer |
| L7 | Low | Dodging into the wall or the opponent sinks the pose into it and snaps back | fixed: the extra distance is scaled by the free distance; the next direction if the pose alone would cross |
| L8 | Low | Flag combinations: `--end` demo values leak into a real run; `--tournament --end`; `--lineup` alone opens the Title | fixed; also `--freeze` now implies a fixed delta |
| L9 | Low | The AI's early-block rule uses a check constant, not `Melee_Block`'s `parry` range | fixed: read from the clip through the reach table |
| L10 | Low | Per-frame costs: the camera fit scans 6.5→24 m every frame; the HUD formats its label every frame; a zone switch copies the animator twice | fixed (camera, HUD); the zone-switch copy kept, as the cheaper read isn't identical at phase 0 |
| L11 | Low | A failed library or character load leaves a bout that can never end | fixed: an "assets missing" banner, any key → Title; scripted runs close |
| P1 | Low | `weapons/*.png` duplicate `characters/*.png` (loaded twice) | fixed: the weapon glTFs point at `../characters/` |
| P2 | Low | The live-edit temp copy is never cleaned up | fixed: removed on detach under the same guards |
| C1 | Nit | Dead code and duplicates (`GetOpeningMax`, `k_FighterCount`, `Rotate`, `k_Infinity`), constants outside GameTuning, check constants mixed into GameTuning, `TitleScreen` hosts the End script | fixed: `GameMath`, `CheckTuning.h`, `Screens.*` |

### Runtime checks after the fixes {#runtime}

Run by the main session on 2026-10-05 (Vulkan unless noted):

| Check | Result |
|---|---|
| `--check` with `--drive=duel` | **107 / 107** (17 asset, 27 movement, 51 combat incl. the parry cooldown, 12 AI incl. the Veteran's guard) and the duel PASS |
| `--live-edit-demo` | hitbox *and* combo moved in the temp copy, reload seen after 0.54 s, the copy removed on exit |
| Backends, `--freeze --debug-hitbox --pose=...@0.42` (fixed delta) | Vulkan run to run **0 px**; Vulkan vs D3D11 and vs D3D12 **89 px** (max delta 12) |
| Perf, `--autoplay --bout=2 --perf --vsync=off`, 3 runs each | Release: frame 0.91–0.99 ms, `Scene::OnUpdate` 0.04 ms, render 0.34–0.36 ms. Debug: frame 4.2–4.5 ms, update 0.83–0.86 ms, render 2.8–3.0 ms |
| AI ladder, `--tournament=10 --player-tier=N` (Release) | tier 2 beats the Recruit 10/10; tier 3 beats the Recruit and the Veteran 10/10; the Champion beats tier 2 9/10; Champion mirror 4–5 with 1 timeout |
| The controls hint on bout 1 | fits one line at 1600×900 |

**Tuning (part of M6):** the Veteran never blocked (a block that would land inside the parry window turned into
a step back, which never escapes a light). It now has a guard stance (raises its block early while in range,
p 0.5, held 0.6–1.2 s, dropping it to punish) and 160 health (was 110). It blocks 1–6 and parries up to 6 times a
bout. **Open (balance):** a tier-1 AI on the Knight still beats the Veteran 10/10. The Knight's sword out-ranges
the two-handed axe, so the Knight's AI swings first whatever brain it runs; the Veteran's lights never land, only
its heavies. A person faces a sturdier, guarding Veteran (bouts twice as long), but the ladder can't show the
step from the Recruit. A later pass would make the Veteran reach-aware (close inside the Knight's reach before
its own wind-up) or give two-handed lights more reach.
Not filed as a finding: every example ships `arialbd.ttf` (Microsoft's Arial Bold). That's a repo-wide licensing
question, older than Marionette, raised with the user separately.

---

## Medium

### M1 — Tapping block keeps a parry window open almost all the time {#m1}

`Fighter.cpp:385`, `575-599`, `682-685`; the HUD hint at `Hud.cpp:14` says "tap to parry". CONFIRMED.

Releasing block lowers it at once, and the next press raises a fresh `Melee_Block`. A raise counts as a parry from
its first frame for 0.2 s. So re-pressing every ~0.2 s parries nearly every attack: no damage, the attacker
staggers, and the riposte is free. It trivialises all three tiers.

**Fix:** a `BLOCK_PARRY_COOLDOWN` (~0.4 s of the fighter's own time) after lowering; a raise inside it gets no
parry window.

### M2 — `--hot-reload` isn't live while the editor has focus {#m2}

`main.cpp:61,85`. CONFIRMED.

Only scripted runs set `UpdateInBackground`. A person editing `.events` has the editor focused, so the game is
paused, `AssetManager::Update` never polls, and the edit lands only on refocus. M5 verified the live edit with
`--live-edit-demo`, which is scripted, so it never paused.

**Fix:** `UpdateInBackground` also when `--hot-reload` is on.
