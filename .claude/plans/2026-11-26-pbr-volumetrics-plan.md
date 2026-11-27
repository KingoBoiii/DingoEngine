**Status**: open — Phase 1 committed (`3d33d8fc`); Phase 2 built, reviewed and GPU-verified 2026-11-27 (uncommitted); next: Phase 3

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

### Phase 2 design (engine-designer, 2026-11-27)

**Gaps found**
- G1. "Lit" means the shader is in `s_LitShaders` (`Renderer3D.cpp:205-210`). The `IsLitShader` callers assume one slot plus the lit UBO: `BindsPastSlotZero` (`:249`), its warnings (`:941-955`, `:1630-1639`, `:2329`, `:3615`) and `PrepareLitMaterial` (`:2383`).
- G2. `GetTwin` copies only slot 0 and the lit params (`:2106-2127`). The variant shaders are lit-only (`:1580`, `:2021`).
- G3. Material slots bind at 2+2i (`Material.cpp:350-355`). Five slots take bindings 2..11, which collides with:
  - `ShadowData` 5, the atlas 6 and the shadow sampler 7 (`Shadows.glsl:13-20`);
  - `DE_ObjectData` 8, a fixed number (`Renderer3D_Lit.glsl:154`);
  - `DE_InstanceData` 9 (`Instancing.glsl:13`);
  - `DE_SkinPalette` 11 (`Skinning.glsl:17`).

  All of them except `DE_ObjectData` already have binding macros, and they are found by name (`Material.cpp:315-321`).
- G4. There is no sRGB format and no gamma code, so the lit path lights in display space.
- G5. Model import:
  - materials are created without a shader (`Model.cpp:259`) and only the diffuse texture is read (`:175`, `:274`);
  - reload re-points slot 0 only (`:1237`, `:1246`);
  - textures are single-mip RGBA8 (`:74-84`);
  - the skinned component gives one material to every submesh (`Scene.cpp:872-874`).
- G6. The renderer has only clamp and point samplers (`Renderer.cpp:131-134`).
- G7. `SurfaceOverride` has no metallic field. `ObjectRecord.Surface.zw` is unused (`Renderer3D.h:758-763`).
- The engine has no MSAA (`NvrhiFramebuffer.cpp:114`), so screen derivatives are safe.

**Public API**
- New `Graphics/PbrMaterial.h`:
  - `PbrTextureSlot` {Albedo, Normal, MetallicRoughness, Occlusion, Emissive}, which are slots 0..4;
  - `PbrMaterialParams`: `Material` (a `MaterialParams`), `BaseColor{1}`, `Metallic 0`, `Roughness 0.5`, `Emissive{0}`, `EmissiveStrength 1`, `Ao 1`, `NormalScale 1`, five `Texture*` maps and a `Sampler*`, all null by default, plus fluent setters.
- `Renderer3D::CreatePbrMaterial` (`Renderer3D.h:497`), `k_PbrTextureSlots = 5` and `Statistics::PbrDraws` (`:598`).
- `ModelLoadParams::PbrMaterials = false` (`Model.h:53`).
- Owner decisions (approved 2026-11-27, all as recommended):
  - P1: the engine owns the PBR shaders (`PbrShaders.cpp`; `Internal::GetPbrShader`/`CreatePbrMaterial`; destroyed at the top of `Renderer::Destroy`).
  - P2: `MaterialParams` gains BaseColor/Metallic/Occlusion/NormalScale and reuses Roughness/Emissive.
  - P3: sRGB decode, linear lighting, lights × π, encode before fog. In post scenes, bloom sees display-space values (document this).
  - P4: shared includes extracted from the lit shader word for word.
  - P5: `SurfaceOverride::Metallic`.
  - P6: a CPU mip chain plus a repeat, trilinear, 8× anisotropic sampler for PBR imports.
  - P7: a skinned component with no material uses the submesh PBR material.
  - P8: PBR honours `CullMode`; glTF `doubleSided` → None.
  - P9: glTF alpha MASK warns once and draws opaque.

