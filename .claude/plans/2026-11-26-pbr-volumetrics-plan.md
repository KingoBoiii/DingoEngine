**Status**: open — Phase 1 built and GPU-verified 2026-11-26; next: Phase 2

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

### Phase 1 design (engine-designer, 2026-11-26)

**Gaps found:** `NvrhiTexture.cpp:86-88` hard-codes depth/mips/array to 1 (Texture3D unusable, no `Depth` param); uploads write only mip 0 layer 0 with no depth pitch (`NvrhiTexture.cpp:160`, `NvrhiCommandList.cpp:109`); compute UAV item is `AllSubresources` + `Unknown` dimension (`NvrhiComputePass.cpp:83`) so a cube gets an `eCube` storage view on Vulkan (`vulkan-texture.cpp:266`) and the tracker moves every mip to UAV; readback is 2D-only and restores all subresources from (0,0)'s state (`NvrhiTexture.cpp:170/:215-220/:261/:280-285`); `.hdr` loads as 8-bit (`FileSystem.cpp:75`). No new binding kinds or compiler changes: `NvrhiShader.cpp:588-609` maps by storage/sampled, and SPIRV-Cross + FXC (`ShaderCompiler.cpp:390-470`) handle samplerCube/sampler3D/image2DArray/image3D.

**Public API**
- `TextureDimension::TextureCube` appended; faces +X −X +Y −Y +Z −Z = layers 0..5.
- `TextureParams` (after `Height`, `Texture.h:35`): `Depth = 1` (3D only), `MipLevels = 1` (0 = full chain), `ArrayLayers = 1` (>1 only for Texture2D → 2D array; cube normalised to 6), plus setters. `constexpr GetMipLevelCount(w, h, d = 1)`.
- `Texture::UploadSubresource(data, mip, layer = 0)` (packed rows of the mip extent; 3D = all slices, layer 0); `Texture::ReadSubresource(mip, layer, done)` (layer = array layer/face, or z slice for 3D); `ReadPixels` = `ReadSubresource(0, 0)` for any dimension; `Upload`/`InitialData` = mip 0 of every layer in order.
- `Texture::CreateFromHdrFile(path, format = RGBA16F, debugName)`: RGBA16F default (Vulkan doesn't guarantee RGBA32F linear filtering; half the memory); CPU `FloatToHalf` (RNE, NaN/negative → 0, clamp 65504), alpha 1, rows flipped like `ReadImage`.
- `ComputePass::SetTexture(binding, tex, baseMip = 0, mipCount = 0)` (`ComputePass.h:74`), `SetStorageTexture(binding, tex, mip = 0)` (`:76`): cube/2D array bind as `image2DArray`, 3D as `image3D`.

**Internal**
- New `Graphics/TextureLayout.h`: `GetMipExtent`, `GetSubresourceBytes`, `NormalizeTextureParams(params, error)` (cube square + 6 layers; Depth>1 only 3D; layers>1 only 2D/cube; MipLevels ≤ chain; D32/RenderTarget only single-layer 2D, D32 1 mip). `Texture::Create` (`Texture.cpp:128`) and `NvrhiTexture::Reinitialize` (`NvrhiTexture.cpp:118`) validate first.
- New `Graphics/HalfFloat.h`: `HalfToFloat` moved from `Texture.cpp:19-32`, plus `FloatToHalf`.
- `NvrhiTexture.cpp`: dimension map `:42` (cube; 2D with layers>1 → `Texture2DArray`); desc `:80-92` from params; `Upload` `:134` loops layers in one list; shared `RecordSubresourceCopy` (2D/array/cube: `w×h` 2D staging + slice; 3D: `w×h×1` 3D staging + `setOrigin(0,0,z)`) restoring only that subresource to its prior state or `initialState`; used by `ReadPixels` `:167` and `NvrhiTextureReadback::Read` `:253`.
- `CommandList::UploadTexture(tex, data, mip, layer, rowPitch, depthPitch)` (`CommandList.h:52`, `NvrhiCommandList.h:32`, `.cpp:103`); restores just that subresource (#154 rule).
- `NvrhiComputePass.cpp`: SRV `:77` gets mip range subresources; UAV `:83` gets `(mip,1,0,All)` and `Texture2DArray` dimension for a cube; validate mip, re-check in `Prepare` `:126-134`; `:165-169` also collects `Texture_SRV` items. `NvrhiCommandList.cpp:353` `RequireStorageStates` gains `Texture_SRV` → `ShaderResource` (only compute lists carry SRV items; render passes unchanged).
- `FileReaders.h:12-16`, `FileSystem.cpp:62-92`: `IsHdrImage`, `ReadImageHdr[FromMemory]` (`stbi_loadf`, under `s_StbMutex`), `FreeImageHdr`. AssetManager stays RGBA8 until Phase 3.
- New engine shader include `<DingoEngine/CubeMap.glsl>` (`DE_CubeFaceDirection(face, uv)`, D3D/Vulkan convention) in `EngineShaders.cpp` `s_EmbeddedShaders`.

**Shader-author rules (docs/compute.md):** cube mip = `writeonly image2DArray`, z = face; 3D = `writeonly image3D`. No `imageLoad` on rgba16f/rgba32f/rgba8 (FXC X3676 on SM5.0/5.1); r32f is fine — Phase 5 integration must follow this. A pass may read and write one texture only in disjoint mips.

**Tests:** "Texture Subresource Test" (`--test=subresource`; `--test=texture` would hit "Texture Quad Test" first, `TestLayer.cpp:61`), `test/src/Tests/Renderer/TextureSubresourceTest.{h,cpp}`, registered at `TestLayer.cpp:82`, readback pattern `ComputeTest.cpp:265`: (1) cube round trip, 16² RGBA8 + RGBA16F, 5 mips, all 30 subresources exact; (2) cube sampled in a pixel shader, face order/orientation, mips 0 and 2; (3) compute writes each cube mip via `image2DArray`, read back, re-run (2) (UAV→SRV barrier); (4) mip chain 32×16 RGBA32F, pass reads N−1 writes N (exact 2×2 average); (5) 3D 16×16×8 RGBA16F `image3D` write, per-slice readback, `sampler3D` fetch + linear between slices; (6) draw → dispatch → draw with the same binding set sees new values; (7) HDR load from a 4×2 RGBE file written by the test (flip, alpha, half tolerance, clamp; RGBA32F exact). Doctests `test/UnitTests/src/TextureLayoutTests.cpp`: mip counts, extents, normalise accept/reject, half round trip.

**Backend risks:** D3D11 cube UAV as 2D array, SRV mip N−1 + UAV mip N in one dispatch (`d3d11-resource-bindings.cpp:460-493`, "still bound" warnings), 3D depth pitch, 8 compute UAV slots. D3D12 per-subresource states and end-of-list transitions (debug layer). Vulkan `CUBE_COMPATIBLE` (`vulkan-texture.cpp:208-210`) + our 2D-array override; run sync validation (llvmpipe serialises). Cube/3D textures handed to Renderer2D/ImGui/DingoUI fail view validation (document).

**Build order:** 1 TextureLayout/HalfFloat + doctests; 2 enum, params, desc, validation (all projects); 3 subresource upload; 4 ReadSubresource + copy helper; 5 compute views + SRV states; 6 CubeMap.glsl; 7 HDR loaders; 8 test case; 9 docs (`docs/textures.md`, `docs/compute.md`, `rendering.md`), As built.

**Owner decisions (approved 2026-11-26, all as recommended):** P1 add `TextureParams::Depth`; P2 explicit `CreateFromHdrFile` (not auto-detect in `CreateFromFile`); P3 `MipLevels = 0` = full chain; P4 allow Texture2D `ArrayLayers > 1` as a 2D array; P5 `ReadSubresource` public.

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

### Phase 1 (2026-11-26, uncommitted at time of writing)

Built as designed, plus:
- `NormalizeTextureParams` also rejects `Unknown`/`Count` dimension and zero width/height. No in-repo caller is affected (reviewed: AssetManager, Model, Font, Renderer3D placeholder, PostProcess, Framebuffer, tests).
- Copy-restore falls back to `initialState` when the list has no tracked state (old code skipped the restore). 1D and D32 textures still can't be read back.
- `CreateFromHdrFile` reads through the virtual file system (packs work), checks `IsHdrImage`, then `ReadImageHdrFromMemory`. There is no path-based `ReadImageHdr`.
- Compute bindings (review H1/M1/M2/M3):
  - Sampled mip ranges store an explicit count (NVRHI wraps `baseMip + AllMipLevels`, so the old code skipped barriers).
  - An invalid binding stays in its slot flagged as an error, and `Prepare` logs once and skips the dispatch until it is set again. No empty descriptors.
  - A texture `Reinitialize` re-derives the whole item, including the cube → `Texture2DArray` storage view.
  - Overlapping sampled and storage mips of one texture in one pass are rejected.
  - `SetStorageTexture(mip > 0)` on a Texture3D is rejected: NVRHI sizes a 3D UAV with mip 0's depth (`d3d12-texture.cpp:672`, `d3d11-texture.cpp:846`).
- `NvrhiFramebuffer.cpp:14-24` asserts that the attachment textures were created.
- Test shaders that use `texelFetch`/`textureSize` on a separate `texture2D`/`texture3D` need `GL_EXT_samplerless_texture_functions` (documented in `docs/compute.md`).
- Results on the owner's machine (RX 7900 XTX, private desktops):
  - "Texture Subresource Test" passes 17/17 on Vulkan (validation), DX11 and DX12 (debug layer clean).
  - Compute, Render Target and Particle pass on all three.
  - Unit tests: 212/212.
  - The error paths (M1/M2/M3) have no GPU coverage.
- Left out: L6, the six barrier commits per cube upload inside a frame (perf only). L5 (`UploadSubresource` outside a frame doesn't check `Renderer::IsRenderThreadParked()`, inherited from `Upload`) is #337.
- Doxygen isn't installed on the Windows machine, so the PR's `API reference` job is the check.
- Docs: new `docs/textures.md`; changes to `docs/compute.md` ("Mips, cubes and volumes"), `docs/README.md`, `docs/rendering-2d.md` (ReadPixels) and `.claude/reference/rendering.md`.
