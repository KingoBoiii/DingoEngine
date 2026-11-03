# Hearthvale review (PR #261, `feat/1-0-hearthvale`)

Reviewer: a fresh review agent with no context from the work. It read the whole diff against `origin/master`, concentrating on `examples/Hearthvale/src` and the build, CI, docs and scripts wiring. Plan §11.2–11.17 "As built" notes took precedence over the design.

Verification run: `Hearthvale --check --graphics=headless` (Debug) exited 0, with 856 passed and 0 failed in 18 suites, in 5.8 s. The High finding below was reproduced with a scratch harness that copies `ValleyWorld`'s cell and occupant bookkeeping onto real `MakeNewGame` grids. The harness is outside the repo. All other findings come from reading the code.

Counts: **High 1, Medium 1, Low 13.**

---

## High

### H1. Replacing the game leaves hundreds of occupants with no visual
**Where:**
- `examples/Hearthvale/src/World/ValleyWorld.cpp:89-98`: the removal loop in `RebuildCell`.
- `World/WorldCells.cpp:115-123`: `DirtyCells::Drain` sorts cells ascending.
- `HearthvaleLayer.cpp:109-130`: `OnGameReplaced` only calls `m_World->MarkAll()`.

**Why it happens:**
- Occupant ids are never reused within one game, but every game numbers them from 1: landmarks first, then a row-major decor scatter.
- `RebuildCell` keys `m_Occupants` by id alone and removes a cell's old ids by id.
- After a replace, `MarkAll` rebuilds every cell in ascending order. Take an id X that sat in cell B in the old game and sits in a lower-numbered cell A in the new game:
  1. `RebuildCell(A)` finds X, sees a different snapshot, destroys the old visual and spawns the new one.
  2. `RebuildCell(B)` still has X in `state.Occupants`. X is not in B's new id list, so the loop finds `m_Occupants[X]` (now cell A's fresh visual), destroys it and erases the entry.
- X has no entity from then on. Nothing marks cell A dirty again, so it never comes back. The occupant is still in the grid, so it blocks walking and placement while invisible.

**When it happens:**
- Every ordinary start goes through this. At launch the seed-1 valley is built and synced behind the title screen (`HearthvaleLayer.cpp:89`, `:343`). Title, then New game or Continue, then a slot calls `GameSaves::StartNew` or `GameSaves::Load` (`ScreenHostMenus.cpp:299-300`, `GameSaves.cpp:89-131`).
- New Game takes its seed from `std::random_device`, and a loaded save is a different valley, so the two grids always differ.
- Only `--load=` at launch escapes, because it runs before `BuildValleyScene` (`HearthvaleLayer.cpp:61`).

**Evidence (harness):** occupants left without a live visual, going from the launch valley (seed 1) to:

| Target seed | Occupants left without a visual |
|---|---|
| 12345 | 1596 of 4165 |
| 2 | 1176 of 4013 |
| 99 | 1699 of 4150 |
| 7 | 1804 |
| 3735928559 | 1686 |

**Why the checks miss it:** `ValleyWorld` is never constructed headless, and no windowed check replaces the game under a live world and then counts visuals.

**Fix (any one of these):**
- Add `ValleyWorld::Reset()`. It destroys every occupant visual, clears `m_Occupants` and each `CellState::Occupants`, then `MarkAll()`. Call it from `OnGameReplaced`.
- Or, in the removal loop, skip the destroy when the entry now belongs to another cell. For example, check `m_Dirty.CellOfOccupant(it->second.Snapshot) != cell`, or keep the owning cell in `OccupantVisual`.
- Or run removals for all drained cells before any spawns.

Add a windowed check that calls `StartNew` with a seed other than 1 and asserts that every grid occupant has a valid entity in `m_Occupants`.

---

## Medium

### M1. A loaded animal keeps the previous game's model when its id matches
**Where:**
- `examples/Hearthvale/src/Life/AnimalWorld.cpp:58-88`: `Sync` keys pieces on `animal.Id` and removes a piece only when `herd.Find(id)` fails.
- `Life/AnimalWorld.h:28`: `Piece::Kind` is stored in `Create` (`AnimalWorld.cpp:28`) but never compared.
- `Farm/Animals.cpp:46-49`: `Herd::Clear` resets `m_NextId = 1`.
- `HearthvaleLayer.cpp:109-130`: `OnGameReplaced` resets `AnimalSim` but not `AnimalWorld`.