**Internal**
- `IsLitShader` also returns true for PBR shaders. Layout, object records, instancing, translucency and static batches then work unchanged.
- `BindsPastSlot(mat, IsPbr ? 5 : 1)` replaces `BindsPastSlotZero`.
- `PreparePbrMaterial`:
  - fills empty slots with `GetWhiteTexture()` and empty samplers with the PBR sampler;
  - writes a 64 B `PbrMaterialData` UBO only when it changes. Its fields are BaseColor, Emissive (rgb + strength), Surface (metallic, roughness, occlusion, normal scale) and Flags (fog target, alpha mode, has-normal-map, one spare for IBL).
- `GetTwin` copies five slots and the PBR fields. The skinned and instanced resolvers pick the PBR variants.
- Shadow casters are unchanged (`Renderer3D_Shadow.glsl`). Translucency uses the existing sorted pass.
- PBR bindings:

  | Binding | Resource |
  |---|---|
  | 0 | Camera |
  | 1 | PbrMaterialData |
  | 2–11 | slots 0–4 (texture/sampler pairs) |
  | 12 | ShadowData |
  | 13 | shadow sampler |
  | 14 | reserved: linear-clamp sampler |
  | 16 | shadow atlas |
  | 17 | ObjectData (new `DE_OBJECT_DATA_BINDING`) |
  | 18 | InstanceData |
  | 19 | SkinPalette |
  | 20, 21 | reserved: prefiltered cube, BRDF LUT (Phase 4) |
  | 22 | reserved: froxels (Phase 5) |

  D3D11 limits hold: constant buffers b0, b1, b12 plus push constants on b2; samplers on registers ≤ 15; SRVs up to t22.

**Shader**
- `<DingoEngine/Brdf.glsl>` is a GLSL subset that also compiles as C++ (the `ToneMappingTests.cpp:25` pattern), so doctests can check it. It holds `D_GGX`, `V_SmithGGXCorrelated`, `F_Schlick`, the exact sRGB conversions and `EnvBRDFApprox`.
- Fragment shader order:
  1. Sample all five textures first, in uniform control flow (FXC X3570/X4014).
  2. MR: G = roughness, clamped to [0.045, 1]; B = metallic.
  3. Apply the object record overrides.
  4. Cotangent frame with the det sign kept, Gram-Schmidt against Ng, and a fallback to Ng when |det| < 1e-12.
  5. Light loops copied from lit `:214-245`. Shadow bias uses Ng; N·L uses the mapped normal.
  6. Hook points for Phase 4 IBL (`PbrAmbient`) and Phase 5 froxels (`ApplySceneFog`).
- Hot reload: `WatchUnmanagedShader` on each variant; add the new files to `s_EmbeddedShaders` (`EngineShaders.cpp:38`).

**Model import**
- `LoadDiffuseTexture` (`:175`) becomes `LoadMaterialTexture`, used for each slot:

  | Slot | Texture type | Factor / notes |
  |---|---|---|
  | Albedo | BASE_COLOR, else DIFFUSE | `AI_MATKEY_BASE_COLOR` |
  | Normal | NORMALS | `GLTF_TEXTURE_SCALE` |
  | Metallic-roughness | `GLTF_METALLIC_ROUGHNESS`, else METALNESS | metallic/roughness factors |
  | Occlusion | LIGHTMAP, else AMBIENT_OCCLUSION | red channel; `GLTF_TEXTURE_STRENGTH` |
  | Emissive | EMISSIVE | colour × intensity |
- Alpha mode BLEND → Translucent. TWOSIDED → CullMode.
- Every slot is RGBA8_UNORM; the shader decodes sRGB for albedo and emissive only.
- Reload (`:1236-1248`) copies all slots.

