# Hearthvale

The v1.0 release game of DingoEngine: a cozy farm-and-build sim. You start at a farmhouse at the edge of a
valley, clear the land, till, plant, water and harvest through a four-season calendar with weather, ship
your crops through the bin and the shop, build fences, paths, lamps, sprinklers and buildings, and get to
know the villagers and animals. Everything is drawn through DingoUI, with a pad or a keyboard and mouse.

Run it with its own directory as the working directory, so `assets/` resolves:

```
cd examples/Hearthvale
../../build/bin/Debug-linux-x86_64/Hearthvale/Hearthvale
```

On Windows the executable is `build\bin\Debug-windows-x86_64\Hearthvale\Hearthvale.exe`. A released build
carries its assets as one encrypted `Hearthvale.dpak` beside the executable.

## How to play

| Action | Gamepad | Keyboard and mouse |
|---|---|---|
| Move | Left stick | WASD |
| Use the tool, or place | A | Left click, or Space (hold to keep going) |
| Interact (talk, open, harvest) | X | Right click, or F |
| Previous / next hotbar slot | LB / RB | Mouse wheel, or 1-9 |
| Rotate the placement | Y | R |
| Cycle the seed | D-pad up | T |
| Rotate the camera | D-pad left / right | Q / E |
| Zoom | Right stick up / down | Ctrl + mouse wheel |
| Inventory | View (Back) | Tab |
| Build menu | B | B |
| Pause | Menu (Start) | Esc |

The target tile is the one in front of the farmer, shown as an outline. With a mouse, the hovered tile
within reach of the farmer is the target instead. Every screen can be used with a pad alone.

The game saves at the start of each day, into the active one of three slots, and the pause menu has
"Save game". `--save-dir=<path>` points the saves elsewhere.

## Flags

| Flag | Effect |
|---|---|
| `--check` | Runs the check suites and logs `[PASS]`/`[FAIL]` lines, then `[Checks] <suite> N passed, M failed` per suite and the total. Headless it exits by itself; windowed the game stays open afterwards |
| `--autoplay` | The farmer bot plays through the same player intent the input fills |
| `--days=N` | Plays N days (forces `--autoplay` and a fixed step) and closes, logging one `[Day]` line per day with the state hash and the process's private memory |
| `--fixed-dt=<s>`, `--steps-per-frame=K` | A fixed simulation step, and how many run per frame (needs a fixed step) |
| `--seed=N` | Valley and weather seed |
| `--day=N`, `--season=spring\|summer\|fall\|winter`, `--time=HH:MM` | Start a fresh game at that point |
| `--weather=clear\|rain\|snow` | Forces the weather |
| `--load=<slot>` | Loads slot 1 to 3, or a slot's file name |
| `--layout=demo` | A pre-built late-game farm: every building placed, every crop planted (for captures and perf) |
| `--perf` | With `--autoplay`, logs `[PERF]` frame and stage timers over 600 frames, then closes |
| `--freeze` | Stops time and animation. With `--check`, the checks that need the clock, the villagers or the animals to move are left out, and the clock stays stopped after a screen closes |
| `--farmer=model\|box` | The skinned farmer, or a box placeholder |
| `--no-post`, `--no-shadows`, `--no-particles`, `--no-fog` | Switch a rendering feature off |
| `--no-instancing`, `--no-static`, `--no-cull` | Switch a v1.0 throughput feature off |
| `--vsync=on\|off` | VSync (off by default under `--autoplay` and `--perf`) |

The engine's own flags apply too: `--graphics=vulkan|dx11|dx12|headless`, `--window=WxH`, `--save-dir=<path>`,
`--log-dir=<path>`, `--vulkan-validation`.

### Checks and autoplay

```
cd examples/Hearthvale
Hearthvale --check --graphics=headless    # the simulation suites; exits by itself
Hearthvale --check                        # all nine suites, with a window; closes with the window
Hearthvale --days=28 --seed=1 --graphics=headless
Hearthvale --layout=demo --freeze --autoplay --perf
```

A passing run has no `[FAIL]`, no `VUID-` and no `LOG ERROR` in its output. Headless runs cover the pure
simulation (valley generation, placement, growth, economy, saves, determinism); Assets, Static and UI need
a window, which is why CI runs `--check` both ways. Two `--days=7 --seed=1` runs log identical `[Day]` lines.
Scripted runs keep updating while the window is unfocused. The `[PERF]` numbers mean something only on a
real GPU in a Release build.

## Credits and licences

Everything below is CC0 unless stated; each pack's licence text is kept beside it in `assets/`.

| What | Author | Licence | Where |
|---|---|---|---|
| Nature Kit 2.1, Survival Kit 2.0, Fantasy Town Kit 2.0, Cube Pets 2.0 (3D models) | Kenney, [www.kenney.nl](https://www.kenney.nl) | CC0 | `assets/kenney/` |
| Interface Sounds 1.0, RPG Audio (sound effects) | Kenney | CC0 | `assets/kenney/` |
| KayKit Adventurers 2.0 (Barbarian, Knight), Character Animations 1.1 | Kay Lousberg, [www.kaylousberg.com](https://www.kaylousberg.com) | CC0 | `assets/kaykit/` |
| Fredoka SemiBold (headings) | The Fredoka Project Authors, [Fredoka](https://github.com/hafontia/Fredoka-One) | SIL OFL 1.1 | `assets/fonts/`, `LICENSE-Fredoka.txt` |
| Inter Bold (body text) | The Inter Project Authors, [Inter](https://rsms.me/inter) | SIL OFL 1.1 | `assets/fonts/`, `LICENSE-Inter.txt` |
| Music, ambience and the other sound effects | Synthesized for this project | same as the repository | `assets/audio/` |

The engine's own components and their licences are in [THIRD-PARTY-NOTICES.md](../../THIRD-PARTY-NOTICES.md).