**Scenario:** In the current game, animal 1 is a chicken. The player loads a slot whose animal 1 is a cow. The piece survives because id 1 still exists. The scene keeps showing the chicken body and its produce marker, while the sim, the produce, the pen capacity and the audio treat it as a cow. The chicken's cluck positions come from `AnimalSim` motions, so they follow the new kinds, but the rendered model is wrong. The same happens with New Game whenever the new herd's ids overlap with the old herd's.

**Fix:** In `Sync`, destroy and recreate the piece when `piece.Kind != animal.Kind`. Or add an `AnimalWorld::Clear()` and call it from `OnGameReplaced`. Extend the Life scene check to replace the herd and compare each piece's kind.

---

## Low

### L1. Night lights keep a stale site when a lamp id moves
**Where:** `examples/Hearthvale/src/Look/NightLights.cpp:71-102`. `Rescan` keys `m_Sites` by occupant id and only creates a site when the id is new.

**Scenario:** Load slot A, which has a lamp with id N at (10,10). Then load slot B from the same valley, where id N is a lamp at (40,12). The light, bulb and sparks stay at (10,10). This is the same id-without-snapshot pattern as H1, but rarer: it needs two saves of one seed that diverged.

**Fix:** Store the position or occupant snapshot in the site and recreate it when they differ. Or clear all sites in `OnGameReplaced`, which already sets `m_LightsSeenCells = ~0ull`.

### L2. Replaced SFX wavs are still on disk, still generated and still packed
**Where:**
- The files: `examples/Hearthvale/assets/audio/ui_click.wav`, `ui_close.wav`, `ui_coin.wav`, `ui_error.wav`, `ui_focus.wav`, `ui_open.wav` and `tool_axe.wav`.
- The code now uses Kenney clips (`Audio/GameAudio.cpp:22-30`), so nothing in `src/` references these seven files.
- `scripts/generate_hearthvale_audio.py:130-164` still writes them.
- `scripts/pack-assets.sh` packs all of `assets/`, so they ship in `Hearthvale.dpak`.

**Fix:** Delete the seven wavs and their `write_wav` calls.

### L3. Three Kenney packs ship but nothing uses them
**Where:**
- The packs: `examples/Hearthvale/assets/kenney/ui-pack` (3.7 MB, 877 files), `kenney/game-icons` (4.2 MB, 421 files) and `kenney/ui-audio` (424 KB).
- Their only reference is their credits rows in `Ui/CreditDefs.cpp`.
- Plan §11.17 records game-icons as skipped.

**Scenario:** They add about 8.3 MB to the repo, the `.dpak` and every release archive for no visible content.

**Fix:** Remove the three folders and their `CreditDefs` rows together. `Checks/MenuChecks.cpp:606-628` requires every `kenney/` folder to have a credits row, so the rows must go with the folders. Also update `THIRD-PARTY-NOTICES.md` if it lists them.

### L4. The migration test fixture ships in the game package
**Where:** `examples/Hearthvale/assets/test/saves/hearthvale-v0.json`. `pack-assets.sh` packs all of `assets/`.

**Fix:** Move the fixture out of `assets/`, for example to `examples/Hearthvale/test/`, and read it from there in the check. Or have the packer skip `assets/test/`, which would be a change to the shared script, so it needs the owner's call.

### L5. The save backup is not tried when the primary file is missing and the .tmp is unusable
**Where:** `examples/Hearthvale/src/Save/SaveStore.cpp:81`, which reads `if (data && source == path && std::filesystem::exists(backup, ec))`.

**Scenario:** The primary file is missing after a crash between rename steps. `SaveData::ReadFile` falls back to the `.tmp`, which parses as JSON but fails `ReadSave`, for example because it is truncated at a field boundary. Since `source != path`, the `.bak` is never tried and the slot reports "damaged", even though a good backup exists.