**Tests**
- "PBR Material Test" (`--test=pbr`), `test/src/Tests/Renderer/PbrMaterialTest.{h,cpp}`, registered after `TestLayer.cpp:82`. Checks:
  1. a 5×5 sphere grid against CPU values within 2/255;
  2. lit coexistence at 0 px;
  3. normal map, including mirrored UVs and no UVs;
  4. skinned Fox;
  5. instanced against forced-CPU;
  6. static batch at 0 px;
  7. surface overrides;
  8. translucent alpha;
  9. a texture in slot 5 warns;
  10. glTF import from data-URI PNGs.
- Doctests in `BrdfTests.cpp`: D normalisation, the white furnace ≤ 1, F(1) = F0, the sRGB round trip and EnvBRDFApprox in [0, 1].
- Proving O1: the Lighting, Shadow, Batch and Throughput tests pass unchanged.

**Backend risks**
- FXC: gradients in loops, register pressure in Debug.
- D3D12: root signature with 3 constant buffers plus push constants.
- Vulkan: sparse bindings up to 22.
- All: derivative seams far from the origin, the normal-map green convention, lit versus PBR brightness.

**Build order**
1. `Brdf.glsl` plus doctests.
2. Extract the includes, add `DE_OBJECT_DATA_BINDING`, lit tests at 0 px.
3. Public API, P2/P5 fields, stats, `docs/pbr.md` stub.
4. `PbrShaders`, the static shader without normal mapping, `CreatePbrMaterial`/`Prepare`, `IsLitShader` and `BindsPastSlot`.
5. Skinned and instanced twins; check static batches and translucency.
6. Test checks 1, 2 and 4–9.
7. Cotangent normal mapping plus check 3.
8. Model import, reload, `Scene.cpp`, P6, check 10.
9. Docs and As built.

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

### Headstone request (2026-11-27, Coffin Works hall haze)

Source: Headstone `.claude/notes/engine-volumetrics-hall-haze.md` (worktree `ea-launch-gate-status-4b4c67`). Today the haze is a per-pixel march in the world shader, which costs +1.3 ms of a 2.7 ms GPU frame. Headstone wants to delete it.

| Need | Covered by |
|---|---|
| 1. Box volume: min/max, density, scattering colour, HG g; no cost outside it | Phase 6 `FogVolumeComponent` box (culled to the frustum) |
| 2. A depth read so the march ends at the surface | Not needed with froxels (O3): every pixel samples the integrated grid at its own depth |
| 3. Half or quarter resolution with a depth-aware upsample | The froxel grid (160×90×64) is far coarser than quarter resolution and needs no upsample |
| 4. Shadowed directional in-scatter, cascade-aware, so truss shadows cut the beams | Phase 5 sun shafts from the cascaded shadow map |
| 5. Frame-stable jitter so the glass shows no dither | Phase 5 temporal reprojection with jittered slices |

Gaps for Phase 5/6:
- **G-H1. Custom shaders.** Headstone draws with its own shader (`graveyard_light.glsl`), not lit or PBR. Phase 5 needs a public include such as `<DingoEngine/Volumetrics.glsl>` and a binding contract, so a custom shader can sample the grid.
- **G-H2. Shadow-cut sharpness.** Thin truss shadows may blur at 160×90 froxels. Phase 5 should expose the grid size and test Headstone's hall (`--spawn=0,8,180,10`) against the approved capture.
- **G-H3. Shadow casters.** Headstone's `CullView` keeps casters within 22 m while the hall is lit. The volume reads the same shadow map, so that rule stays on Headstone's side.

**O5 (owner, 2026-11-27): add a raymarch pass.** Beside the froxel grid, the engine gets a local-volume raymarch pass that does what Headstone asked:
- box volumes;
- a scene depth read (`PostProcess::CopySceneDepth`, `PostProcess.h:154`, already exists);
- half or quarter resolution with a depth-aware upsample;
- one shadowed directional light, cascade-aware;
- blue-noise or temporal jitter.

