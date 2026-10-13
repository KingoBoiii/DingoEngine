# Shipping Checklist

This page is the overview for the day you give your game to players: what to build, what goes in
the folder, what the game writes on their machine, and what you get back when it crashes. It
links to the detailed pages instead of repeating them.

- [Build the Distribution configuration](#build-the-distribution-configuration)
- [Put the folder together](#put-the-folder-together)
- [Name, icon and version](#name-icon-and-version)
- [Licences](#licences)
- [Where the game writes](#where-the-game-writes)
- [When it crashes](#when-it-crashes)
- [Settings players expect](#settings-players-expect)
- [Before you release](#before-you-release)
- [Limits](#limits)

## Build the Distribution configuration

Ship a **Distribution** build. Your game's project needs the same block the examples have:

```lua
filter "configurations:Distribution"
    kind "WindowedApp"                -- no console window
    optimize "On"
    symbols "Off"
    defines { "DE_DISTRIBUTION" }

distributionSymbols()                 -- Windows: a PDB, so a player's minidump can be read
```

`DE_DISTRIBUTION` is what the engine's headers switch on, so define it in the game's project, not
only in the engine's. Set it together with `WindowedApp`: `EntryPoint.h` defines `WinMain` for a
Windows Distribution build and `main` otherwise. [Getting started](getting-started.md) has the
whole `premake5.lua`.

```bash
./vendor/premake/bin/premake5.exe vs2026       # Windows, then MSBuild DingoEngine.slnx with
                                               #   /p:Configuration=Distribution /p:Platform=x64
./Generate-Linux.sh && make -j"$(nproc)" config=distribution MyGame     # Linux
```

What Distribution changes:

| | Debug and Release | Distribution |
|---|---|---|
| Console (Windows) | `main`, a console window | `WinMain`, no console: stderr goes nowhere, so the log is the only record |
| `DE_ASSERT`, `DE_CORE_ASSERT` | Debug only: log, then break into the debugger | compiled out, as in Release |
| `DE_VERIFY`, `DE_CORE_VERIFY` | log, then break into the debugger (`SIGTRAP` on Linux) | `FatalError("Verify Failed ...")`: log, message box on Windows, exit code 1 |
| Crash on Windows | dump and log line | the same, plus a message box naming the folder |
| Vulkan validation | on in Debug only; `--vulkan-validation` turns it on elsewhere | off, and the engine creates no validation-message callback |
| Symbols | Debug builds only | a PDB on Windows with `distributionSymbols()`, never shipped: see [Reading a minidump](#reading-a-minidump) |

Two things Distribution does **not** change:

- The built-in Debug window (F3 to F8: engine, renderer, input, assets, animation and profiler
  tabs) is on in every configuration. Set `EnableDebugOverlays = false` in `ApplicationParams`
  to strip it from the shipped game (see
  [Application & Layers](application-and-layers.md#applicationparams)).
- The command-line options (`--log-dir=`, `--log-here`, `--save-dir=`, `--vulkan-validation`)
  work in every build, so a player can use them if you tell them to.

## Put the folder together

A shipped game is a folder. The assets are not in it as loose files: they are one encrypted
`<Game>.dpak` that the executable mounts at start-up from its own directory, so the game starts
from any working directory. [Distribution](distribution.md) has the key, `embedPackKey()`, the
DingoPack tool, the `.dpakignore` file and what the encryption does not protect against. In short:

1. `DingoPack genkey dpak.key` once, and `embedPackKey()` in the game's `premake5.lua`
   ([Distribution](distribution.md#2-embed-it)). Keep the key out of a public repository
   ([Keeping the key](distribution.md#keeping-the-key)).
2. Pack the assets ([Pack and ship](distribution.md#4-pack-and-ship)). `scripts/pack-assets.sh` and
   `scripts/pack-assets.ps1` take the name of a project under `examples/`; for a game anywhere else
   call `DingoPack pack <assetsDir> <out.dpak> --key dpak.key`.
3. Put the package beside a **copy** of the executable, and name them alike: the engine looks for
   `<executable name>.dpak`. Never leave a `.dpak` in `build/bin/`, where every development run
   would mount it.

### Windows

```
MyGame\
  MyGame.exe
  MyGame.dpak
  assimp-vc145-mt.dll   kubazip.dll   minizip.dll   poly2tri.dll   pugixml.dll   zlib1.dll
  THIRD-PARTY-NOTICES.md
  LICENSE-*.txt         (the licence files from the root of your assets/)
```

The six DLLs are assimp's and its dependencies'. `copyAssimpRuntime()`, which every executable
that links the engine calls in its `premake5.lua`, copies them beside the `.exe` after each
Release or Distribution build; without them the game dies with `STATUS_DLL_NOT_FOUND` before
`main`. Ship all six, not only `assimp-vc145-mt.dll`.

The engine is built with the dynamic C runtime (`/MD`), so the player needs the Microsoft Visual
C++ Redistributable for x64. Nothing here installs it.

The release workflow (`.github/workflows/build-release.yml`, run by hand) does this for every
example: it packs `<Example>.dpak` into the Distribution build folder, copies the licence files
and `THIRD-PARTY-NOTICES.md` in, and zips the folder.

### Linux

`scripts/package-linux-programs.sh <Config> <output dir> [suffix]` writes one
`<Program>[-suffix]-linux-x86_64.tar.gz` per example, holding a `<Program>/` folder:

```
MyGame/
  MyGame
  MyGame.dpak
  THIRD-PARTY-NOTICES.md
  LICENSE-*.txt                (the licence files from the root of your assets/)
  MyGame.desktop               (if the project calls exeMetadata)
  MyGame.png                   (if the project has an icon.png)
  install-desktop-entry.sh
```

The script loops over `examples/*/`, so a game kept elsewhere repeats its steps: copy the
executable, pack the assets, copy the licence files, `THIRD-PARTY-NOTICES.md` and what
`exeMetadata()` generated under `build/generated/<Project>/`, and `tar` the folder. assimp is
linked statically, so the game needs only `libvulkan1` and a Vulkan driver on the player's machine.

The first run compiles shaders and font atlases into a `.cache/` folder beside the executable (see
[Where the game writes](#where-the-game-writes)). Don't put a `.cache/` in the folder you ship.

## Name, icon and version

`exeMetadata { ... }` in the game's `premake5.lua` gives the executable its name, version and
icon. [Distribution](distribution.md#5-name-icon-and-version) lists the fields; the parts that
matter when shipping:

- Only `Name` is required. Set `Version` (`"1.2"` or `"1.2.3"`) to your game's own, or the
  executable carries the engine's version and build number. See
  [Versioning](versioning.md#version-numbers-in-the-build).
- `icon.ico` and `icon.png` sit beside the game's `premake5.lua` (`Icon = "art/game"` names
  another pair). On Windows the version resource shows in Explorer's Details tab and the icon is
  the file's, the window's and the taskbar's.
- On Linux premake writes a `.desktop` template and copies the PNG, and the tarball ships them
  with `install-desktop-entry.sh`. The player runs it from the extracted folder: it writes
  `dingo-<Project>.desktop` into `~/.local/share/applications` (or under `$XDG_DATA_HOME`) with the
  folder's absolute path, because a desktop entry can't hold a relative one. `--remove` takes it
  out again, and it has to be run again if the folder moves.

Three names are easy to mix up, and all three are yours to set:

| | Set by | Shown as |
|---|---|---|
| Display name | `exeMetadata { Name = ... }` | Explorer's Details tab, the desktop entry's name |
| Window title | `ApplicationParams::Window.Title` | the window's title bar |
| User-data folder | `ApplicationParams::Name` | `<Name>` in [Where the game writes](#where-the-game-writes) |

## Licences

The engine is MIT (`LICENSE.txt`) and links third-party code (GLFW, NVRHI, Jolt, Box2D, miniaudio,
shaderc and more). Most of these licences ask that their notice travels with the binary. The
engine keeps them in one file, `THIRD-PARTY-NOTICES.md` at the repository root, and **a shipped
game carries it beside the executable**. It already includes the engine's own MIT text.

- `scripts/gather-notices.sh` writes the file from `scripts/notices/components.txt`: one line per
  component with its upstream, licence and licence text. Edit the manifest, never the file; run
  the script with `--check` to see whether the file is out of date.
- The manifest is the engine's. If your game links another library, or uses fonts, models, music
  or sounds under a licence that asks for a notice, ship those licences too.
- Licence files at the root of `assets/` (`LICENSE-Inter.txt`, `LICENSE-KayKit-*.txt`) are copied
  beside the package by the release workflow and by `package-linux-programs.sh`: any file there
  whose name contains `licence` or `license`. Name yours alike.

## Where the game writes

Only the shader and font caches are written next to the executable, and they fall back to the
per-user folder when it can't be written, as under Program Files. Every per-player file lives in
one folder named after the game:

| What | Where by default | How to move it |
|---|---|---|
| Logs: `Dingo.log`, then `Dingo.1.log` to `Dingo.4.log` for the four runs before | `<user data>/<Name>/logs` | `ApplicationParams::LogDirectory`; `--log-dir=<path>`; `--log-here` (the working directory) |
| Crash dumps (Windows): `Dingo-<date>-<time>-<pid>.dmp`, the newest five | the logs folder | with the logs |
| Saves: `<slot>.json` and its `.bak` | `<user data>/<Name>/saves` | `ApplicationParams::SaveDirectory`; `--save-dir=<path>` |
| Settings: the `settings` slot | the saves folder | `ApplicationParams::SettingsSlot` names the slot the window starts from |
| Shader and font caches | `.cache/` beside the executable | none; a folder that can't be written falls back to `<user data>/DingoEngine/cache/<executable name>` |
| `imgui.ini` (Debug window layout) | the working directory | none: the engine leaves Dear ImGui's default |

`<user data>` is `%LOCALAPPDATA%` on Windows and `$XDG_DATA_HOME`, else `~/.local/share`, on
Linux. So a game called `MyGame` logs to `%LOCALAPPDATA%\MyGame\logs` and
`~/.local/share/MyGame/logs`, and saves to the `saves` folder beside it.

**How `<Name>` is chosen.** It is `ApplicationParams::Name`. When that is empty, the engine uses
the executable's file name without its extension (`Application::GetName()` returns the result).
It is not the window title and not the `exeMetadata` name. Set it explicitly: renaming the
executable would otherwise move your players' saves, and it is used as the folder name as it is,
so it must be legal as one. Two games with the same `Name` share one folder.

```cpp
ApplicationParams params{
    .CommandLineArgs = args,
    .Name = "MyGame",
    .SettingsSlot = std::string(Settings::k_DefaultSlot),
    .Window = { .Title = "My Game" },
    .Assets = AssetManagerParams().SetPackageKey(Generated::GetPackageKey()),
    .EnableDebugOverlays = false,
};
```

Pass `--save-dir=<path>` when you test a shipped build, so it doesn't touch your own saves.
`Log::GetFilePath()` names the current log, for a "show the log" button in a menu. See
[Logs and crashes](application-and-layers.md#logs-and-crashes) for the rotation rules and
[Save slots](saves-and-settings.md#save-slots) for the save files.

## When it crashes

A crash report needs the `Application` to exist: the handlers are installed in its constructor.
After that, this is what happens.

| | The player sees | You get |
|---|---|---|
| **Windows**: unhandled exception, stack overflow, `abort()`, pure virtual call, invalid CRT parameter | In Distribution, a message box titled "Crash": "*Name* crashed", and the logs folder. In other configurations, nothing. | A minidump in the logs folder, and a line in `Dingo.log` that names it |
| **Linux**: SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT, SIGTRAP | Nothing: the window closes. The process ends as the signal would, core dump included if the system allows one. | A backtrace in `Dingo.log` (and stderr), frames as `executable(+offset)` |
| **Both**: an uncaught C++ exception | As above | Its `what()` in the log first |
| **Both**: `FatalError(message)`, which a failed `DE_VERIFY` calls in Distribution | In a Windows Distribution build, a message box with the message and the log's path; elsewhere nothing | The message, with file and line, in the log. No dump or backtrace; exit code 1 |

`FatalError` (`DingoEngine/Core/FatalError.h`) is the way out when the game can't go on: a missing
file it can't start without, say. The engine uses it too: when no graphics device can be created,
start-up ends in a `FatalError`, and the log says why.

Ask the player for the whole logs folder: `Dingo.log`, the runs before it and any `.dmp`.

### Reading a minidump

The dump holds every thread's stack, the global data and the memory the stacks point to, not all
of memory. Open the `.dmp` in Visual Studio (File, Open, then *Debug with Native Only*) or WinDbg,
with the **PDB of the exact build the player ran**: a PDB from another build of the same source
won't match. The dump names the exception and its address, and the call stack follows from there.

On Windows, `distributionSymbols()` (called after the Distribution block) gives the game's
Distribution build a PDB. The engine's Distribution library has no debug info, so engine frames
show only the names of the engine functions the game calls, with no file or line. Archive the PDB of every build you release, and don't ship
it: the engine's own release workflow moves its examples' PDBs out of their packages into a
separate `DingoEngine-<version>-Distribution-symbols-windows-x86_64.zip`.

Linux Distribution builds have no symbols. Add `symbols "On"` under the Distribution filter for
Linux too, and the build gives `addr2line -C -f -e <executable> <offset>` a function
and a line for each frame of the backtrace. Keep an unstripped copy of every release's executable.

## Settings players expect

A shipped game usually has an options screen with:

- **Window mode and resolution.** `Windowed`, `BorderlessFullscreen` and `ExclusiveFullscreen`;
  `Window::SetWindowMode`, or `ToggleFullscreen` for a hotkey. `Window::GetDisplayModes()` lists
  the monitor's modes, largest first, for a resolution list. See
  [Window modes](application-and-layers.md#window-modes) and
  [Display modes](saves-and-settings.md#display-modes).
- **VSync.** `Window::SetVSync(bool)`, safe from any layer or event handler. With it off, some
  Vulkan drivers still cap the frame rate. See [VSync](application-and-layers.md#vsync).
- **Volume.** A master volume and one slider per audio bus (music, effects, voice):
  `AudioEngine::SetMasterVolume`, `SetBusVolume`. See [Buses](audio.md#buses).
- **Gamepad dead zone**, and your own options in `Settings::Game`.

`Settings` stores all of it in the `settings` save slot, and `ApplicationParams::SettingsSlot`
opens the window in the saved mode instead of switching after it appears. Create the game's audio
buses first; `Apply` sets only the buses that exist:

```cpp
Settings settings = Settings::Load();              // the saved slot over the running state
settings.Audio.BusVolumes["Music"] = 0.6f;
settings.Display.Mode = WindowMode::BorderlessFullscreen;
settings.Apply();
settings.Save();
```

[Saves & Settings](saves-and-settings.md#settings) has the rest.

## Before you release

- [ ] The game is built in **Distribution**, with `kind "WindowedApp"` and `DE_DISTRIBUTION` in its
      project, and was rebuilt against the engine version you ship (the promise is source
      compatibility, not binary: see [Versioning](versioning.md)).
- [ ] `ApplicationParams::Name`, `Window.Title`, and `exeMetadata`'s `Name` and `Version` are set.
- [ ] `EnableDebugOverlays` is `false`, unless you want players to have the F3 to F8 window.
- [ ] The folder holds the executable, `<Game>.dpak` named like it, the DLLs (Windows),
      `THIRD-PARTY-NOTICES.md` and the licence files, and none of: `assets/`, `.cache/`,
      `imgui.ini`, logs, a stray `.dpak`.
- [ ] The key is not in a public repository, and the package opens with the key built into this
      executable (`DingoPack verify <package.dpak> --key dpak.key`, then start the game).
- [ ] You started it from another working directory and from a read-only folder.
- [ ] You ran it on a machine without the Vulkan SDK, Visual Studio or your repository: on Windows
      with the Visual C++ Redistributable installed, on Linux with `libvulkan1` and a driver.
- [ ] A first run (no logs, saves or `.cache/`) and a second run both start. `Dingo.log` of the
      second has no errors and no warnings about missing files.
- [ ] Settings survive a restart: window mode, size, VSync and volumes.
- [ ] You forced a crash once in the Distribution build (call `FatalError("test")`, or fault on
      purpose) and found the log, the message box and, on Windows, a dump that opens with your
      archived PDB.
- [ ] You archived the PDBs (Windows) or unstripped executables (Linux) of this exact build.
- [ ] Linux only: `install-desktop-entry.sh` adds a launcher with the icon, and `--remove` takes it
      out.

## Limits

- **No installer.** A release is a folder or a zip (Windows) or a `.tar.gz` (Linux). The engine
  makes no MSI, no Setup program, no AppImage, Flatpak or `.deb`, and bundles no runtime
  redistributable.
- **No code signing.** The executables are unsigned, so Windows may warn before running a
  downloaded one.
- **No auto-update.** A player replaces the folder. A patch or DLC can be a second `.dpak` mounted
  with `AssetManager::Unpackage` ([Distribution](distribution.md)).
- **No store integration.** The engine has no Steam or itch.io code (no Steamworks, no butler
  upload, no achievements or cloud saves). The release workflow uploads to GitHub releases only,
  and runs by hand.
- **No icon on a Linux window at run time.** The desktop entry's icon shows in menus and launchers,
  but the engine doesn't pass the window an icon. On Windows the icon is the executable's.
- **Crash reports stay on the player's machine.** Nothing is uploaded; the player sends the files.
  Linux has no minidump, only a backtrace and the system's core dump.
- **The release scripts cover `examples/*`.** A game elsewhere repeats their steps by hand.
- **An asset package is deterrence, not protection.** The key ships with the game. See the
  [threat model](distribution.md#threat-model).
- **Windows and Linux on x86-64 only.** DirectX 11 and 12 exist on Windows only; Vulkan needs a
  1.3 driver on both.