**Fix:** Try the `.bak` whenever `source != backup`, not only when `source == path`.

### L6. Save decoding accepts some inconsistent grids
**Where:** `examples/Hearthvale/src/Save/SaveGame.cpp`:
- `DecodeGrid` (`185-272`) checks ranges, def ids, the `Built` range, duplicate tiles and occupant order and overlap. It does not compare an occupant's W/H/rotation against `RotatedFootprint(def)`, does not reject crops on non-soil ground or under a non-greenhouse occupant, and does not reject `Built` on ground that is not a path.
- `DecodeBoard` (`474`) does not check `Done` against `Progress`.
- `DecodeHerd` (`550`) does not check that an animal's home is an existing pen.

**Scenario:** Only a hand-edited or bit-rotted save reaches this, but the failures are invisible: for example, an occupant whose footprint differs from its model, or a pen-less animal.

**Fix:** Add the three grid checks and the two cross-checks, each failing with a clear error so the slot reads "damaged".

### L7. A local is named `near`
**Where:** `examples/Hearthvale/src/HearthvaleLayer.cpp:528-531`, `const std::optional<VillagerPick> near = ...`.

**Problem:** CLAUDE.md says to never name anything `near` or `far`, because `<Windows.h>` defines them as macros. It compiles today only because nothing in the include chain pulls in `<Windows.h>`.

**Fix:** Rename it to `nearby` or `pick`.

### L8. Chicken positions are scaled by `TILE_SIZE` twice
**Where:** `examples/Hearthvale/src/HearthvaleLayer.cpp:435`, `frame.Chickens.emplace_back(motion.Position.x * TILE_SIZE, ...)`.

**Problem:** `AnimalMotion::Position` is already in world units; `AnimalWorld.cpp:92` uses it unscaled. With `TILE_SIZE = 1.0f` there is no effect, but any other tile size would move every cluck.

**Fix:** Drop the `* TILE_SIZE`.

### L9. Play time keeps counting while the game is paused
**Where:** `examples/Hearthvale/src/HearthvaleLayer.cpp:600-601`. Time is added whenever the title screen is not open, before the director decides whether to advance.

**Scenario:** Leaving the pause menu, options or the sleep fade open for ten minutes adds ten minutes to the slot's "Play time".

**Fix:** Add the time the director actually advanced, or skip `AddPlayTime` while `ScreenHost` pauses the sim. If wall time is the intended meaning, the slot label should say so.

### L10. Selling or binning with zero held shows "Bad amount"
**Where:** `examples/Hearthvale/src/Ui/ScreenHostMenus.cpp:416-419` (shop sell-back) and `:453-455` (bin).

**Problem:** `count = min(quantity, held)` is 0 when nothing is held. `ShopSellItem` and `bin.Ship` then return `BadCount`, which `Farm/Economy.cpp:23` shows as "Bad amount" instead of "Not enough items".

**Fix:** Return `TradeResult::NotEnough` when `held == 0` before clamping. Or check that in the economy functions before the count check.

### L11. Comments refer to the plan and to change history
**Where:** CLAUDE.md's comment policy forbids change-tracking and narration. These comments point at plan sections or phases that the code does not need:
- Plan or phase references:
  - `examples/Hearthvale/src/Farm/Board.h:44` (plan 2.1)
  - `Farm/Friendship.h:21` (plan 2.2)
  - `Farm/EconomySim.h:15` and `Farm/FarmSim.h:19` (plan 3.2)
  - `ValleyDirector.h:15` (plan 3.2)
  - `World/ValleyWorld.h:30` (plan 11.5)
  - `Checks/UiChecks.h:15`, `Checks/LifeWorldChecks.h:15` and `Checks/StaticChecks.h:14` (plan 5)
  - `GameTuning.h:20` (plan 1.4, E9)
  - `Life/PlayerIntent.h:7` (P13)
- Change-tracking: `Life/PlayerInput.cpp:51`, "the plan gave both to E".

**Fix:** Delete the parenthesised plan references and the history clause, and keep the "why" sentences.