Defaults until the Phase 6 design session (the owner can change them then):
- The pass is opt-in per volume, a mode on `FogVolumeComponent`.
- It is built in Phase 6, after the froxel core.
- It composites after opaque and before translucents.

Open for that design: how it combines with froxel fog in the same view, and whether translucents see it.

## Phase 6: local volumes and local-light scattering

- O5 raymarch mode for box volumes (see "Headstone request" above).
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

### Phase 2 (2026-11-27, uncommitted at time of writing)

Built as designed in 9 groups, plus:
- **Shader colour space (review M2, owner decision):** `AmbientColor` and every light colour are `SrgbToLinear`-decoded in `Renderer3D_Pbr.glsl`, and lights are still × π.
  - White ambient and full-strength white light match lit exactly.
  - Mid-tones are brighter (gamma-correct falloff), as documented in `docs/pbr.md`.
- **Lit shader split:** the lit vertex variants compile to byte-identical SPIR-V. The fragment SPIR-V has the same instructions in a different declaration order, so the shader cache recompiles once.
- **Twins:** `GetTwin` rebuilds a cached twin whose shader no longer matches the requested one.
- **Shader failures:** `GetPbrShader` returns nullptr for an invalid shader and remembers the failed variant (`s_Attempted`, reset in `DestroyPbrShaders`). The skinned and instanced resolvers then fall back to the lit twin.
- **Normal map:** relative degeneracy tests (review L3). The `sign(det)` form is proven by check 3: u and v tilts, mirrored u and v, and no UVs, identical on all three backends.
- **Import:**
  - assimp 6.0.4 stores the occlusion strength as `"$tex.file.strength"`.
  - Albedo and emissive mips are averaged in linear light (`MipFilter::Srgb`). Normals are renormalised.
  - The texture cache key carries the use (Plain, Color or Normal), so a shared ORM image loads once.
  - A reload that changes `alphaMode` or `doubleSided` warns and keeps the first values (review L2).
- **Mirrored meshes (review L1, owner decision):** documented, not changed. PBR culls back faces, so a mirrored mesh needs `CullMode::None`.
- **Tests:**
  - "PBR Material Test" (`--test=pbr`) runs checks 1–11: grid against the CPU BRDF (including a dim render for the roughness 0.1 column), lit coexistence 0 px, normal map, skinned Fox, instancing, static batch, surface overrides, translucency, slot 5, glTF import, and a Scene-driven skinned component.
  - The on-screen view shows the grid and a flat / normal map / scale 2 panel.
  - New doctests: `BrdfTests.cpp` (with white-furnace lower bounds) and `MipChainTests.cpp`. Unit tests: 225/225.
- **Results on the owner's machine** (RX 7900 XTX, private desktops):
  - `--test=pbr`: 61/61 on Vulkan (validation), DX11 and DX12 (debug layer clean).
  - Lighting 43, Batch 75, Animation 117, Model 3D 4, Shadow 32 and Throughput 42–43 (allocator address-reuse variance), unchanged on all three backends.
  - FXC compiles the static PBR fragment shader at -O3 and -Od.
- **Review** (engine-reviewer): 0 High, 2 Medium, 7 Low, all fixed or decided by the owner as above.
- **Needs testing on a real GPU** (the owner's eye):
  - lit versus PBR brightness side by side in a real level;
  - derivative-normal seams on real assets far from the origin;
  - 8× anisotropic filtering on all three backends;
  - D3D12 per-mip uploads during a PBR import (widens #337's exposure outside a frame).
- **Docs:** new `docs/pbr.md`; changes to `docs/README.md`, `lighting.md`, `performance.md`, `animation.md`, `scenes-and-ecs.md`, `asset-pipeline.md`, `Shadows.glsl` binding note, and `.claude/reference/rendering.md` ("PBR materials").
- Doxygen isn't installed here, so the PR's `API reference` job is the check.
