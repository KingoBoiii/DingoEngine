# Dustlight

The v1.3 showcase of DingoEngine: sun shafts and volumetric fog in a dusty hall, with PBR props lit by
HDR environments. It uses every v1.3 feature: glTF PBR materials, image-based lighting and the skybox,
the froxel volumetric fog with sun shafts, local fog volumes (box and sphere, with noise), the raymarch
mode, and point and spot light scattering.

Run it with its own directory as the working directory, so `assets/` resolves:

```
cd examples/Dustlight
../../build/bin/Debug-windows-x86_64/Dustlight/Dustlight.exe
```

On Linux the executable is `build/bin/Debug-linux-x86_64/Dustlight/Dustlight`.

## What is in the scene

- A 12 x 5 x 16 m room with three windows in the south wall. The sun is shadowed and sweeps with the time of day.
- A hanging lantern whose spot and point lights scatter in the fog, with a froxel sphere volume around it.
- Dust motes drifting in the beams, and a raymarch fog volume in the west alcove.
- Props: a wooden table, a brass vase, crates, and a grid of 24 spheres (roughness left to
  right, metallic bottom to top) next to an emissive sphere that blooms.
- Three environments (sunny sky, Venice sunset, moonless night), baked over the first frames, one per frame, then switched instantly.

## Controls

| Action | Input |
|---|---|
| Look | Hold right mouse button and move |
| Move | WASD, with Q / E down and up and Shift to go faster (while looking) |
| Camera presets | 1 room, 2 sphere grid, 3 raymarch alcove |
| Control panel | F1 shows or hides it |

The panel has the time of day, the environment, volumetric fog on/off, the froxel grid size, temporal
blending, the debug view, the raymarch switch and resolution, the room and sphere volumes and their
density, the lantern, the dust motes, and the GPU timers `Volumetrics`, `VolumeRaymarch` and `Environment`.

## Flags

| Flag | Effect |
|---|---|
| `--check` | Steps through the feature switches, logs `[PASS]`/`[FAIL]` lines, `Dustlight cost: ...` and `DUSTLIGHT CHECK: N passed, M failed`, then closes |
| `--env=0\|1\|2` | Starts on that environment |
| `--time=<hours>` | Starts at that time of day (0 to 24; the sun is up from 7 to 18) |
| `--view=1\|2\|3` | Starts at that camera preset |
| `--no-raymarch` | Starts with the raymarch volume off |
| `--freeze` | Fixed step, holds the picture after a short settle and hides the panel, for captures |

The engine's own flags apply too (`--graphics=vulkan|dx11|dx12`, `--window=WxH`, ...).

## Assets

Poly Haven (CC0): the HDRIs in `assets/hdri/` and the glTF models in `assets/models/`. Credits and licences
are in `assets/LICENSES.md` and the repository's `THIRD-PARTY-NOTICES.md`.