### L12. Formatting slips
**Where:**
- `examples/Hearthvale/src/HearthvaleLayer.cpp:313-317`: the Camera stage lambda's `if` body opens its brace one indent too deep.
- `Audio/GameAudio.cpp:22-30`: the `k_Sfx` table's columns lost their alignment when the Kenney paths replaced the wav paths.

**Fix:** Reindent both.

### L13. Script nits
**Where:**
- `scripts/ci/linux-smoke-test.sh:4-8`: the header comment now wraps mid-sentence ("…Marionette's --check and / Hearthvale's --check (headless, then windowed) / from an existing build…"). It also names "Marionette's --check" twice, at line 4 and again at line 7 in the packed-run clause, and reads as if Marionette runs twice unpacked.
- `scripts/icons/icons.txt:11`: the Hearthvale row sits after Marionette, which breaks the file's alphabetical order.

**Fix:** Rewrap the header, for example "…every example, Marionette's and Hearthvale's --check (Hearthvale headless, then windowed)…". Move the icons row above Marionette.

---

## Checked and clean
- **Root `premake5.lua`:** includes Hearthvale.
- **`examples/Hearthvale/premake5.lua`:**
  - calls `copyAssimpRuntime()`, then `embedPackKey()`, then `exeMetadata { ... }` with the same Company and Copyright as the other examples
  - `dpak.key`, `icon.ico` and `icon.png` are present
- **Release workflow (`.github/workflows/build-release.yml`):** the Hearthvale Windows upload and the `example-game-hearthvale` job mirror Marionette's. Linux packaging loops over every example.
- **`.vscode`:** `tasks.json` and `launch.json` have Hearthvale entries.
- **Smoke test:** adds the headless and windowed `--check` runs, with `EXPECT_LOG` on the pass line.
- **Linux tester:** `scripts/linux-tester/run-checks.sh` and its README row are added.
- **Docs and notices:** README, ROADMAP and project-history updates are consistent. The Fredoka notice URL matches the copyright line in the font's own OFL text.
- **Path rules:** no `path.string()` or `generic_string()`. Paths reach text only through `FileSystem::PathToUtf8`. `MenuChecks.cpp:608` and `:626` build paths from ASCII literals only.
- **`<Windows.h>` and literals:**
  - no `<Windows.h>`
  - the unparenthesised `std::min`/`std::max` calls in the private `CameraMath`, `CameraRig`, `LampLod`, `NightCurve` and `AudioMix` files are harmless there
  - no non-ASCII literals and no TODOs
  - `SfxDef::Near`/`Far` are capitalised fields, so they are fine
- **Engine and vendor:** no engine source and no `vendor/` changes. The diff outside `examples/Hearthvale` is build wiring, docs, notices and scripts only.
- **Sim logic:** `Crops.cpp` (till, water, plant, harvest, regrow, withering at the season change, sprinklers, rain) and `Economy.cpp` (shop and bin).
- **Day roll:** `EconomySim` (shipping on the ending day, board settled on the new day) and `FarmSim` (the coin loss on passing out).
- **Sleep and screens:** `SleepFlow` and its pause interplay with `ScreenHost` (`ScreenHost.cpp:187-201`; `blocked` stops screens opening during fades).
- **Board, Friendship and Animals:**
  - `Board.cpp`: Give is disabled when progress is full, at `MenuPages.cpp:373`. A wrong-villager gift being consumed is intended, per `BoardChecks.cpp:151`.
  - `Friendship`, `VillagerDialogue` and the Animals sim: keyed by ids, not addresses.
- **Placement and interaction:** `Placement.cpp`, `BuildController.cpp`, `Tools.cpp`, `PlaceInteract.cpp` and `Interactions.cpp`.
- **`TileGrid`:** occupant ids stay ascending, erasing keeps the order, and `FindOccupant` is a binary search.
- **`ValleyDirector::Restore`:** move-assigns into the same `m_Grid`, so the references held by `ValleyWorld` and `NightLights` stay valid.
- **Save writes:** go through the engine's atomic `SaveData` (`.tmp`, then rename, with a `.bak` kept). `DecodeGrid` range-checks `Built` before `RemoveBuilding` uses `Built - 1`.
