# Arena Shooter

The v0.6.0 showcase for the DingoEngine **AssetManager** (asset pipeline, async
loading, and live hot-reload). It is a wave-based top-down 2D arena shooter built
as a single thin `Layer` drawing with `Renderer2D` directly (no Scene/ECS), using
`FlappyBird` as the structural reference.

## What it demonstrates

- **Async loading + loading screen** — every asset is queued with
  `AssetManager::LoadAsync` from `GameLayer::OnAttach`; the loading screen draws a
  progress bar from `GetPendingCount()` and switches to the menu once nothing is
  pending.
- **Path-relative asset root** — configured through
  `ApplicationParams.Assets` (`RootDirectory = "assets"`,
  `EnableHotReload = true`).
- **Shader hot-reload** — the animated background is a fullscreen quad drawn with
  a raw `Pipeline` + `Renderer::DrawIndexed` (before the `Renderer2D` scene),
  fed by `assets/shaders/background.glsl` loaded through the manager.
- **Texture hot-reload** — the ship, enemy, and bullet sprites are PNGs loaded
  through the manager, so edits swap in live.
- **Audio** — shoot / enemy-death / player-hit / wave-start one-shots via
  `AudioEngine::PlayOneShot`.

## Controls

| Action | Keyboard / Mouse | Gamepad |
|---|---|---|
| Move | WASD | Left stick |
| Aim | Mouse cursor | Right stick |
| Fire | Left mouse / Space | (A) / Right trigger |
| Start / Retry | Enter / Space | (A) |
| Quit | Escape | - |

Survive escalating waves of enemies that spawn at the arena edges and chase you.
Bullets destroy enemies; contact costs one of your three lives (with a brief
invulnerability flicker). Score and wave climb as you clear each wave.

## Building & running

From the repository root:

```
./vendor/premake/bin/premake5.exe vs2026
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" examples\ArenaShooter\ArenaShooter.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

Run it with the example directory as the working directory so the relative
`assets/...` paths resolve:

```
cd examples\ArenaShooter
..\..\build\bin\Debug-windows-x86_64\ArenaShooter\ArenaShooter.exe
```

## Trying the hot-reload demos

Leave the game running, edit a file, and save — the change appears within the
poll interval (0.5s) with no restart.

**Shader hot-reload**

1. Open `assets/shaders/background.glsl`.
2. Change a color or animation constant in the fragment section (e.g. bump the
   grid color `vec3(0.15, 0.45, 0.6)` to something bright, or scale `uv * 24.0`).
3. Save. The background updates live. A compile error keeps the previous shader
   running and logs the error, so you can fix and re-save.

**Texture hot-reload**

1. Open any sprite in `assets/sprites/` (`player.png`, `enemy.png`, `bullet.png`)
   in an image editor.
2. Repaint it and save as the same file.
3. The new artwork swaps in immediately; existing `Texture*` pointers stay valid
   (the manager reinitializes the GPU texture in place).

> Offline edits (while the game is not running) are fine too: the shader bytecode
> cache is validated against a hash of the source at startup, so a changed
> `background.glsl` recompiles automatically on the next launch.
