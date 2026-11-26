**Status**: open — scope and D1–D6 approved 2026-11-26; next: Phase 1

# v1.3: PBR materials, image-based lighting and volumetric fog

## Owner's decisions (2026-11-26)

- **O1. PBR is opt-in.** `Renderer3D::CreatePbrMaterial(PbrMaterialParams)` sits beside `CreateLitMaterial`; existing lit materials and every shipped game look the same as in v1.2. Both lit paths share lights, shadows, fog, AO and the object records.
- **O2. PBR scope:** Cook-Torrance for the existing directional/point/spot lights, texture maps (albedo, normal, metallic-roughness, AO, emissive) loaded from glTF, and IBL with a skybox.
- **O3. Volumetrics use a froxel grid:** compute fills a camera-aligned 3D grid, integrates it once, and every pixel (opaque, translucent, particles) samples it.
- **O4. Volume set:** global height fog, local fog volumes (box/sphere), sun shafts from the cascaded shadow map, point/spot scattering from the shadow atlas.

## Further decisions (approved by the owner 2026-11-26)

- **D1. Version v1.3.0**, branch `feat/pbr-volumetrics`, one PR with a phases checklist (like v0.9).
- **D2. No tangent attribute.** The PBR shader builds the normal-map frame from screen derivatives (cotangent frame). The arena `Vertex` (`Renderer3D.h:654`, 52 B, shared by the lit layout `Renderer3D.cpp:308-314`) stays as it is, so nothing in batching, skinning or static batches moves. *Alternative: a 16 B tangent per vertex from assimp's `aiProcess_CalcTangentSpace` (already requested, `Model.cpp:37`). Rejected for now: +30 % vertex size for every mesh, lit or not; revisit if derivative seams show on real assets.*
- **D3. Cube maps join `TextureDimension`** (`Enums/TextureDimension.h`) as `TextureCube`, with `MipLevels` and `ArrayLayers` on `TextureParams` (`Texture.h:31`). Needed by IBL; also gives games a skybox.
- **D4. IBL bakes at load, on the GPU**: equirect `.hdr` → cube (compute), diffuse irradiance as 9 SH coefficients, prefiltered specular mips (GGX), and one BRDF LUT made at renderer start-up. No offline baking or new pack format in v1.3.
- **D5. Volumetric fog is off by default** (`Renderer3DCapabilities` flag, like `LightBudgetFade`). When on, the global volume replaces the distance `Fog` (`Light.h:93`) for both lit paths; when off, nothing changes.
- **D6. Showcase: upgrade Candlewick** (lantern stealth, v0.7) — fog volumes in the halls, lantern cones, moon shafts through windows, PBR props — with a `--check` mode. Plus Dingo-TestFramework cases: a PBR material sphere grid, an IBL sphere grid, a froxel fog scene.

---

## Phase 1: texture foundations

- Cube and mip support: `TextureDimension::TextureCube`, `MipLevels`, `ArrayLayers`, per-mip/per-face upload and views (Vulkan/D3D11/D3D12 through NVRHI), storage writes to a mip of a cube for compute.
- 3D storage textures for the froxel grid: `Texture3D` exists; confirm `IsStorage` + sampling on all three backends.
- HDR image load (`.hdr` through stb_image's float path) into `RGBA16F`/`RGBA32F`.
- Tests: cube upload round trip, mip chain readback, 3D storage write then sample.

## Phase 2: PBR material, analytic lights

- `Renderer3D_Pbr.glsl` (GGX distribution, Smith-GGX correlated visibility, Schlick Fresnel, energy-conserving diffuse) reusing `Shadows.glsl` and the `CameraData` UBO (`Renderer3D.h:694`, mirror `Renderer3D_Lit.glsl:126-138`).
- `PbrMaterialParams`: BaseColor, Metallic, Roughness, Emissive, Ao, plus the five texture slots; white/flat-normal defaults.
- Object records and surface overrides (`Graphics/SurfaceOverride.h`) honoured where they make sense; skinned PBR draws too.
- `Model` import: an opt-in load flag maps glTF metallic-roughness materials (base colour, normal, MR, occlusion, emissive textures) to PBR materials; today only the diffuse texture is read (`Model.cpp:175`, `:265-277`).
- Hot reload in Debug like the lit shader.

## Phase 3: environment and skybox

- `Environment` asset: `.hdr` → cube (compute), `Renderer3D::SetEnvironment`, `SkyboxComponent` (environment, intensity, rotation), drawn after opaque at the far plane.
- Asset manager and packing support for `.hdr`.

## Phase 4: image-based lighting

- SH9 irradiance, prefiltered specular cube mips, BRDF LUT; PBR ambient = IBL when an environment is set, else today's ambient colour.
- Exposure/intensity control; tests compare against reference values for a white furnace and a known HDR.

## Phase 5: froxel volumetrics core

- Grid (default 160×90×64, exponential depth slices, `RGBA16F`), compute passes: inject density + in-scatter, temporal reprojection with jittered slices, front-to-back integrate.
- Global height fog (density, falloff, albedo, anisotropy g), sun shafts from the cascaded directional shadow (`Shadows.glsl:97`).
- Apply in lit, PBR, translucent and particle shaders; `FogComponent` gains volumetric settings.
- Pass order goes between shadows and opaque in `Renderer3D::EndScene` (`Renderer3D.cpp:611-618`).

## Phase 6: local volumes and local-light scattering

- `FogVolumeComponent`: box/sphere, density, albedo, edge falloff, optional scrolling 3D noise; culled to the frustum, budget like local lights.
- Point/spot in-scatter using `LocalLightShadow` (`Shadows.glsl:128`) and the shadow atlas.
- ImGui debug panel for the grid (slice view, cost timer).

## Phase 7: showcase, docs, review

- Candlewick upgrade + `--check`, test cases (D6).
- Docs: new `docs/pbr.md` and `docs/volumetrics.md`, updates to `lighting.md`, `post-processing.md`, `asset-pipeline.md`; `.claude/reference/rendering.md` sections.
- Milestone review `.claude/reviews/YYYY-MM-DD-v1.3-review.md`; open findings become issues.

## Needs testing on a real GPU (every phase)

Vulkan, DX11 and DX12 on the owner's machine: visual comparison of the test cases, D3D12 debug layer clean, Vulkan validation clean, froxel pass timings.

## As built

(empty)
