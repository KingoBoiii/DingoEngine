# DingoEngine on Linux: tester bundle

Thanks for testing! DingoEngine is a C++ game engine that has so far only been run on Windows
and in a headless Linux CI (a virtual X server with a software GPU). It has never run on a
real Linux desktop with a real GPU. This bundle holds the engine's test app and ten small games, built for
x86-64 Linux. Everything runs from this folder, nothing gets installed, and you can delete the
folder when you're done.

It takes about 10 minutes for the automatic part (step 2) and as long as you like for the rest.

## 1. Requirements

- A distro from 2024 or newer: glibc 2.38+ and GCC 13's libstdc++. That covers Ubuntu 24.04+,
  Mint 22+, Pop!_OS 24.04+, Debian 13, Fedora 39+, Arch, Manjaro and openSUSE Tumbleweed. Older
  distros fail at start with `GLIBC_2.38 not found`; if that happens, just tell me.
- A Vulkan driver, plus the tools the checks use:

  | Distro | Command |
  |---|---|
  | Ubuntu, Mint, Pop!_OS, Debian | `sudo apt install libvulkan1 mesa-vulkan-drivers vulkan-tools python3-xlib` |
  | Fedora | `sudo dnf install vulkan-loader mesa-vulkan-drivers vulkan-tools python3-xlib` |
  | Arch, Manjaro | `sudo pacman -S vulkan-icd-loader vulkan-tools python-xlib` plus your GPU's driver: `vulkan-radeon`, `vulkan-intel` or `nvidia-utils` |

  On NVIDIA's own driver the Vulkan part comes with the driver, so you don't need
  `mesa-vulkan-drivers` there. `vulkaninfo --summary` should list your GPU.

Unpack the bundle and open a terminal in it:

```sh
tar -xzf DingoEngine-linux-tester.tar.gz
cd DingoEngine-linux-tester
```

## 2. Automatic checks

```sh
./run-checks.sh
```

This opens each of the 15 test-app pages and 10 games in turn, for 15 seconds each, then closes
it the way clicking the window's X would. Leave the mouse and keyboard alone while it runs:
windows will pop up and close by themselves. Most of the test pages check their own results,
and the script also looks for crashes, hangs on close and Vulkan errors. At the end it
benchmarks one game for 30 seconds and writes `dingo-linux-report-<date>.tar.gz`. **Please send me that
file.** It holds a summary, the system details (distro, GPU, driver, monitors, audio server)
and one log per program. The first time each program starts, it may take a few seconds longer,
because it compiles its shaders.

## 3. Play

Start anything with `./play.sh <name>`, e.g. `./play.sh Marionette`. Run `./play.sh` on its own
to list the programs. Each run also saves its log in `logs/`.

| Program | What it is | Controls |
|---|---|---|
| FlappyBird | Flappy Bird | Space flaps, starts and restarts. Esc quits |
| SpaceInvaders | Space Invaders | A/D or arrows move, Space fires and starts. Esc quits |
| AngryBirds | slingshot physics | Drag with the left mouse button and let go to fire. Space/Enter/click continue. Esc quits |
| Breakout3D | 3D Breakout | A/D or arrows move, Space launches and restarts, **F11 toggles fullscreen** |
| DungeonCrawler | 2D dungeon | WASD moves, Space attacks, R restarts. Esc quits |
| DungeonCrawler3D | 3D dungeon | WASD/arrows move, Space attacks, R restarts. Esc quits |
| ArenaShooter | twin-stick shooter, **gamepad**, **sound** | WASD + mouse aim + left click, or the sticks + A/right trigger. Esc quits |
| EchoVault | 3D platformer, **gamepad**, **3D sound** | WASD/stick moves, Space/A jumps. Esc quits |
| Candlewick | 3D stealth with lights, **gamepad**, **3D sound** | WASD/stick moves, Q/X lantern, hold E/A to light a brazier, Esc/Start pauses |
| Marionette | 3D melee duel, **gamepad**, **sound** | WASD/stick moves, J/left click light attack, K/right click heavy, hold Shift/RB to block, Space/A dodges, Esc back to the title |
| TestFramework | the engine's test app | Pick a page in the "Tests" panel. Cursor Test: keys 1/2/3 switch the cursor between normal, hidden and locked |

In every program, **F3 to F7** open the engine's debug window (Engine, Renderer, Input, Assets,
Animation). F5 (Input) shows live keyboard, mouse and gamepad state.

### Worth trying

Anything that goes wrong is useful, but these are the things a headless CI can't check:

- [ ] **Graphics**: no flicker, black frames, garbled colours or missing text. Do red and blue look right (not swapped)?
- [ ] **Window**: resize it, maximize it, minimize it and restore it. Breakout3D: F11 to fullscreen and back.
- [ ] **Focus**: alt-tab away. The games pause while unfocused and should carry on cleanly when you come back.
- [ ] **Mouse**: Cursor Test in the test app: does mode 3 (locked) give smooth mouse-look, and does Esc give the cursor back?
- [ ] **Keyboard**: on a non-US layout (AZERTY, QWERTZ), do WASD games move the way you'd expect?
- [ ] **Gamepad** (if you have one): ArenaShooter, EchoVault, Candlewick, Marionette. Unplug it and plug it back in while a game runs. F5 shows what the engine sees. (Rumble only works on Windows so far.)
- [ ] **Sound**: ArenaShooter, EchoVault, Candlewick, Marionette. In EchoVault and Candlewick, sounds should pan left and right as you move.
- [ ] **Scaling and monitors**: with display scaling above 100%, is the debug window's text sized right? Can you drag a window to a second monitor? In the test app, can you drag an ImGui panel out of the main window?
- [ ] **Session**: if your desktop offers both X11 and Wayland at login, try `./run-checks.sh` in the other one too. On Wayland the engine runs through XWayland for now.
- [ ] **Speed**: does anything stutter? The F3 tab shows the frame time.

## 4. Send back

- `dingo-linux-report-<date>.tar.gz` from step 2
- the `logs/` folder, if you played
- a few lines on what you saw, with a screenshot or video for anything visual:

```
Distro / desktop / X11 or Wayland:
GPU and driver:
What worked:
What broke (program, what you did, what happened):
```

The reports hold no personal data beyond what you see in them: distro, hardware and the
programs' own logs, which include this folder's path (and so your user name).
