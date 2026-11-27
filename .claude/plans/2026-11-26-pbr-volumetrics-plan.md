**Status**: open — Phases 1–4 committed (`3d33d8fc`, `97235b5e`, `13c54ffb`, `a94c3af8`, `04693ec3`, `e15b509f`); Phase 5 built and reviewed (uncommitted); left: L1 failed-dispatch case logs once but still samples a stale volume that scene (explain in PR); D3D12 #1008 shadow-atlas duplicate-barrier warning rises to ~2.5k lines in the froxel run (atlas used as compute SRV), same known warning

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
- **D6. Showcase** (*changed 2026-11-27: a fresh example project, see Phase 7*)**: upgrade Candlewick** (lantern stealth, v0.7) — fog volumes in the halls, lantern cones, moon shafts through windows, PBR props — with a `--check` mode. Plus Dingo-TestFramework cases: a PBR material sphere grid, an IBL sphere grid, a froxel fog scene.

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


### Phase 3 design (engine-designer, 2026-11-27)

**Gaps found**
- G1. `.hdr` isn't an asset type: `AssetTypeFromExtension` doesn't list it (`AssetTypes.cpp:44-55`), so `Unpackage` skips packed `.hdr` files (`AssetPackaging.cpp:305`). `Package` already packs every file (`AssetPackage.h:42-52`).
- G2. `Renderer::Dispatch` only records into the frame list (`Renderer.cpp:642-647`). A texture created with `InitialData` outside a frame submits its own list (`NvrhiTexture.cpp:231-233`, `:275-289`) = #337; `createTexture` alone submits nothing (`:200`). So the cube conversion runs inside a frame and the equirect pixels reach the GPU through the frame list.
- G3. No reverse-Z: depth clears to 1.0 (`NvrhiTexture.h:11`); D3D clamps depth to 1, so `Less` never passes at the far plane (`rendering.md:52`). The sky uses `LessEqual`, no depth write. No MSAA (`NvrhiFramebuffer.cpp:114`).
- G4. A material without a scene buffer binds its UBO at 0, slot 0's texture at 1, sampler at 2 (`Material.cpp:354-358`). Default sampler is linear-trilinear-clamp (`Renderer.cpp:132`, `Sampler.h:15-22`).
- G5. Scene-scoped renderer state is cleared in `ClearSceneLights` (`Renderer3D.cpp:615`), before the draws, so `EndScene` copies fog into `m_CameraData` first (`:590-607`). Scene fog is chosen by `SubmitFog` (`LightSystem.cpp:98-146`) from `SubmitLights` (`:149`), called by `SceneRenderer` (`SceneRenderer.cpp:76`).
- G6. Scenes are not serialised; only the debug inspector lists components (`SceneDebug.cpp:208`, `:695`).
- G7. AO at depth ≥ 1 is exactly 1 (`PostAmbientOcclusion.glsl:61-63`), so a sky writing no depth leaves AO, soft particles and the depth copy as with a clear background.

**Public API** (recommended options; see P1–P10)
- New `include/DingoEngine/Graphics/Environment.h`:
  - `EnvironmentParams { Path; FaceSize = 0 (auto); DebugName }` with setters.
  - `class Environment`: `static Environment* Create(const EnvironmentParams&)` (nullptr when no device, not `.hdr`, or bad size); `~Environment`, `bool Prepare()`, `bool IsReady() const`, `Texture* GetCubeMap() const`, `uint32_t GetFaceSize() const`, `uint32_t GetMipLevelCount() const`, `uint64_t GetId() const` (never reused).
  - `SceneEnvironment { Dingo::Environment* Environment = nullptr; float Intensity = 1; float Rotation = 0 /*deg about +Y*/; bool DrawSkybox = true; }`.
- `Renderer3D` (next to `SetFog`, `Renderer3D.h:325-333`): `bool SetEnvironment(const SceneEnvironment&)` (false for non-finite or negative intensity), `void ClearEnvironment()`. Scene-scoped like `SetFog`: last call before `EndScene` wins; `EndScene` clears it. `Statistics` (`:539-629`) gains `bool Skybox`, `uint32_t EnvironmentBakes`.
- `SkyboxComponent { Dingo::Environment* Environment = nullptr; float Intensity = 1; float Rotation = 0; bool Visible = true; bool Enabled = true; }` in `Components.h` after `FogComponent` (`:335`); spell the member `Dingo::Environment*` (GCC). Phase 4 IBL reads the same component; `Visible = false` = IBL with no sky.
- `AssetType::Environment` appended before `Count` (`AssetTypes.h:22-34`); `GetEnvironment(handle)` and `Get<Environment>` (update the static_assert text, `AssetManager.h:193-217`).

**Environment (internal)**
- Owns two `Texture*`, deleted in the destructor: the cube (`TextureCube`, RGBA16F, full mip chain `MipLevels = 0`, `IsStorage`, sampled) and the equirect (RGBA16F 2D, one mip, no `InitialData`, so it submits nothing).
- `Create` reads the file through the VFS (the `Texture.cpp:117-145` path, `FloatToHalf`) and keeps the halves on the CPU until the bake.
- Format and full chain are final for Phase 4: GGX prefilter importance sampling needs the source mips, SH9 projects from a small mip. Phase 4 adds its prefiltered cube, SH buffer and LUT beside them.
- Face size: auto `floor_pow2(W/4)` clamped to [32, 1024] (4k equirect → 1024², ~64 MB with mips). Explicit size must be a power of two in [16, 2048], else nullptr. Pure function `Internal::ChooseEnvironmentFaceSize` in `Graphics/EnvironmentLayout.h` (doctested).
- Bake (`Prepare`), inside a frame only (false outside one, in a skipped frame, or with an invalid bake shader, logged once):
  1. `UploadSubresource` the halves into the equirect (joins the frame list, `NvrhiTexture.cpp:268-272`; #337 doesn't widen).
  2. Dispatch `EnvironmentBake.glsl` `DE_ENV_EQUIRECT`: 8×8 groups, z = face; equirect `texture2D` at 1, sampler at 2 (linear, U repeat, V clamp), `writeonly image2DArray rgba16f` at 3 (`SetStorageTexture(3, cube, 0)`); face size in push constants; 2×2 stratified taps per texel.
  3. Each mip N ≥ 1: `DE_ENV_DOWNSAMPLE` reads mip N−1 via `SetTexture(1, cube, N-1, 1)` as `textureCube`, `textureLod(dir, 0)` at the destination texel centre (the shared corner of four source texels, so bilinear = exact average, no `imageLoad`), writes mip N.
  4. Free the CPU halves; move the equirect and per-mip ComputePasses into a `Renderer::RunAfterFrame` lambda that deletes them (never refers back to the Environment). NVRHI keeps GPU handles alive until the list retires (`NvrhiTexture.cpp:225`).
- Shaders in `Graphics/EnvironmentShaders.{h,cpp}`: `Internal::GetEnvironmentShader(EquirectBake|Downsample|Skybox)`, lazy via `CreateEngineShader`, watched like PBR (`PbrShaders.cpp:35-54`); `DestroyEnvironmentShaders` beside `DestroyPbrShaders` (`Renderer.cpp:178`).
- `Internal::EnvironmentAccess` (friend, like `MaterialAccess`) gives the AssetManager create-from-halves and in-place reload (`Texture::Reinitialize` on the cube: same id, new generation, cached sets rebind, needs a re-bake).
- Lifetime: owned by the caller or the AssetManager, deleted before `Renderer::Destroy`; alive from `SetEnvironment` to `EndScene` (like a mesh for `SubmitMesh`); deleting after the scene that drew it is safe (NVRHI refs). Phase 4 adds its IBL stages to `Prepare`.

**Render pass order and pipeline**
- `EndScene`: right after `ResolveSceneLights` (`Renderer3D.cpp:588`) copy the pending `SceneEnvironment` to a local (`ClearSceneLights` at `:615` drops it); call `Prepare()` (counts `EnvironmentBakes`) before the shadow pass; `DrawSkybox()` between `DrawSkinnedSubmissions()` (`:686`) and `DrawTranslucentPass` (`:687`). Translucents, particles, then post `End` (tone map, bloom) draw over it.
- Target with no depth attachment: draw the sky before the opaque loop (`:641`).
- Skip for orthographic (`CameraPosition.w < 0.5`), `DrawSkybox == false`, or not ready.
- `Skybox.glsl`: vertex = fullscreen triangle (`Fullscreen.glsl:9-11`), `gl_Position = vec4(ndc, 1, 1)`, passes `v_Ndc`. Fragment: `dir = unproject(ndc,1) − unproject(ndc,0)` via `InverseViewProjection`, rotate about Y by −Rotation, `texture(samplerCube(u_Cube, u_Sampler), dir)` implicit LOD (no branches), `o_Color = vec4(LinearToSrgb(rgb * Intensity), 1)`. Hook `ApplySkyFog(color, dir)` = identity for Phase 5.
- `SkyboxData` UBO at 0, std140, append-only, 96 B: `mat4 InverseViewProjection` (CPU `glm::inverse(dmat4(VP))`), `vec4 Params` (cos θ, sin θ, intensity, spare), `vec4 Reserved`. Cube at 1, `Renderer::GetClampSampler()` at 2 (D3D11 b0/t1/s2).
- Material: `CullMode::None`, `Opaque`, depth test on, write off, `LessEqual`, `UniformWritesPerFrame = k_MaxScenesPerFrame` (UBO written in every scene). One per cube, `m_SkyboxMaterials` keyed by `Texture::GetId()` (swapping a texture rebuilds pipelines, `FullscreenPass.h:10-14`); idle for `k_MaxIdleBatchScenes` → released like `ReleaseIdleTwins` (`:555`); all freed in `Shutdown`. A released entry never touches its possibly freed texture (implementer confirms `Material`'s destroy path). `Renderer::Draw(material, 3)` in the current target/viewport.

**Colour space**
- `.hdr` is linear radiance; the sky outputs `LinearToSrgb(radiance × Intensity)` (`Brdf.glsl:41`, extends past 1) — the same display-space encoding PBR applies before fog and lit outputs natively. Radiance 1 = white 255, like a fully lit white surface; independent of PBR.
- Into the RGBA16F post scene target values > 1 survive to tone map/bloom; into the RGBA8/BGRA8 swap chain they clip, as lights do.
- `Intensity` is linear. Phase 4: uniform sky radiance 1 → irradiance π → PBR diffuse albedo/π × E = albedo, matching `AmbientColor` white 1.

**Fog**
- Distance `Fog` doesn't apply to the sky (joins the "not fogged" list, `Renderer3D.h:325-331`, `docs/lighting.md`). Document: with a skybox set `FogComponent::UseClearColor = false` and `Fog.Color` = horizon colour. Phase 5 fills `ApplySkyFog` from the froxel grid's last slice.

**Scene/ECS**
- `SubmitSkybox` beside `SubmitFog` (`LightSystem.cpp:98-146`), same rules: the camera entity's own enabled component wins, else the first enabled one on a non-camera entity in entity order; inactive entities skipped; a second candidate warns once. Called from `SubmitLights` (`:149`); `SetEnvironment` after `Scene::SubmitLights` overrides it.
- Register with `DE_INSTANTIATE_COMPONENT` (`Entity.cpp:521`), `BuiltinComponents` (`ComponentStorage.h:49-55`), `CopyComponentIfExists` (`Scene.cpp:192`), `SceneDebug.cpp:208`/`:695`. No serialisation (G6).

**AssetManager and packing**
- `.hdr` → `Environment` (`AssetTypes.cpp:44-55`), `ToString` (`:11-23`), doc list (`AssetTypes.h:68-72`).
- Policy row after AudioClip (`AssetManager.cpp:409`, static_assert `:436`): `Load` = `Environment::Create`; worker `Decode` = `VirtualFileSystem::ReadBytes` + `ReadImageHdrFromMemory` + halves into new `AsyncResult::HalfPixels` (`AssetManagerData.h:65-77`); `Publish` creates or reloads in place; `ReloadInPlace` keeps the previous on failure; `HotReloadWatched` and `NeedsGraphicsDevice` true; `Environments` map beside `Textures` (`AssetManagerData.h:114`); update the `SupportsInPlaceReload` doc (`AssetManager.h:236-240`).
- ImGui per-type counts follow `Count` (`ImGuiDebugPanels.cpp:751`). `.hdr` stays deflated in packs (`PackWriter.cpp:19` unchanged).

**Backends**
- Cube `image2DArray` storage views are Phase 1's (`NvrhiComputePass.cpp:83`).
- New: cube SRV of mip N−1 plus 2D-array UAV of mip N on one texture in one dispatch. D3D11: overlap tracking and "still bound" warnings (`d3d11-resource-bindings.cpp:460-493`); D3D12: 6×M per-subresource transitions; Vulkan: `CUBE_COMPATIBLE` image with cube SRV subrange plus 2D-array storage view.
- Depth exactly 1.0: D3D depth clip is off; Vulkan clips but z = w is inside the clip volume.

**Tests**
- "Skybox Test" (`--test=skybox`) in `test/src/Tests/Renderer/SkyboxTest.{h,cpp}`, registered after `TestLayer.cpp:86`. Synthetic RGBE equirect written by the test (Phase 1 check 7 pattern): smooth analytic f(dir) plus a sun spot at 20.0. CPU references include `Environment.glsl` and `CubeMap.glsl` as C++ (the `Brdf.glsl` pattern).
- Checks: 1 bake matches f(`DE_CubeFaceDirection`) on 6 faces of mip 0; 2 each mip = 2×2 average down to 1×1; 3 orientation −Z/+X/−X/+Y/−Y centre pixels; 4 rotation 90° direction; 5 intensity 2 doubles linear; 6 opaque box covers sky 0 px vs no sky, near-far quad after the sky still passes depth; 7 lit and PBR meshes 0 px identical with/without sky; 8 post, tone map None, −2 EV: sun 20 reads 5 before the swap chain; 9 scene precedence, `Enabled`/`Visible = false`, `InactiveComponent`, duplicate; 10 two scenes in one frame with two environments; 11 fogged scene leaves sky pixels unchanged; 12 orthographic → `Statistics::Skybox` false; 13 AssetManager `.hdr` load, hot reload keeps pointer, pack round trip in a temp dir (never `build/bin`); 14 delete unbaked / right after drawing, no validation errors; 15 non-`.hdr` or non-power-of-two size → nullptr + error.
- On-screen view 2×2: forward sky with lit and PBR spheres; six axis viewports; rotation 90°; post HDR with exposure.
- Doctests `test/UnitTests/src/EnvironmentTests.cpp`: equirect uv↔dir round trip, −Z = u 0.5 / +X = u 0.75 / +Y = top, face-size rule, rotation sign, `.hdr` → Environment and `ToString`.
- Unchanged: Lighting, PBR, Batch, Throughput, Post. GPU runs via `.claude/skills/pr-gpu-test/gpu-test.ps1`.

**Build groups** (each builds; each runs its checks)
1. Pure maths: `Shaders/Environment.glsl` (`DE_EquirectUv`/`DE_EquirectDirection`, C++-compilable subset), `Graphics/EnvironmentLayout.h`, `.inl` + `s_EmbeddedShaders` rows (`EngineShaders.cpp:22-26`, `:59-63`), doctests.
2. Environment and bake: `Environment.h/.cpp`, `EnvironmentAccess.h`, `EnvironmentShaders.{h,cpp}`, `Shaders/EnvironmentBake.glsl`, `Renderer.cpp:178`. Test skeleton + checks 1, 2, 14, 15.
3. Renderer3D sky: `SceneEnvironment`, `Set`/`ClearEnvironment`, `Shaders/Skybox.glsl`, `m_SkyboxMaterials`, `DrawSkybox`, `Prepare` in `EndScene`, stats. Checks 3–8, 10–12.
4. Scene/ECS: `SkyboxComponent`, 4 registrations, `SubmitSkybox`, SceneDebug. Check 9.
5. Assets: `AssetType::Environment`, policy row, `HalfPixels`, getters. Check 13.
6. Docs: new `docs/environment.md` (orientation, units, fog note, cube limits for 2D, ImGui and DingoUI); `asset-pipeline.md`, `scenes-and-ecs.md`, `lighting.md`, `textures.md`, `README.md`; `rendering.md` "Environment and skybox".

**Needs testing on a real GPU**
- Sky at depth exactly 1.0 on all three backends; cube SRV mip N−1 + UAV mip N (D3D11 warnings, D3D12 debug layer, Vulkan sync validation); identical panorama orientation; HDR sun blooms, > 65504 clamped; first-use bake time for a 4k HDR (Tracy zone + GPU timer "Environment"); 64 MB at 1024 face; seamless cube filtering at face edges.

**Owner decisions** (2026-11-27: all A approved)
- P1 Environment shape: (A) concrete class, `Create`/`delete`, owned by caller or AssetManager; (B) abstract interface + Internal impl; (C) `std::shared_ptr` like `StaticBatch`.
- P2 Component: (A) `SkyboxComponent` with `Visible`, also read by Phase 4 IBL; (B) `EnvironmentComponent` with `DrawSkybox`; (C) separate Skybox and Environment components.
- P3 Precedence: (A) scene-scoped `SetEnvironment`, component submitted in `SubmitLights`, last call wins (like `SetFog`); (B) persistent renderer setting overriding the component; (C) component always wins.
- P4 Intensity: (A) linear multiplier; (B) EV stops; (C) nits.
- P5 Rotation: (A) degrees about +Y; (B) quaternion; (C) entity transform.
- P6 Face size: (A) auto `floor_pow2(W/4)` in [32, 1024], explicit powers of two up to 2048; (B) fixed 512; (C) always required.
- P7 Distance fog on the sky: (A) unfogged + documented horizon-colour recipe; (B) fogged at infinite distance; (C) per-component `FogAmount`.
- P8 `.hdr` in the AssetManager: (A) loads as `AssetType::Environment`; (B) RGBA16F `Texture2D`; (C) chosen by load params.
- P9 Bake timing: (A) lazily at first `EndScene` that uses it + public `Prepare()` for loading screens; (B) lazily only; (C) in `Create` (rejected: submits outside a frame, widens #337).
- P10 Test imagery: (A) synthetic RGBE written by the test only; (B) also a small CC0 1k HDRI in `test/assets` for the on-screen view.

## Phase 4: image-based lighting

- SH9 irradiance, prefiltered specular cube mips, BRDF LUT; PBR ambient = IBL when an environment is set, else today's ambient colour.
- Exposure/intensity control; tests compare against reference values for a white furnace and a known HDR.

### Design (engine-designer, 2026-11-27)

Gaps found:
- G1. Material binding sets are cached per key with no eviction (`Material.cpp:23-50`, `:315-326`). Binding each Environment's own IBL textures would leave one pass per (material, environment) holding deleted GPU memory. So **materials bind only renderer-owned IBL resources**, which never change.
- G2. `setComputeState` resets NVRHI graphics state (`vendor/nvrhi/src/vulkan/vulkan-compute.cpp:165`); a copy does not. The #154 restore rule (`NvrhiCommandList.cpp:123-131`) applies to every copy into a texture a stable binding set samples.
- G3. Render passes re-require only raw-buffer and UAV items (`NvrhiCommandList.cpp:358-362`, `:281`): a readonly storage buffer gets a `ShaderResource` barrier every draw.
- G4. No `CommandList` texture-to-texture copy exists (only staging copies, `NvrhiTexture.cpp:144`, `NvrhiGraphicsBuffer.cpp:229`).
- G5. `EnvironmentBake.glsl:12-14` declares its sampler and `image2DArray` for every variant; new variants need their own declarations (binding sets match reflection exactly).
- G6. Flat ambient is `SrgbToLinear(AmbientColor)·(diffuseColor + EnvBRDFApprox)·occlusion` (`Renderer3D_Pbr.glsl:95-98`), no (1−F) on diffuse. IBL keeps that structure so the furnace matches term by term.

**Bake pipeline** (`Environment::Prepare`, `Environment.cpp:121-193`): new stages after the downsample loop (`:157-168`), before `EndGpuTimer` (`:170`), same frame list and `passes` vector (`RunAfterFrame`, `:186-191`). Each stage runs only if the previous recorded; failure sets `m_BakeFailed` (`:180-184`); `m_Ready` and `m_BakeGeneration++` only when all recorded.
1. BRDF LUT once per process: `Internal::EnsureBrdfLutBaked()` (`EnvironmentShaders.cpp`), `DE_IBL_BRDF_LUT`, 16×16 groups of 8×8, binding 3 `writeonly image2D rgba16f`; `s_LutBaked` set only when recorded.
2. SH9 `DE_ENV_SH`: one group of 64. Source `SetTexture(1, cube, shMip, 1)`, `S = min(faceSize, 32)`, `shMip = log2(faceSize/S)`; `textureLod` at texel centres × exact texel solid angle (`DE_CubeTexelSolidAngle`); `shared vec4 s_Sum[9][64]` tree reduction; scale by `4π/Σdω` × cosine-lobe/π (1, 2/3, 1/4). Output binding 4 `writeonly buffer DE_EnvironmentShOut { vec4 C[9]; }` (RawBuffer_UAV u4), storing **irradiance/π**, so diffuse = `diffuseColor·Σ cᵢYᵢ(n)`.
3. Specular prefilter `DE_ENV_PREFILTER`: one dispatch per mip of `m_Specular` (`SetStorageTexture(3, m_Specular, m)`, 8×8, z = face), source `m_Cube` all mips + `Renderer::GetClampSampler()`. Mip 0 = one `textureLod` at `max(log2(faceSize/128), 0)`; mip m ≥ 1: roughness m/5, 64 Hammersley GGX samples, N = V = R, PDF-based source LOD `max(0.5·log2(Ωs/Ωp) + 1, 0)`, weight `max(NoL,0)`, no branch, `textureLod` only.

Hot reload: `EnvironmentAccess::Reload` (`:232-263`) already clears `m_Ready`; `m_Specular`/SH buffer are fixed-size, never reinitialised.

**Resources and lifetime**
- Environment (create in `Build` `:101-104`, delete in `~Environment` `:85-89`): `m_Specular` TextureCube RGBA16F face 128, `k_PrefilterMips = 6`, storage (~1.05 MB); `m_Irradiance` 144 B storage buffer, no initial data; `m_BakeGeneration`. `EnvironmentAccess` gains `GetSpecular`, `GetIrradiance`, `GetBakeGeneration`.
- Renderer3D (`Initialize` next to `:376-383`, `Shutdown` next to `:484-485`): `m_IblSpecular` (same format, no storage), `m_IblIrradiance`, `m_IblSourceId`, `m_IblSourceGeneration`. Never uploaded; undefined until first copy but never selected.
- LUT: global 128×128 RGBA16F storage texture, `Internal::GetBrdfLut()` created lazily (no submit) from `Renderer3D::Initialize`, deleted in `DestroyEnvironmentShaders` (`EnvironmentShaders.cpp:54-64`, `Renderer.cpp:178`), `s_LutBaked` reset there.
- Copy into the active slot in `EndScene` after `Prepare` (`Renderer3D.cpp:597-601`), before the scene UBO upload (`:620`), when `(GetId, GetBakeGeneration)` changed: new `CommandList::CopyTexture(dst, src)` (`CommandList.h` after `:52`; 6 faces × 6 mips, then `setTextureState(dst, All, initialState)` + `commitBarriers()`, `UploadTexture` pattern `:126-131`) and `CopyBuffer(dst, src, size)` with the same restore. `Statistics::EnvironmentLightingCopies` counts them. Ids never reused; L7 not widened.
- Threading: all main-thread recording into the frame list; #337 not widened.

**Bindings** (`Renderer3D_Pbr.glsl`; 22 stays reserved for Phase 5 froxels)

| Binding | Resource | Name | D3D |
|---|---|---|---|
| 14 | sampler, `Renderer::GetClampSampler()` | `u_EnvironmentSampler` | s14 |
| 15 | `readonly buffer DE_EnvironmentSh { vec4 Sh[9]; }` | `DE_EnvironmentSh` | t15 RawBuffer_SRV |
| 20 | `textureCube` | `u_EnvironmentSpecular` | t20 |
| 21 | `texture2D` | `u_EnvironmentBrdf` | t21 |

- Material: name constants next to `Material.h:229-238`; private `SetEnvironmentResources(GraphicsBuffer*, Texture* specular, Texture* lut, Sampler*)` next to `:315` + `MaterialAccess::SetEnvironmentResources` (`MaterialAccess.h:14`); bound by name in `GetOrCreateRenderPass` (`Material.cpp:315-380`), so only PBR binds them; ids join `MakeCacheKey` (one key per renderer).
- Call sites next to every `SetShadowResources`: `Renderer3D.cpp:672` (opaque/static/instanced), `:1891` (translucent), `:2418` (skinned). Shadow probe `:3935` is not PBR.
- Scene UBO: `CameraData` appends `glm::vec4 EnvironmentParams` after `FogParams` (`Renderer3D.h:738-750`): x = IBL on, y = `Intensity × LightingIntensity`, z/w = cos/sin(−Rotation). Update static_assert (`:753-758`) and mirror (`Renderer3DScene.glsl:29-41`); 96 B prefix frozen. Set before `:620`, zeroed in `ClearSceneLights` (`:2740-2753`).

**Shader math and colour space** (new `Shaders/Ibl.glsl`, C++-compilable like `Brdf.glsl`)
- `DE_ShBasis`, `DE_ShLambertOverPi`, `DE_CubeTexelSolidAngle`, `DE_Hammersley`, `DE_ImportanceSampleGgx`, `DE_PrefilterSourceLod`, `DE_PrefilterLod(r) = r·(k_PrefilterMips−1)` + inverse, `DE_IntegrateBrdf` (Filament DFG with `V_SmithGGXCorrelated` from `Brdf.glsl`, 256 samples), `DE_BrdfLutUv(NoV, r) = vec2(NoV, r)·(127/128) + 0.5/128`.
- `PbrAmbient` replaces `Renderer3D_Pbr.glsl:94-98`, gains `normal`, `geometricNormal`, `toCamera`; called at `:147` after all samples. Branch-free, selected at the end: n and R rotated by `DE_RotateY` (`Environment.glsl`); `irr = max(Σ Sh[i]·Yᵢ(n), 0)`; `pre = textureLod(cube, R, DE_PrefilterLod(r))`; `dfg = textureLod(lut, uv, 0).rg`; `so` = Lagarde specular occlusion; horizon `h = min(1 + dot(R, Ng), 1)²`; `ibl = EP.y·(diffuseColor·irr·occlusion + pre·(F0·dfg.x + dfg.y)·so·h)`; `return EP.x > 0.5 ? ibl : flat` (flat path byte for byte today's).
- Colour space: `.hdr` radiance is linear, enters undecoded; only `AmbientColor` is decoded. Uniform radiance 1 at Intensity 1 → irr = 1 → diffuse = diffuseColor, exactly white `AmbientColor` 1. Intensity is one linear multiplier on sky (`Skybox.glsl:51`) and IBL before the shared `LinearToSrgb` (`:178`). Post exposure still acts on encoded values.
- AO: material AO scales IBL diffuse and drives `so`; SSAO unchanged (post, #153). No multiscatter. Orthographic cameras get IBL (sky stays perspective-only).

**Public API**: `SceneEnvironment` (`Environment.h:109-119`) and `SkyboxComponent` (`Components.h:366-377`) gain `bool Lighting = true`, `float LightingIntensity = 1`; mapped in `SubmitSkybox` (`LightSystem.cpp:150`), validated in `SetEnvironment` (`Renderer3D.cpp:2655`), shown in SceneDebug (`SceneDebug.cpp:708`). `Renderer3D::Statistics` (`Renderer3D.h:640-641`) gains `EnvironmentLighting`, `EnvironmentLightingCopies`. `IsReady` doc (`Environment.h:74`) covers irradiance and specular.

**Tests**
- Doctests `test/UnitTests/src/IblTests.cpp` (`BrdfTests.cpp:9-26` pattern): SH orthonormal; texel solid angles sum to 4π; uniform cube → irr 1; analytic L = a + b·y + c(3y²−1) closed form; Hammersley + GGX pdf normalisation; LOD round trip; LUT bounds (A+B ≤ 1, ≥ 0.98 at NoV 1 r 0.045, monotone, |LUT − approx| ≤ 0.06); `so` = 1 at AO 1.
- "IBL Test" (`--test=ibl`, `test/src/Tests/Renderer/IblTest.{h,cpp}`, registered after `TestLayer.cpp:88`), synthetic RGBE: 1 LUT readback vs CPU; 2 SH uniform; 3 SH analytic within 1.5 %; 4 prefilter uniform/analytic/mip-5 MC; 5 white furnace grid; 6 analytic diffuse ±Y + chrome centre vs sky; 7 intensity / `LightingIntensity` 0 / `Lighting = false`; 8 rotation; 9 lit 0 px; 10 two environments per frame, copy counts; 11 hot reload; 12 delete then no environment; 13 occlusion 0.5; 14 skinned/instanced/translucent/static equal static; 15 orthographic.
- On-screen 2×2: IBL sphere grid before the sky; furnace grid; diffuse-only vs specular-only; normal-mapped + AO sphere IBL vs AmbientColor.
- Skybox Test check 7 becomes lit 0 px, PBR 0 px only with `Lighting = false`.

**Build groups**
1. `Ibl.glsl` + `.inl` + `s_EmbeddedShaders` row (`EngineShaders.cpp:22-29`, `:62-69`), `IblTests.cpp`. Verify: unit tests.
2. Bake: Environment members, 3 kernels (`k_VariantCount` 3→6, `EnvironmentShaders.cpp:11`, `:34-38`), LUT ownership, `Prepare` stages, access getters, IBL Test checks 1–4. Verify: `--test=ibl` 1–4, `--test=skybox`.
3. Public fields and plumbing, no rendering change. Verify: full build, Skybox check 9.
4. Renderer wiring + shader: copies with restore, active resources, `CameraData`, Material bindings, 3 call sites, `EndScene` copy, `PbrAmbient`. Verify: IBL 5–10, 12–15, Skybox 7, PBR/Lighting/Batch unchanged.
5. Hot-reload check 11, on-screen panel, cost logging.
6. Docs (`docs/environment.md`, `pbr.md`, `scenes-and-ecs.md`, `lighting.md`, `.claude/reference/rendering.md`) and As built.

**Risks (GPU)**: copy-into-sampled transitions on D3D12/Vulkan sync validation/D3D11 bound warnings; FXC `groupshared`+`barrier()` and 64/256-sample loop compile time at -Od; s14/t15/t20/t21 in the D3D12 root signature; LUT RGBA16F precision at grazing; PDF-LOD seams; fireflies at roughness 0.2–0.4; SH ringing under a strong sun. Cost: bake < 1 ms estimated on the RX 7900 XTX; +1.05 MB per Environment and per Renderer3D, 128 KB LUT; per pixel 2 fetches, 9 buffer loads, ~40 ALU.

**Owner choices — approved 2026-11-27: all A.** (recommended first): P1 controls (A `Lighting` + `LightingIntensity`); P2 IBL replaces `AmbientColor` (A); P3 flat path keeps `EnvBRDFApprox` (A); P4 no multiscatter (A); P5 fixed 128 prefiltered cube copied into a renderer slot (A); P6 LUT lazily at first `Prepare` (A); P7 no new exposure control (A); P8 IBL resources internal only (A); P9 SH on GPU (A); P10 Lagarde specular occlusion + horizon fade (A).

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

### Headstone follow-up (2026-11-27, note `engine-volumetrics-hall-haze.md` @ Headstone `4dbd48b`)

Use case: the Coffin Works hall, 24 m, six 55° east-window shafts cut by roof trusses. The haze is cool-blue dust visible only in the shaft light. Today it is a 12-step march in `graveyard_light.glsl`, with 1 atlas tap per step and HG phase, costing +1.3 ms of a 2.7 ms frame. Every gap below gets its answer in `docs/volumetrics.md`.

| # | Gap | Phase | First answer (to confirm in the phase design) |
|---|---|---|---|
| H1 | Per-volume in-scatter tint apart from light colour: haze (0.60, 0.74, 1.00) under (0.64, 0.58, 1.02) | 5 (global) + 6 | Needs a `ScatterTint` multiplier on in-scatter, ≥ 0, not clamped, separate from `Albedo`. The owner decides whether it joins `VolumetricFog` in Phase 5 |
| H2 | Vertical density profile inside a box: full below 11 m, 0 from 15 m, ×2.5 at 7–12 m | 6 | Height gradient or curve on `FogVolumeComponent` |
| H3 | Max in-scatter cap per pixel; per-volume near fade (0 within 1 m, full from 4 m); does the volume really dim the surface behind it? | 6 (cap, near fade) / 5 (dimming) | Dimming: yes, froxel apply is `color·T + L` (check 2). Cap and near fade go in the Phase 6 design |
| H4 | Smooth fade where cascades end; behaviour past the last cascade | 5 | Today `DE_DirectionalShadowVolume` (`Shadows.glsl:137`) returns 1, fully lit, past the last cascade: a hard edge. Add a fade to lit over the last cascade's tail. Internal only, no API |
| H5 | Scrolling noise as scale, drift velocity, min/max density | 6 | Already planned (noise); expose those four fields |
| H6 | HIGH/LOW/OFF knobs at runtime: raymarch resolution and step count, per volume or global | 5 (grid) + 6 | Froxel: `SetVolumetricsEnabled` + `SetVolumetricGrid` are already runtime. Raymarch: Phase 6 design |
| H7 | Box volume with shadows off | 5 + 6 | Yes: unshadowed returns 1 (`Shadows.glsl:134`) and ambient in-scatter is separate. Test both |
| H8 | Compute inject + raymarch on DX11 and Vulkan (Windows + Linux) | 5 + 6 | Froxel inject/integrate pass on Vulkan, DX11 and DX12 (group 3). Linux runs Vulkan |
| H9 | Measured cost on a mid-range GPU, froxel and raymarch | 7 | Only the owner's RX 7900 XTX is available. Open: which mid-range GPU (owner) |
| H10 | Raymarch + froxel in one view; translucents vs raymarch; raymarch vs a custom shader's own distance fog | 6 | Phase 6 design questions |
| H11 | Debug view of lit / shadowed / outside-cascade samples | 6 | Joins the planned grid debug panel |

**Owner (2026-11-27): H1 and H4 join Phase 5**, built with group 5.
- H1 adds `VolumetricFog::ScatterTint`, a `glm::vec3{1}` that must be ≥ 0, finite and unclamped. It multiplies in-scatter only, never extinction. `VolumetricData` gains a vec4 `ScatterTint` at 224, making it 240 B; the mirror sits in the inject kernel.
- H4 fades `DE_DirectionalShadowVolume` toward lit over the last cascade's tail, internally.
- Tests:
  - a tint check: in-scatter scales by the tint and transmittance is unchanged;
  - a fade check: no step at the cascade end.

**Owner approved P15 and P16 (2026-11-27)**: both are built with group 5, alongside H1 and H4.

**Headstone round 2 (2026-11-27, note @ `8840dda`).** The dust shows only where the shaft light provably reaches; unreached or unverifiable air is dark (fill 0). The map also has point and spot lamps.

| # | Ask | Phase | Proposed answer |
|---|---|---|---|
| H12 | Past the last cascade: lit or dark, with the fade kept | 5 | **P15**: `VolumetricFog::OutsideShadowLight` (0..1, default 1 = lit, today's behaviour; 0 = dark). Used for samples outside every cascade and outside the cascade map; a light with no shadow at all stays unshadowed (H7). Phase 6 volumes get the same field |
| H13 | Only the directional light scatters | 6 | Per-volume and global `LocalLightIntensity`, where 0 = directional only. A per-light flag only if it fits without changing the mirrored light structs in `CameraData` |
| H14 | Ray-length in-scatter thinning (full to 8 m, 25 % from 24 m); fade when looking along the beam | 6 | Lean: a far fade with a keep fraction beside the H3 near fade (same code). The view-along-beam fade is left to `Anisotropy` (lower g) and the cap. If the Phase 6 design rejects either, it's a look change for Headstone |
| H15 | Box volume in froxel mode as the only fog, no global fog | 6 | Yes: froxel is the default `FogVolumeComponent` mode. Activation becomes "global fog set OR any volume". Until then, a global fog with `Density` 0 works |
| H16 | Reset history on a camera cut; stability at a 4× timescale | 5 | **P16**: public `Renderer3D::ResetVolumetricHistory()` (`InvalidateVolumetricHistory` exists internally) and an ECS route on the camera. Timescale doesn't matter in Phase 5, because history is per frame and the medium is static; the Phase 6 noise drift must reproject, or be tested at 4× |

Constraints:
- Custom shaders mirror `CameraData` byte for byte. `Renderer3DCamera.glsl` only grows by appending, and every change is documented.
- `u_VolumetricFog` stays at 12/13 by default (Headstone uses 0–3 and 5–7).
- Off by default, with zero cost when off.
- `CreateLitMaterial` output is identical to v1.2.

Done when Phases 5 and 6 work in `examples/Dustlight` with the toggle panel and cost timer, `docs/volumetrics.md` answers H1–H11, and the v1.3 release notes list the final API names.

### Phase 5 design (engine-designer, 2026-11-27)

**Gaps found**
- G1. The froxel pass sits between `ClearSceneLights` and the opaque draws. `EndScene` uploads `CameraData` at `Renderer3D.cpp:644` and `ShadowData` at `:650`, then `ClearSceneLights` (`:652`) wipes fog, lights and environment; the shadow atlas is drawn only at `:671-675`. So the volumetric decision and its UBO upload happen before `:644`, and the dispatches after `:675` but before the sky-first draw at `:680`.
- G2. Custom shaders see only the 96 B `CameraData` prefix, and `Renderer3DScene.glsl` also carries varyings. Split the block into `Renderer3DCamera.glsl`.
- G3. Declaring the volume inside `Renderer3DScene.glsl` would break custom shaders that use bindings 12/13, so lit, PBR, particles and sky each include `Volumetrics.glsl` explicitly.
- G4. The new bindings go into cached material binding sets. Sampled textures are not in `m_RenderPassStorageItems` (`NvrhiCommandList.cpp:432-436`), but each scene's dispatch resets graphics state (`setComputeState`), so the first draw re-requires every set's states. No new barrier API.
- G5. Two compute files, not `#ifdef` variants (includes paste regardless of `#if`; binding sets must match reflection, Phase 4 G5).
- G6. Up to 32 scenes a frame but one history pair: only one scene per frame is temporal; others never write the history.
- G7. Nothing serializes `FogComponent` (only `Scene.cpp:192` duplicates it). No save-format change.

**Resources** (Renderer3D-owned, created in `Initialize` next to `:388-396`, deleted in `Shutdown` next to `:499`; code in new `src/DingoEngine/Graphics/Renderer3DVolumetrics.cpp`)
- `m_VolumetricIntegrated`: `Texture3D RGBA16F`, `IsStorage`. Created 1×1×1 with (0,0,0,1) as the dummy every material binds; on the first active scene `Reinitialize`d to the grid (same pointer and `GetId()`, new generation, so cached sets re-point; IBL G1 holds). rgb = in-scatter from the camera to slice k's far boundary, a = transmittance.
- `m_VolumetricScatter[2]` (history pair): `Texture3D RGBA16F`, storage, lazy at grid size. rgb = σs·L_in, a = σt.
- Memory at 160×90×64: 7.03 MiB per texture, 21.1 MiB total; 8 B when off.
- `m_VolumetricDataBuffer`: volatile UBO, `SetMaxWritesPerFrame(k_MaxScenesPerFrame)`, uploaded only by scenes that dispatch.
- `m_VolumetricInject[2]` (pass i writes `Scatter[i]`, samples `Scatter[1−i]`) and `m_VolumetricIntegrate[2]` (pass i reads `Scatter[i]`): fixed resources, so binding sets never rebuild on a swap; only the atlas is re-set with `SetTexture` when its `GetId()`/pointer changes.
- Shaders via `Internal::GetVolumetricShader(Inject|Integrate)` in new `Graphics/VolumetricShaders.{h,cpp}` (pattern `EnvironmentShaders.cpp:27-48`); `DestroyVolumetricShaders()` beside `Renderer.cpp:180`.
- `SetVolumetricGrid`: `Reinitialize` all three in place, invalidate history. Lazy-create failure: log once, scene treated as off.

**Slices and mapping** (`VolumetricMath.glsl`, C++-compilable like `Brdf.glsl`)
- Boundary b at view depth `z_b = Near·(Far/Near)^(b/D)`; slice 0 integrates from depth 0. View depth `d = clip.w` of `ViewProjection·(p,1)`.
- Froxel uv = `(ndc.x·0.5+0.5, 0.5−ndc.y·0.5)` on both inject and apply sides (same matrix, no backend y-flip).
- Inject world position = `CameraPosition + ray(uv)·d`; `ray(uv)` = bilinear blend of 4 CPU (double) corner rays scaled so `dot(row3(VP).xyz, ray) = 1` (exact with infinite far). Segment length `Δd·|ray(uv)|`, so transmittance is radial like `Renderer3DScene.glsl:60`.
- Apply: `s = D·log2(d/Near)/log2(Far/Near)`, `w = clamp((s−0.5)/D, 0.5/D, 1−0.5/D)`, `textureLod(...,0)`; for `d < z_1` `mix(vec4(0,0,0,1), sample, d/z_1)`; beyond Far, the last slice.

**`CameraData`** (append after `EnvironmentParams`, `Renderer3D.h:755`)
- `vec4 VolumeParams` @ **1856**: x = 1 when this scene samples the volume, y = Near, z = 1/log2(Far/Near), w = Far.
- `vec4 VolumeGrid` @ **1872**: xyz = grid size, w = 1/z_1. `sizeof` = **1888**; extend the static_assert `:763-765`.
- Mirrors: block moves from `Renderer3DScene.glsl:12-42` to new `Renderer3DCamera.glsl` (included at the top of `Renderer3DScene.glsl`); `docs/lighting.md` CameraData row; `.claude/reference/rendering.md`. `Renderer3D_MeshVertex.glsl:5-8` unchanged.
- Set every `EndScene` (0 when inactive), zeroed in `ClearSceneLights` (`:2792-2806`).

**`VolumetricData`** (std140, 224 B, private struct + static_assert in `Renderer3D.h`, mirrored in both kernels)

| Offset | Member | Contents |
|---|---|---|
| 0 | `CameraPosition` | camera position |
| 16 | `FrustumRays[4]` | corner rays, uv (0,0), (1,0), (0,1), (1,1) |
| 80 | `mat4 PreviousViewProjection` | last temporal scene's view-projection |
| 144 | `Grid` | xyz = size, w = 1 if history valid |
| 160 | `Slices` | x = Near, y = Far, z = log2(Far/Near), w = jitter |
| 176 | `Medium` | x = Density, y = BaseHeight, z = HeightFalloff, w = g |
| 192 | `Albedo` | rgb = albedo, a = current-frame weight (1 = no history) |
| 208 | `Intensities` | x = ambient, y = directional, zw reserved (Phase 6 volume/light counts) |

**Pass sequence in `EndScene`** (main thread, frame list; #337 not widened)
1. Before the fog block at `:627`, `PrepareVolumetrics(perspective)`:
   - Active = `Capabilities.Volumetrics && m_VolumetricFogSet && perspective && !IsFrameSkipped()`.
   - Lazily allocate; write `VolumeParams`/`VolumeGrid`; zero `FogColor`/`FogParams` when active (distance fog skipped, `Fogged` false, no AO fog).
   - Parity: temporal = `settings.Temporal` and no temporal scene yet this frame (`Renderer::GetFrameIndex()`): reads `Scatter[h]`, writes `Scatter[1−h]`, `h = 1−h`. Non-temporal writes `Scatter[1−h]` without touching history.
   - History valid when `m_VolumetricHistoryFrame == frame−1` and grid/Near/Far unchanged.
   - Jitter: temporal = Halton base 2 of `(frame % 8)+1`, the same for every froxel (frame-stable, no spatial dither, Headstone #5); non-temporal 0.5.
   - Upload `VolumetricData`.
2. Move `shadowAtlas` (`:677`) above the shadow block; after `:671-675`, `DispatchVolumetrics(shadowAtlas)` in GPU timer `"Volumetrics"`:
   - **Inject** (4×4×4 groups, 40×23×16): SRV atlas t3 (DepthWrite → ShaderResource via `RequireStorageStates`), history t5; UAV `Scatter[w]` u7; UBOs b0/b1/b2.
   - **Integrate** (8×8×1, 20×12×1, loop over D): `Scatter[w]` t5 (UAV → SRV); `m_VolumetricIntegrated` u7 (SRV → UAV); b1.
   - The first draw after re-requires ShaderResource through its set (G4). A later scene's inject puts the SRV → UAV barrier after the earlier scene's reads.
3. `Statistics`: `Volumetrics`, `VolumetricTemporal`, `VolumetricDispatches += 2`.

**Compute kernels**
- `VolumetricInject.glsl`: includes `Renderer3DCamera.glsl` (b0), `Shadows.glsl` with `DE_SHADOW_DATA_BINDING 2`, `ATLAS 3`, `SAMPLER 4`, and `VolumetricMath.glsl`; declares `VolumetricData` b1, `texture3D u_History` t5, `sampler u_HistorySampler` s6 (`Renderer::GetClampSampler()`), `writeonly image3D rgba16f` u7.
  1. Bounds-check and return.
  2. `d = z(k + jitter)`.
  3. `DE_FroxelMedium(world)`: σt = Density·exp(−HeightFalloff·max(y−BaseHeight, 0)), σs = Albedo·σt.
  4. `DE_FroxelInScatter(world, rayDir)` = `AmbientColor·Ia + Σ_{i<LightCounts.x} π·HG(dot(Dir_i, −rayDir), g)·Color_i·Id·(i == ShadowCounts.y ? DE_DirectionalShadowVolume(world) : 1)`.
  5. Current = (σs·L, σt).
  6. Reproject the froxel centre (k+0.5) with `PreviousViewProjection`; if valid and inside [0,1]³, `mix(textureLod(history, uvw, 0), current, Albedo.a)` with `k_VolumetricHistoryBlend = 0.1`, else current.
  7. `imageStore`.
  - Phase 6 adds volumes to `DE_FroxelMedium` and local lights to `DE_FroxelInScatter` (t8+, `Intensities.zw`).
- `VolumetricIntegrate.glsl`: `GL_EXT_samplerless_texture_functions`, `texelFetch` only (X3676). Per slice: `len = (z_{k+1} − z_k)·|ray|` (z_0 = 0); `Tr = exp(−σt·len)`; `S_int = σt > 1e-6 ? (S − S·Tr)/σt : S·len`; `scat += trans·S_int`; `trans *= Tr`; store.
- `Shadows.glsl` (after `:127`): `DE_DirectionalShadowVolume(worldPosition)` — cascade pick, one `textureLod` compare (hardware 2×2), no normal offset or cascade blend, `mix(1, s, ShadowParams.x)`, 1 when `ShadowCounts.x == 0`. Lit and PBR don't call it.
- D3D11 limits: CBs b0–b2, UAV u7 (< 8), samplers s4/s6; typed UAV store of R16G16B16A16_FLOAT is required at FL11 (Phase 1 proved 3D storage writes on all three backends).

**Applying the fog** (`Volumetrics.glsl`, public)
- `texture3D u_VolumetricFog` at `DE_VOLUMETRIC_FOG_BINDING` (default 12); sampler `u_VolumetricFogSampler` at `DE_VOLUMETRIC_FOG_SAMPLER_BINDING` (default 13) unless `DE_VOLUMETRIC_FOG_SAMPLER` names an existing sampler (`#ifndef`, honoured by the GLSL compiler).
- `vec4 DE_SampleVolumetricFog(mat4 viewProjection, vec4 volumeParams, vec4 volumeGrid, vec3 worldPosition)`; `vec3 DE_ApplyVolumetricFog(vec3 color, vec4 fog, int target)` returns `color·T + (target == 0 ? L : target == 1 ? 0 : 1−T)` (lit fog's targets).
- Callers branch on uniform `VolumeParams.x > 0.5` with `textureLod` inside (no implicit derivatives; X3570/X4014 don't apply). Off costs no fetch.
- Lit (`Renderer3D_Lit.glsl:113-119`): distance-fog block unchanged byte for byte; volumetric block after it. t12, s13 (`GetClampSampler`). Lit uses 0–3, 5–9, 11; 4 is `SkinData` (custom shaders only).
- PBR: `#define DE_VOLUMETRIC_FOG_BINDING 22`, `#define DE_VOLUMETRIC_FOG_SAMPLER u_EnvironmentSampler` next to `Renderer3D_Pbr.glsl:21-24`; `ApplySceneFog` (`:134-142`) branches the same way; call stays at `:214`.
- Translucent lit and PBR: free (same shaders, call site `:1917`).
- Particles (`ParticleDraw.glsl`): vertex adds `v_WorldPosition` (location 4); fragment `#define DE_VOLUMETRIC_FOG_BINDING 7`, sampler alias `u_SpriteSampler` (clamp, `ParticleRenderer.cpp:233`); applied before the premultiply at `:121`, target 1 when `Batch.w & 4` (additive). `ParticleDrawData` appends `VolumeParams`/`VolumeGrid` (192 → 224 B, assert `ParticleRenderer.cpp:65`); `ParticleRenderer::EndScene` (`:241`, called at `Renderer3D.cpp:745`) takes the texture and both vec4s; bound by name at `:232-235`.
- Sky (`Skybox.glsl`): `SkyboxData::Reserved` (`Renderer3D.h:675`, `Skybox.glsl:27`) becomes `Volume` (x = on, y = last slice's w); texture binding 3, sampler alias `u_Sampler`; `ApplySkyFog` (`:42-45`) samples (uv from `v_Ndc`, last slice), target 0 — the sky gets fog to `Far`; bound in `DrawSkybox` (`Renderer3D.cpp:2762`).
- Material: `k_VolumetricFogName = "u_VolumetricFog"` and `k_VolumetricFogSamplerName` next to `Material.h:236-248`; private `SetVolumetricResources(Texture*, Sampler*)` next to `:323` + `MaterialAccess::SetVolumetricResources` (`MaterialAccess.h:15`); bound by name in `GetOrCreateRenderPass` (`Material.cpp:331-348`, after `:399`; texture required, sampler only if reflected); both ids join `MakeCacheKey` (`:23-58`). Call sites next to every `SetEnvironmentResources`: `Renderer3D.cpp:697`, `:1917`, `:2445`.

**Public API**
- `struct VolumetricFog` in `Light.h` after `Fog` (`:101`): `Density` 0.02, `BaseHeight` 0, `HeightFalloff` 0, `Albedo{1}`, `Anisotropy` 0.2, `AmbientIntensity` 1, `LightIntensity` 1, `NearDistance` 0.5, `MaxDistance` 64, `Temporal` true. Display-space units like the lit shader: a white volume under ambient A scatters A.
- `Renderer3D::SetVolumetricFog(const VolumetricFog&)` / `ClearVolumetricFog()`, scene-scoped like `SetFog`, next to `Renderer3D.h:332-334`. Returns false (warn once) for non-finite values, negative `Density`/intensity, `NearDistance ≤ 0` or `MaxDistance ≤ NearDistance`; clamps Anisotropy to ±0.95 and Albedo to 0..1. Effect only with the capability on.
- `Renderer3DCapabilities` (`:63-130`): `bool Volumetrics = false`, `glm::uvec3 VolumetricGrid{160, 90, 64}` (x, y clamped 8–256, z 8–128). Runtime `SetVolumetricsEnabled`/`IsVolumetricsEnabled`/`SetVolumetricGrid`/`GetVolumetricGrid` beside `:537-540`; update the "only Instancing and LightBudgetFade can change" doc at `:60-62`.
- `Statistics` (after `:645`): `Volumetrics`, `VolumetricTemporal`, `VolumetricDispatches`.
- `FogComponent` (`Components.h:336-358`): `std::optional<VolumetricFog> Volumetric` (precedent `MeshRendererComponent::Surface`). `SubmitFog` (`LightSystem.cpp:139-142`) calls `SetVolumetricFog(*Volumetric)` when present; `UseClearColor` still affects only `Fog.Color`. SceneDebug shows the fields read-only (`SceneDebug.cpp:697-705`).
- `Internal::Renderer3DAccess` gains integrated/scatter texture getters for tests.

**Test: "Froxel Fog Test"** (`--test=froxel`, `test/src/Tests/Renderer/FroxelFogTest.{h,cpp}`, registered after `TestLayer.cpp:90`)
- Checks run on a second renderer, grid 32×18×16, Temporal off unless stated, RGBA16F target, starting after 10 frames. The on-screen renderer runs at 64×36×32 until checks finish, then `SetVolumetricGrid` to the default (also exercises check 14).
- Checks:
  1. Off is unchanged: capability off / on but unset / set then toggled off give 0 px vs each other with distance fog; 0 dispatches; integrated stays 1×1×1.
  2. Ambient-only uniform volume (g = 0, falloff 0): centre lit and PBR quad matches CPU `mix(c, A, 1−e^{−σr})` and `FogMode::Exponential` within 2/255.
  3. Readback of slices D/2 and D−1: α = e^{−σ·z_{k+1}·|ray|}, rgb = A(1−α), within 2e-3.
  4. Height falloff: closed-form column transmittance at top and bottom of the centre column.
  5. HG: toward-sun vs away ratio = HG ratio; g = 0 gives σs·C/4.
  6. Shafts: a texel in a caster's shadow has sun in-scatter ≈ 0; `ShadowStrength` 0.5 halves it.
  7. Temporal: after 30 still frames within 1/255 of Temporal off; consecutive frames differ ≤ 1/255 at 8 px; a teleport gives no history.
  8. Two scenes in one frame: only the first temporal; the second equals the reference.
  9. Fog targets: Additive takes only T, Multiply goes to white; lit and PBR translucent fogged.
  10. Particles: alpha and additive quads vs CPU.
  11. Sky pixel = `sky·T(Far) + L(Far)`.
  12. Test-only custom shader via `Volumetrics.glsl` + `Renderer3DCamera.glsl` equals CPU.
  13. Scene path vs direct API: 0 px at 1.5/255; a `FogComponent` without `Volumetric` keeps distance fog.
  14. Grid resize: same id, new generation, cached materials draw, no errors.
  15. Orthographic camera has no volume.
  16. Invalid settings rejected, warn once.
- On-screen 2×2: sun shafts through a slatted wall (temporal); height fog over boxes with lit and PBR spheres; g −0.5 / 0 / 0.8 toward the sun; translucent, particles and sky in fog.
- Doctests `test/UnitTests/src/VolumetricTests.cpp`: slice depth/coordinate round trip (z_0 = Near, z_D = Far, monotone); HG integrates to 1, HG(0) = 1/4π; integrate step vs closed form + σt → 0 limit; height density; Halton values and mean.

**Build groups**
1. `VolumetricMath.glsl` + `Renderer3DCamera.glsl` split (`.inl` + `s_EmbeddedShaders` rows, `EngineShaders.cpp:5-29`, `:45-70`), doctests. Verify: unit tests; Lighting 43, PBR 61 unchanged.
2. Public API + Scene plumbing, no rendering. Verify: full build.
3. Resources, `VolumetricShaders`, inject/integrate kernels, `Shadows.glsl` tap, `EndScene` hooks. Verify: checks 3–8, Shadow 32.
4. `CameraData` +32 B, `Volumetrics.glsl`, lit/PBR/particle/sky, Material binding + cache key, call sites. Verify: checks 1, 2, 9–16; Lighting, PBR, Skybox, IBL, Particle, Batch, Post unchanged.
5. On-screen panel, GPU timer, cost log.
6. Docs: new `docs/volumetrics.md`; `lighting.md` (CameraData 1888 B, `:552` limitation), `scenes-and-ecs.md`, `pbr.md` (22), `particles.md`, `environment.md` (sky fog), `shadows.md`; `.claude/reference/rendering.md` section + PBR row 22; As built.

**Needs testing on a real GPU**
- D3D11 debug layer: CS-UAV / PS-SRV hazard on `m_VolumetricIntegrated` between scenes; atlas as CS SRV after the DSV pass.
- D3D12 root signature with t12/s13/t22/t7/t3. Vulkan sync validation: inject → integrate → draw, two scenes a frame.
- FXC compile time of the integrate loop at -Od; RGBA16F transmittance precision at long range.
- Visual: ghosting under fast rotation at blend 0.1; slice banding near Near; truss shadow sharpness at 160×90 (G-H2, Headstone hall `--spawn=0,8,180,10` vs the approved capture); sky seam at Far.
- Cost estimate: inject ~0.15 ms, integrate ~0.05 ms on the RX 7900 XTX at default; per pixel 1 trilinear fetch + ~15 ALU; 21 MiB.

### Phase 5 public API choices (owner chose A for P1–P14, 2026-11-27)

| # | Question | Options | Recommended |
|---|---|---|---|
| P1 | Capability name | A `Volumetrics` + `SetVolumetricsEnabled`; B `VolumetricFog`; C enum mode | A |
| P2 | Grid size lives | A `Capabilities.VolumetricGrid` + runtime `SetVolumetricGrid`; B per scene; C create-time only | A |
| P3 | Settings shape | A `VolumetricFog` struct + `FogComponent::Volumetric` optional; B `FogMode::Volumetric`; C new component | A |
| P4 | Units | A `Density` = extinction/unit (= `Fog::Density` Exponential), falloff/unit, …; B visibility distance | A |
| P5 | Flag on, only distance fog set | A distance fog stays; B auto-convert | A |
| P6 | Colour space | A display (like lit fog); B linear | A |
| P7 | Dispatches | A inject+temporal, integrate (2); B separate temporal (3) | A |
| P8 | Shaft shadow lookup | A single-tap `DE_DirectionalShadowVolume`; B 3×3 PCF | A |
| P9 | Temporal, many scenes | A first temporal scene only; B per-view history | A |
| P10 | Custom-shader contract | A includes + auto-bound names + binding macros; B also a public texture getter | A |
| P11 | Translucent, particles, sky in Phase 5 | A all three; B translucent only | A |
| P12 | AO fade under volume | A none, documented + issue; B crude exponential | A |
| P13 | Orthographic | A no volume; B ortho grid | A |
| P14 | History blend | A fixed 0.1 internal; B public `TemporalBlend` | A |

## Phase 6: local volumes and local-light scattering

- O5 raymarch mode for box volumes (see "Headstone request" above).
- `FogVolumeComponent`: box/sphere, density, albedo, edge falloff, optional scrolling 3D noise; culled to the frustum, budget like local lights.
- Point/spot in-scatter using `LocalLightShadow` (`Shadows.glsl:128`) and the shadow atlas.
- ImGui debug panel for the grid (slice view, cost timer).

### Phase 6 design (engine-designer, 2026-11-27)

Read: plan 1-22, 359-595, 720-737; `Renderer3DVolumetrics.cpp`, `VolumetricInject.glsl`, `VolumetricMath.glsl`, `Shadows.glsl`, `Renderer3DCamera.glsl`, `Renderer3D.cpp:609-772, 1913-1954, 2864-2993`, `Renderer3D.h:725-851, 1440-1461`, `PostProcess.cpp:264-286, 578, 617-623`, `FullscreenPass.h`, `Light.h`, `Components.h:270-366`, `ImGuiDebugPanels.cpp:385-420`, `docs/volumetrics.md:180-193`.

**Gaps found**
- G1. `CopySceneDepth` copies only once per post `Begin` (`PostProcess.cpp:280`, reset at `:559`). If an earlier scene in the same `Begin` made the copy, a second scene's raymarch would read stale depth. Fix: an internal re-copy.
- G2. The raymarch needs a sampled depth, which only the post scene target has. The swap chain never has one (`NvrhiSwapChainFramebuffer.cpp:46`); other framebuffers only with `DepthSampleable` (`NvrhiFramebuffer.h:26`).
- G3. `LocalLightData` has no spare float (`Renderer3D.h:781-786`: `Color.w` is the cone scale, `SpotDirection.w` the cone offset). A per-light scatter flag goes in a new engine-only buffer, so `CameraData` and `ShadowData` stay as they are. **Phase 6 appends nothing to `CameraData`; it stays 1888 B.**
- G4. Volume submissions must become records before `ClearSceneLights` at `Renderer3D.cpp:675`, so all volume work goes into `PrepareVolumetrics` at `:649`.
- G5. To keep Phase 5's 88 PASS bit-identical: leave `DE_DirectionalShadowVolume` (`Shadows.glsl:134-167`) untouched and add a separate function next to it; the inject keeps the Phase 5 path when `Intensities.z == 0 && Intensities.w == 0`, as a uniform branch.
- G6. The engine has no timescale. Headstone scales `deltaTime` itself, so the noise drift needs a caller clock.
- G7. The lit and PBR shaders are **not touched** in Phase 6, so `CreateLitMaterial` output stays the same as v1.2.

**Data layout** (private structs in `Renderer3D.h` beside `:733-748`, each with a static_assert)
- `VolumetricData` stays 240 B. `Intensities.z` = froxel volume count; `Intensities.w` = local-light slots to loop, 0 unless some medium and some light has a non-zero local scatter.
- `FogVolumeData` (std140, b8, volatile UBO, `SetMaxWritesPerFrame(k_MaxScenesPerFrame)`, created in `Initialize` next to `:409`), 3744 B. Froxel records first, then raymarch records.

| Offset | Member | Contents |
|---|---|---|
| 0 | `Global` | x = global `LocalLightIntensity`, y = global `MaxInScatter` (0 = none), z = debug view, w = 1 if any volume has noise |
| 16 | `DirectionalScatter` | per directional light `VolumetricIntensity` |
| 32 | `LocalScatter[8]` | slot s at [s/4][s%4]: per local-light `VolumetricIntensity` |
| 160 | `Volumes[16]` | `FogVolumeRecord`, 224 B each |

- `FogVolumeRecord`, 224 B:

| Offset | Member | Contents |
|---|---|---|
| 0 | `WorldToLocal[3]` | affine rows; box is [-1,1]³, sphere is the unit ball |
| 48 | `Shape` | xyz = half extents in world units, w = 0 box / 1 sphere |
| 64 | `Medium` | density, g, edge fade (world units), raymarch steps |
| 80 | `Albedo` | rgb = albedo, a = ambient intensity |
| 96 | `ScatterTint` | rgb = tint, a = `OutsideShadowLight` |
| 112 | `Intensities` | directional, local, `MaxInScatter`, flags |
| 128 | `HeightFade` | fade start, fade end, band bottom, band top |
| 144 | `HeightBand` | band multiplier, softness, 2 reserved |
| 160 | `Fades` | near start, near end, far start, far end |
| 176 | `FadeKeep` | near keep, far keep, noise min, noise max |
| 192 | `Noise` | xyz = phase in tiles (fract, computed in double on the CPU), w = 1/scale |
| 208 | `Drift` | xyz = world displacement since the history frame |

- `RaymarchData` (b1 of the raymarch pass, volatile UBO), 128 B: `mat4 InverseViewProjection` (0), `CameraPosition` (64), `Target` (80: xy low-res size, zw full-res size), `Params` (96: x first raymarch record, y raymarch count, z scale 2/4, w froxel active), `Reserved` (112).
- Composite UBO (binding 0 of the fullscreen material, 96 B): `InverseViewProjection`, `CameraPosition`, `Low` (size, 1/size).
- Resources:
  - Noise: `m_VolumeNoise`, 32³ `R8` unorm, tileable 2-octave gradient noise from `Internal::GenerateVolumeNoise` (CPU, so tests can replicate it), uploaded as `InitialData` (3D upload exists, `NvrhiTexture.cpp:260`), 32 KB, created with the inject passes. `m_VolumeNoiseSampler`: linear Wrap, owned by Renderer3D.
  - Raymarch, lazy: `m_RaymarchScatter` (2D `RGBA16F` storage, rgb = L, a = T), `m_RaymarchDepth` (2D `R32F` storage, view distance), `m_RaymarchPass`, `m_RaymarchComposite` (a `FullscreenShader` material). On a target resize or knob change they `Reinitialize` in place, so ids stay stable.
  - Memory at 1080p: 6.2 MB half resolution, 1.6 MB quarter.

**Culling / budget** (`CullFogVolumes()`, new, top of `PrepareVolumetrics`)
- Returns at once if `!Capabilities.Volumetrics`, the camera is orthographic, or nothing was submitted (zero-cost path).
- Frustum test of each volume's bounding sphere with `Frustum::Intersects` (as for lights at `Renderer3D.cpp:2909`). Froxel-mode volumes are also culled when their distance gap is past the froxel `MaxDistance`.
- Rank by distance gap from the camera (0 inside), then submission order. Keep `MaxFogVolumes`, at most `k_MaxRaymarchVolumes = 4` raymarch. Warn once on overflow.
- Raymarch volumes **fall back to froxel** (warn once, `RaymarchFallbacks++`) unless `post.IsActive() && post.GetSceneTarget() == Renderer::GetRenderTarget()`.
- New `Statistics`: `FogVolumes`, `RaymarchVolumes`, `FogVolumesCulled`, `FogVolumesDropped`, `RaymarchFallbacks`, `RaymarchDispatches`.

**Pass order** (`Renderer3D::EndScene`)
1. `:631` `ResolveSceneLights`: in the slot loop (`:2974-2985`) write `LocalScatter[slot]` from a new `LocalLightCandidate::VolumetricIntensity`. `SubmitLight(DirectionalLight)` (`:2597`) writes `DirectionalScatter[count]`.
2. `:649` `PrepareVolumetrics`: `CullFogVolumes` → build records.
   - Froxel pass on when `Capabilities.Volumetrics && perspective && (m_VolumetricFogSet || froxelCount > 0)` (H15).
   - Without a global fog, `VolumetricData` comes from a default `VolumetricFog{}` with `Density = 0`.
   - `FogColor`/`FogParams` are zeroed **only when `m_VolumetricFogSet`**.
   - Then `PrepareRaymarch()` fills `RaymarchData`.
3. `:701` `DispatchVolumetrics` (unchanged place): upload `FogVolumeData` once per scene, then inject and integrate.
4. **New** `DrawVolumeRaymarch(shadowAtlas)` after the sky (`:754-755`), before `DrawTranslucentPass` (`:756`), in GPU timer `"VolumeRaymarch"`:
   - (a) `post.ApplyAmbientOcclusion()` (runs once per `Begin`, `PostProcess.cpp:578`), so AO does not darken the raymarched haze;
   - (b) `Internal::PostProcessAccess::RefreshSceneDepth(post)`, new private method: clear `DepthCopied`, call `CopyDepth` (G1);
   - (c) upload `FogVolumeData` if step 3 didn't;
   - (d) dispatch the raymarch, 8×8 groups over the low-res grid;
   - (e) composite with `Internal::DrawFullscreen(m_RaymarchComposite, Renderer::GetRenderTarget())`.
   - Re-`SetTexture` the depth copy and atlas only when their `GetId()` changes (as `Renderer3DVolumetrics.cpp:221-228`).
5. `ClearSceneLights` (`:2864`) clears `m_FogVolumes`.

**Shader changes**
- `VolumetricMath.glsl` (C++-compilable) gains `DE_DistanceFade(t, start, end, keepBefore, keepAfter)` (linear), `DE_HeightProfile(h, fade, band)`, `DE_BoxEdgeFade` / `DE_EllipsoidEdgeFade`, `DE_RayBoxInterval` / `DE_RayUnitSphereInterval` (local space), `DE_InterleavedGradientNoise`, `DE_CapInScatter(L, cap)` (scales by `cap / maxChannel`, no-op when cap ≤ 0).
- New `VolumetricMedia.glsl` (internal): `FogVolumeData` block at b8, `u_VolumeNoise` t9, `u_VolumeNoiseSampler` s10; `DE_FogVolumeSample(i, world, t, out sigmaT, out scatterWeight)` covering edge, height, noise and near/far fades. Fades scale in-scatter only; σt is never changed.
- `Shadows.glsl` adds `vec2 DE_DirectionalShadowVolumeParts(world)` (x = in-cascade shadow with strength, y = outside weight) and `float DE_LocalLightShadowVolume(light, world)` (one hardware compare tap, cube face pick as `:179-189`, `mix(1, s, Record.z)`). The existing function stays (G5).
- `VolumetricInject.glsl`:
  - Includes at the top: Camera, Shadows, VolumetricMath, VolumetricMedia.
  - Accumulates the global medium plus each froxel volume: σt sums; per-medium coefficients `cA += σs⊙tint·Ia`, `cD += σs⊙tint·Id`, `cDo += σs⊙tint·Id·out`, `cL += σs⊙tint·Il`; effective g is the σt-weighted mean.
  - Lights evaluated once. Directional: `HG·C·dirScatter·(cD⊙(s(1−t)+t(1−S)) + cDo⊙tS)`. Local: `π·HG(dot(normalize(p−L), −ray), g)·C·falloff²·cone²·LocalScatter·DE_LocalLightShadowVolume`, following `Renderer3D_Lit.glsl:93-104`.
  - Reprojection subtracts the drift of the dominant noisy volume (largest σt): `PreviousViewProjection·(centre − Drift)`.
  - Debug view: light terms become false colour (lit green, shadowed red, outside blue scaled by t, unshadowed yellow).
- `VolumetricIntegrate.glsl` adds b8 and stores `DE_CapInScatter(scat, Global.y)` while accumulating the uncapped sum.
- New `VolumeRaymarch.glsl` (compute):
  - Bindings: b0 Camera, b1 `RaymarchData`, b2 ShadowData, t3 atlas, s4 shadow sampler, t5 depth copy (`texelFetch`), u6 `r32f` depth out, u7 `rgba16f` scatter out, b8/t9/s10 media, t11/s12 froxel volume (`Volumetrics.glsl` with `DE_VOLUMETRIC_FOG_BINDING 11`, `_SAMPLER_BINDING 12`). Samplers ≤ 15, UAVs < 8.
  - Per low-res texel: (1) nearest of its s² full-res depths → view distance, sky = +∞; (2) per raymarch volume, intersect in local space, clip to [nearStart if keep = 0, surface]; (3) march `Steps` samples at `t0 + (i + IGN(pixel))·dt` with the analytic step (`DE_SliceTransmittance`/`DE_SliceInScatterFactor`); (4) cap per volume; if froxel is active, multiply by froxel T at the volume's entry; (5) insertion-sort by entry (≤ 4), combine `L += Tprefix·Lv`, `T *= Tv`; (6) store.
- New `VolumeRaymarchComposite.glsl` (fullscreen): pixel distance from the depth copy; 4 bilinear low-res texels weighted by bilinear × `1/(1e-3 + |di−d|/d)`; if the weight sum < 1e-3, nearest-depth texel. Output `(L/max(1−T,1e-5), 1−T)` with `BlendMode::Alpha` (= `dst·T + L`); discard when T > 0.9999 and L ≈ 0.
- New `VolumetricSlice.glsl` (compute, debug): copies slice k of the integrated or scatter volume into a 2D `RGBA16F` storage texture with exposure and a T view.
- Each new file gets a `.inl` and an `s_EmbeddedShaders` row (`EngineShaders.cpp:32-35`, `:78-81`). `VolumetricShaderKind` gains `Raymarch` and `Slice`.

**Raymarch interactions (H10)**
- Froxel + raymarch: final = `(c·T_f + L_f)·T_r + T_f(entry)·L_r`. Error: froxel haze in front of a box also takes the box's T (documented).
- Translucents and particles draw after the composite and don't see the raymarch volume (documented).
- Custom shaders need no change and no new bindings: the composite applies on top of their output, including their own distance fog.
- Raymarch-only scenes keep the distance `Fog`.

**Noise and temporal stability (H5/H16)**
- New `Renderer3D::SetFogVolumeTime(double seconds)`, persistent. `Scene::OnUpdate(dt)` adds to a private `m_FogVolumeTime`; `Scene::SubmitLights` passes it on.
- Phase = `fract(v·time/scale)` in double. `Drift = v·(time − historyTime)`, kept with the history. Δ < 0 or > 0.5 s → no drift, history dropped.
- At 4× timescale Δ is 4× larger and the reprojection moves 4× further, so stability doesn't depend on timescale.
- The raymarch has no history; its jitter is frame-stable by construction.

**Debug view (H11)**
- `Renderer3D::SetVolumetricDebugView(VolumetricDebugView)` writes `Global.z`; works in both paths.
- A "Renderer3D volumetrics" block in `RendererStatsSection` after the shadow block (`ImGuiDebugPanels.cpp:419`): stats, on/off, grid, resolution and debug-view toggles; slice slider + integrated/scatter/T choice via `Internal::Renderer3DAccess::RequestVolumetricSlice`, run at the end of `DispatchVolumetrics`, shown with `ImGui::Image` as at `:417`; `Volumetrics` and `VolumeRaymarch` timers from `Renderer::GetGpuTimers()` (`:1174`).

**ECS**
- `FogVolumeComponent { FogVolume Settings; bool Enabled = true; }` after `Components.h:366`: `DE_INSTANTIATE_COMPONENT` (`Entity.cpp`), `Internal::BuiltinComponents`, `Scene::DuplicateSubtree`. Submitted in `LightSystem::SubmitLights` after `SubmitFog` (`LightSystem.cpp:303`), `InEntityOrder`, skipping `InactiveComponent` and `!Enabled`; world matrix from `HierarchySystem::WorldMemo`. `SceneDebug` shows it read-only next to `:706-716`.
- Light components (`Components.h:275`, `:303`, directional) get `VolumetricIntensity`, through `ToLight`.

**Risks only a real GPU shows**
- D3D11: debug-layer hazards (CS-UAV → PS-SRV on raymarch textures, depth copy RTV → CS SRV); FXC compile time at -Od of the volume × step × light loops (keep bounds dynamic); `R8` 3D sampled from CS.
- D3D12: raymarch root signature (4 CBs, 4 SRVs, 2 UAVs, 3 samplers); the #1008 atlas barrier warning will grow.
- Vulkan: sync validation for depth copy → compute → composite, two scenes a frame, second depth copy in one `Begin`.
- Visual: upsample halos at quarter res; static IGN pattern on smooth haze; lag on overlapping drifting volumes; Headstone glass (translucent) not hazed by a raymarch box; truss sharpness at half res vs the approved capture.
- Cost (estimates): raymarch half res, 16 steps 0.2-0.4 ms on the RX 7900 XTX, quarter ≈ 0.1 ms; froxel inject with 8 volumes and 32 local lights ≈ +0.1 ms.

### Phase 6 implementation groups

1. **Math, API, CPU culling, ECS (no GPU change).** `VolumetricMath.glsl` additions, `Light.h` (`FogVolume`, enums, `VolumetricFog::LocalLightIntensity`/`MaxInScatter`, light `VolumetricIntensity`), `Renderer3D.h`/`.cpp` (`SubmitFogVolume`, `SetFogVolumeTime`, capabilities + setters, validation, `CullFogVolumes`, stats), `Components.h`, `Entity.cpp`, `ComponentStorage.h`, `Scene.cpp`, `LightSystem.cpp`, `SceneDebug.cpp`, `Internal::GenerateVolumeNoise`.
   - Test: doctests in `VolumetricTests.cpp` (fades, height profile, edge fades, ray intervals, IGN range, noise tiling, `DE_CapInScatter`). New "Fog Volume Test" (`--test=volume`), checks 1 (zero dispatches off/culled), 11 (budget, drop order), 12 (Scene records = direct API via `Renderer3DAccess::FogVolumeRecords`), 13 (`Inactive`/`Disabled`/duplicate), 14 (invalid input rejected, warns once).
2. **Froxel volumes on the GPU.** `FogVolumeData`, noise texture + sampler, `VolumetricMedia.glsl`, `Shadows.glsl` Parts, inject (media, fades, noise, drift reprojection), integrate cap, H15 activation, `.inl`/embedded rows.
   - Test: Fog Volume checks 2-10 read back as Froxel checks 3-5: 2 box-only keeps distance fog; 3/4 box and sphere σt inside/outside/edge; 5 Headstone height profile at 5/9/13/16 m; 6 near/far fades, T unchanged; 7 noise min = max 0 diff, min 0/max 2 matches CPU noise; 8 drift at 1× and 4× with temporal within 2/255 of non-temporal; 9 per-volume tint, g, `OutsideShadowLight`; 10 two overlapping volumes vs CPU mixture + global `MaxInScatter` cap. `--test=froxel` still 88 PASS; Shadow Test unchanged.
3. **Local-light scattering.** `DE_LocalLightShadowVolume`, inject local loop, `LocalScatter`/`DirectionalScatter` writes, light-component fields.
   - Test: check 15: point in-scatter vs CPU HG·falloff²; `LocalLightIntensity = 0` and per-light `VolumetricIntensity = 0` each 0 diff vs directional-only; spot outside cone 0; shadowed spot behind a caster ≈ 0. Froxel test still passes (Phase 5 scenes with point/spot lights set `LocalLightIntensity = 0` if their expected values assumed none).
4. **Raymarch.** `VolumeRaymarch.glsl`, `VolumeRaymarchComposite.glsl`, resources, `PostProcessAccess::RefreshSceneDepth`, `DrawVolumeRaymarch`, fallback.
   - Test: new "Raymarch Fog Test" (`--test=raymarch`): 1 fallback without post + capability off; 2 uniform ambient box vs analytic `c·T + A·albedo·(1−T)` at half and quarter; 3 wall inside box; 4 sky behind box; 5 no halo at a depth edge; 6 shaft ratio; 7 cap; 8 fades; 9 8 vs 64 steps within 1/255; 10 froxel + raymarch formula; 11 translucent over box matches doc formula; 12 custom opaque hazed same as lit; 13 two still frames 0 px diff; 14 resize keeps ids; 15 Scene = API; 16 two scenes in one `Begin` refresh depth; 17 half ↔ quarter at runtime.
5. **Debug view, panel, docs.** `VolumetricSlice.glsl`, `SetVolumetricDebugView`, `ImGuiDebugPanels.cpp` block. Docs: `docs/volumetrics.md` (volumes, raymarch, H2-H15), `scenes-and-ecs.md`, `lighting.md`, `shadows.md`, `.claude/reference/rendering.md` (b8/t9/s10, raymarch 0-12), As built.
   - Test: Fog Volume check 16 and Raymarch check 18 check false colour at known froxels/pixels; slice texture has the grid's xy size. Panel checked by hand.

### Phase 6 public API choices (owner chose A for P17–P39, 2026-11-27)

| # | Question | Options | Recommended |
|---|---|---|---|
| P17 | Settings shape | A `FogVolume` struct (Light.h) + `Renderer3D::SubmitFogVolume(const FogVolume&, const glm::mat4&)` (scene-scoped) + `FogVolumeComponent{Settings, Enabled}`; B component only | A |
| P18 | Shape | A `FogVolumeShape::Box`/`Sphere`, `HalfExtents{1}` × transform scale (sphere = ellipsoid); B unit shape, transform scale only | A |
| P19 | Mode | A `FogVolumeMode::Froxel` (default) / `Raymarch` on the volume; B separate component | A |
| P20 | Budget | A `Capabilities.MaxFogVolumes = 8` (≤ 16), ≤ 4 raymarch inside it, ranked by camera gap; B separate budgets | A |
| P21 | H2 profile | A `HeightFadeStart/End` (off when End ≤ Start) + `HeightBandBottom/Top/Multiplier = 1/Softness = 0.5`, metres above the volume's bottom along local up; B up to 8 curve keys; C world heights | A |
| P22 | H3/H14 fades | A `NearFadeStart/End/Keep` (0/0/0, off), `FarFadeStart/End/Keep` (0/0/1, off), linear in camera distance, in-scatter only; B also scale density | A |
| P23 | H3 cap | A `FogVolume::MaxInScatter` per pixel (raymarch) + `VolumetricFog::MaxInScatter` per pixel for the whole froxel volume (0 = none); B froxel per-volume cap per sample; C no froxel cap | A |
| P24 | H5 noise | A 32³ R8 CPU tileable texture; `NoiseScale` 4, `NoiseVelocity` (units/s), `NoiseMin`/`NoiseMax` 1/1 (off when equal); B ALU noise | A |
| P25 | Noise clock | A `Renderer3D::SetFogVolumeTime(double)`, Scene advances it from `OnUpdate` dt; B wall clock; C per-volume offset | A |
| P26 | Drift vs history | A reproject by the dominant noisy volume's drift; B raise blend on σt change; C no temporal with noise | A |
| P27 | H6 knobs | A resolution global (`Capabilities.VolumeRaymarchResolution` Half/Quarter + `SetVolumeRaymarchResolution`), steps per volume `RaymarchSteps` 16 (1-64); B both per volume; C both global | A |
| P28 | Raymarch depth | A post's depth copy, refreshed per raymarch scene; no post scene target → froxel fallback + warning; B Renderer3D-owned copy for any `DepthSampleable` framebuffer | A |
| P29 | Jitter | A frame-stable per-pixel IGN, no history; B temporal jitter + history; C blue-noise texture | A |
| P30 | Composite | A after opaque and sky, AO first, `BlendMode::Alpha` with `(L/(1−T), 1−T)`; B new premultiplied blend; C after translucents | A |
| P31 | H10 translucents | A don't see raymarch (documented; use Froxel for glass); B entry/exit texture sampled by lit/PBR translucents | A |
| P32 | H10 froxel + raymarch | A raymarch L × froxel T at entry; B no coupling | A |
| P33 | H15 activation | A froxel on with global fog OR a froxel volume; only global `VolumetricFog` replaces distance `Fog`; range from `VolumetricFog{}` unless set; B volumes also replace `Fog` | A |
| P34 | H13 local lights | A `LocalLightIntensity` (global + per volume) default 1, plus `VolumetricIntensity = 1` on Directional/Point/Spot lights and components (new buffer, `CameraData` untouched); B global/per-volume only; C default 0 | A |
| P35 | Local shadow in medium | A one-tap `DE_LocalLightShadowVolume`; B 3×3 `LocalLightShadow` | A |
| P36 | Overlapping media, different g | A σt-weighted g with per-medium coefficients; B a light loop per medium | A |
| P37 | H11 debug | A `SetVolumetricDebugView(None/ShadowSamples)` + Renderer Stats block with slice view and timers; B panel only | A |
| P38 | Overlapping raymarch volumes | A sorted by entry, combined in sequence; B one union march | A |
| P39 | Other `FogVolume` defaults | A `Density` 0.05, `Albedo{1}`, `Anisotropy` 0.2, `ScatterTint{1}`, `OutsideShadowLight` 1, `AmbientIntensity`/`LightIntensity` 1, `EdgeFade` 0.5; B `EdgeFade` 0 | A |

### Headstone answers that change from the "Proposed answer" column

- **H2**: heights are above the volume's bottom, not world heights; Headstone's numbers hold when the box sits at y = 0.
- **H3**: per-pixel cap is per volume only in Raymarch mode; froxel volumes share `VolumetricFog::MaxInScatter`; no global fog → no cap.
- **H6**: resolution global, steps per volume.
- **H10**: translucents don't see raymarch volumes; Headstone's hall glass must be opaque or the volume Froxel. Raymarch needs the post chain, else falls back to froxel.
- **H13**: a per-light flag fits (engine-only `FogVolumeData`): `VolumetricIntensity` on every light type.
- **H14**: near/far fades thin in-scatter only; transmittance and surface dimming unchanged.
- **H15**: a box-only froxel scene keeps the distance `Fog`; froxel range defaults to 0.5-64; set a `Density` 0 global fog to change it.
- **H5/H16**: drift reprojected with a caller clock (`SetFogVolumeTime`), so 4× is exact, not just tested.
- **O5 defaults changed**: per-pixel IGN jitter, not blue noise; composite after the sky too, AO first; the raymarch needs the post scene target.

## Phase 7: showcase, docs, review

- **D6 changed (owner, 2026-11-27): a fresh example project instead of the Candlewick upgrade.** Candlewick stays as it is. Name: `examples/Dustlight` (owner confirmed 2026-11-27). Content so far (more to come from the owner):
  - A room built with Phase 6 local volumes, with windows letting sunlight in to make sun shafts.
  - Owner picked all five (2026-11-27): a sun/time-of-day slider so the shafts sweep; dust-mote particles in the beams; PBR props lit by the window sky (IBL); a lantern whose cone scatters (Phase 6 point/spot); a toggle panel (volumetrics on/off, grid size, temporal) with the GPU cost timer.
  - **Scope (owner, 2026-11-27): the example showcases every v1.3 feature**, not only the room. That means:
    - PBR materials with glTF texture maps (albedo, normal, metallic-roughness, AO, emissive), and a metal/roughness grid;
    - HDR environments with the skybox, IBL on PBR props, and switching between environments;
    - global height fog and sun shafts;
    - Phase 6 local volumes (box/sphere, noise), the O5 raymarch mode, and point/spot scattering.
  - `--check` mode as before.
- Test cases (D6).
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

### Phase 3 (2026-11-27, `a94c3af8`)

Built as designed (owner chose A for P1–P10) in 5 code groups plus docs, with these changes:
- **Orientation:** +Y is v = 1 because the `.hdr` loader flips rows (texture row 0 is the picture's bottom); −Z = u 0.5, +X = u 0.75. Check 3 pins it.
- **HDR through post:** the sky writes `LinearToSrgb(radiance × Intensity)` into the RGBA16F post target too, like lit and PBR draws, so a sun of 20 reaches post as about 3.7, not 20. Exposure therefore acts on encoded values: −2 EV is not a quarter of the radiance. Check 8 uses −3 EV against the encoded reference and fails if post or exposure is skipped (review M3).
- **Sky direction** is `UnprojectPoint(0.5) − UnprojectPoint(0.0)`, finite for an infinite-far projection (review L6).
- **Bake failure** (review M4): a failed dispatch sets `m_BakeFailed`, keeps the equirect, logs once and stops retrying until `EnvironmentAccess::Reload`. The bake shader's hot-reload generation doesn't clear it. `Prepare` has a Tracy zone and a GPU timer "Environment" (L8).
- **AssetManager:** `.hdr` decodes on the worker through `Internal::EnvironmentAccess::DecodeHdr` (VFS + stb only); `Environment::Create` uses the same helper. The asset manager always uses the auto face size, reloads included.
- **SubmitSkybox:** a chosen component with a null `Environment` clears an earlier `SetEnvironment`, like `SubmitFog` (review L5).
- **Engine fix found on the way:** `NvrhiCommandList::Clear` re-requires the cleared colour target's current state after `ClearColorAttachment`. NVRHI's Vulkan `clearTexture` doesn't add the texture to the command buffer's references, so a target only cleared in a frame and then resized hit `VUID-vkDestroyImage-image-01000`. No barrier is recorded on any backend; noted in `.claude/reference/rendering.md`.
- **Issue filed in passing:** #339, `ImGuiRenderer::m_BindingsCache` keeps every `ImGui::Image` texture alive until shutdown.
- **Left as is (review L7):** a deleted Environment's cube lives on in its cached sky material for up to `k_MaxIdleBatchScenes` scenes (≈268 MB at a 2048 face for ~5 s), the same pattern as the lit/PBR twins.
- **Tests:**
  - "Skybox Test" (`--test=skybox`) runs checks 1–15 plus 7a and 12b (no-depth target draws the sky first); check 9 reports its nine scene cases separately. The on-screen view is a 2×2: sky with a lit and a PBR sphere, six axis views, 90° rotation, post at −1 EV with bloom.
  - Doctests in `EnvironmentTests.cpp` (convention, round trip, rotation sign, face-size rule, `.hdr` → Environment). Unit tests: 231/231.
- **Results on the owner's machine** (RX 7900 XTX, private desktops, `gpu-test.ps1`):
  - `--test=skybox`: 27/27 on Vulkan (validation), DX11 and DX12 (debug layer clean); the only error lines are check 15's 7 deliberate rejections.
  - Lighting 43, PBR 61, Post 23, Batch 75, Asset Manager 36, Hierarchy 94, Shadow 32, Render Target and Pack 51 unchanged on all three (Pack run one backend at a time).
- **Review** (engine-reviewer): 0 High, 4 Medium (docs, Doxygen on deleted copies, check 8, bake retry), 5 Low; all fixed except L7 above.
- **Needs testing on a real GPU:**
  - D3D11 debug layer and Vulkan synchronization validation during the bake (cube SRV of mip N−1 plus UAV of mip N);
  - a real 4k HDRI: sun orientation, bake time, ~64 MB at a 1024 face, seams at face edges;
  - the sky at depth exactly 1.0 with `LessEqual` on another GPU vendor.
- **Docs:** new `docs/environment.md`; changes to `docs/README.md`, `asset-pipeline.md`, `scenes-and-ecs.md`, `lighting.md`, `textures.md`, `pbr.md`, `AssetManager.h` hot-reload docs, and `.claude/reference/rendering.md` ("Environment and skybox", Vulkan clear rule).
- Doxygen isn't installed here, so the PR's `API reference` job is the check.

### Phase 4 (2026-11-27, `e15b509f`)

Built as designed (owner chose A for P1–P10) in 5 code groups plus docs and review fixes, with these changes:
- **Include rule:** the shader include preprocessor pastes each include once and ignores `#if`, so `EnvironmentBake.glsl` includes `Brdf.glsl` and `Ibl.glsl` at the top, not per variant. `Ibl.glsl` does not include `Brdf.glsl` (no include guard there); include Brdf first.
- **Prefilter source LOD** has no `+1` bias: with it, mip 5 was 7.4–7.9 % off the Monte Carlo reference; without it 2.4–2.5 % (limit 3 %). Review L6: a real sun `.hdr` may sparkle at roughness 0.2–0.4; check on the GPU and restore the bias if so.
- **LUT vs `EnvBRDFApprox`** differ by up to 0.2 at F0 = 1 (the Karis fit is loose against height-correlated Smith). The flat path keeps the approximation (P3); the furnace check compares against the LUT. IBL 1 tolerance is 4e-3 at the NoV 0 / r 0 texel.
- **`EnsureBrdfLutBaked`** takes `Prepare`'s `passes` vector (`RunAfterFrame` is private to `Renderer`). `GetBrdfLut` remembers a creation failure and logs once (review L5).
- **Shared sizes** (review L1): `Internal::k_IblFaceSize`, `k_IblMipLevels`, `k_IblIrradianceBytes` in `EnvironmentAccess.h`; a doctest pins `k_PrefilterMips == 6`.
- **`CommandList::CopyTexture`** transitions all subresources once before its per-subresource loop (review L2); `CopyBuffer` rejects a size larger than either buffer at runtime.
- **Bake errors name the stage** (cube, downsample, BRDF LUT, SH projection, specular prefilter) (review L3). A failed IBL stage still keeps the sky from drawing, as designed.
- **Scene/ECS (owner request):** `SkyboxComponent` → `SubmitSkybox` maps `Lighting` and `LightingIntensity`; a hidden sky still lights. Nothing serializes `SkyboxComponent`, so there is no save-format change. SceneDebug shows the fields read-only. IBL 16 checks the Scene path against the direct one (0 px at 1.5/255).
- **IBL Test** (`--test=ibl`): checks IBL 1–16 (29 lines) and an on-screen 2×2 view (grid under the analytic sky, white furnace, diffuse and chrome, normal-mapped AO sphere IBL vs flat). Checks start after 10 frames so the panel's resize lands first. Check 4c uses the exact Funk–Hecke form (two Monte Carlo band factors per mip) instead of a per-texel reference, which took ~6 min in Debug (review H1).
- **Results on the owner's machine** (RX 7900 XTX, private desktops), Vulkan (validation), DX11, DX12 (debug layer):
  - IBL 29/29, Skybox 27 (7 deliberate error lines), PBR 61, Lighting 43, Batch 75, Shadow 32, Post 23; 0 `VUID-`.
  - 4c worst error per mip 1–5: 0.57, 0.89, 1.02, 1.63, 2.39–2.54 %.
  - Unit tests 244/244.
- **Review** (engine-reviewer): 1 High, 1 Medium, 6 Low. Fixed H1, M1, L1, L2, L3, L5. Left: L4 (editing a bake shader at runtime doesn't re-bake until restart; Debug only), L6 (GPU check above).
- **Needs testing on a real GPU:** a real sun `.hdr` on a roughness grid (L6); one RenderDoc/PIX capture of the environment copy's barriers; lit versus PBR-with-IBL brightness in a real level.
- **Docs:** `docs/environment.md` ("Image-based lighting"), `pbr.md`, `scenes-and-ecs.md`, `lighting.md` (`CameraData` row, now 1856 B), `.claude/reference/rendering.md` (PBR bindings 14/15/20/21, copy-restore rule, include rule).

### Phase 5 (2026-11-27, uncommitted)

Built as designed (owner chose A for P1–P14; H1, H4, P15 and P16 approved) in 6 groups, with these changes:
- **Public API:** `Renderer3DCapabilities::Volumetrics` and `VolumetricGrid`, `SetVolumetricsEnabled`/`SetVolumetricGrid`, `VolumetricFog`, `SetVolumetricFog`/`ClearVolumetricFog`, `ResetVolumetricHistory`, `FogComponent::Volumetric`, `CameraComponent::ResetVolumetricHistory` (one-shot, consumed in `SceneRenderer`), `Statistics::Volumetrics/VolumetricTemporal/VolumetricDispatches`.
- **H1** `VolumetricFog::ScatterTint` (`vec3{1}`, finite, ≥ 0, unclamped, in-scatter only): `VolumetricData` is 240 B, `ScatterTint` at 224 (rgb = tint, a = `OutsideShadowLight`), mirrored in the inject kernel.
- **H4 + P15** `DE_DirectionalShadowVolume(world, outside)`: the last cascade's shadow fades to `OutsideShadowLight` over `(end − start) × CascadeBlend` (the surface shadow's width), also used outside every cascade and outside the map; `ShadowCounts.x == 0` stays unshadowed. `OutsideShadowLight` is clamped to 0..1 (NaN rejected).
- **P16** `Renderer3D::ResetVolumetricHistory()` drops the history for the next scene; the ECS route is the camera's one-shot flag.
- **Deviations from the design:**
  - `SetVolumetricFog` also rejects a negative `HeightFalloff`.
  - The integrate step is two functions in `VolumetricMath.glsl`, `DE_SliceTransmittance` and `DE_SliceInScatterFactor`, instead of one.
  - Check 12's custom shader has its own world-space vertex stage, because `Renderer3D_MeshVertex.glsl` needs `a_ObjectIndex`.
  - Check 14b fails on empty or black images, so a failed readback no longer passes.
- **Froxel Fog Test** (`--test=froxel`): 88 PASS lines on Vulkan, DX11 and DX12 after the review fixes (checks 1–20, with 1d/1e added: the original 1–16 plus 17 `ScatterTint`, 18 the cascade-end fade, 19 `OutsideShadowLight`, 20 the history reset by API and by camera). Checks 18/19 use the on-screen renderer at Near 30, Far 90 and a 600 m slab, and read the inject slices around the last cascade's end. The on-screen 2×2 view now has a PBR sphere (height panel) and, in the dense panel, a translucent sphere, an alpha and an additive particle emitter and the environment's sky. One log line after the checks gives the `Volumetrics` timer: 0.13–0.17 ms mean over four scenes a frame at 160×90×64 on the RX 7900 XTX.
- **Review (engine-reviewer, high): M1, M2, L1-L9 fixed.** M1 non-finite values now fail every check helper (`MaxChannel`, `Relative`, the 18/19 series, 4b, 17); M2 the top-left panel is a sun behind slats lit toward the camera under a roof, so beams show; L1 `EnsureVolumetricResources` re-checks the shaders each scene and a failure leaves `VolumeParams` zero, logging once; L2 no sticky create flag, the shaders are re-queried (hot reload recovers); L3 docs and `///` say the grid is allocated at first use and kept; L4 the create-time grid is clamped in the `Renderer3D` constructor; L5 past `MaxDistance` surfaces take the fog at `MaxDistance` (docs and `Light.h`); L6 AO is applied after the fog and darkens dense haze (docs); L7 `DE_DirectionalShadowVolume` scales the outside value by `ShadowStrength`; L8 custom-shader docs note `FogColor`/`FogParams` are zeroed with the volume active; L9 `IsUnused` needs a non-null texture and checks 1d/1e cover a used volume switched off (0 px, 0 dispatches). Froxel test now 88 PASS lines per backend.
- **Results** (RX 7900 XTX, private desktops, Debug): all PASS on 3 APIs, 0 `VUID-`, 0 sync, 0 error lines; D3D12 shows only the known shadow-atlas barrier warning. Unit tests 250/250.
- **Docs:** new `docs/volumetrics.md` (incl. the Headstone H1–H16 answers), `README.md`, `lighting.md`, `scenes-and-ecs.md`, `pbr.md` (binding 22), `particles.md`, `environment.md`, `shadows.md`, and `.claude/reference/rendering.md` (section, PBR row 22).
- **Needs testing on a real GPU:** the list under "Phase 5 design" still stands (D3D11 debug-layer hazards on the integrated volume between scenes, ghosting at blend 0.1 under fast rotation, slice banding near Near, truss shadow sharpness in the Headstone hall, the sky seam at Far); the D3D debug layer was clean in this run.

### Phase 6 (2026-11-27, uncommitted)

Built as designed (owner chose A for P17–P39; H11 debug view approved, binding) in 5 groups, with these changes:
- **Group 1, API, math, culling, ECS:** `FogVolume`, `FogVolumeShape`, `FogVolumeMode`, `VolumetricDebugView`, `VolumetricFog::LocalLightIntensity`/`MaxInScatter`, `VolumetricIntensity` on `DirectionalLight`/`PointLight`/`SpotLight` and their components, `Renderer3D::SubmitFogVolume`/`SetFogVolumeTime`/`SetVolumeRaymarchResolution`/`SetVolumetricDebugView`, `Capabilities.MaxFogVolumes`/`VolumeRaymarchResolution`, `FogVolumeComponent` (registered, duplicated, shown in `SceneDebug`), `CullFogVolumes`, new `Statistics`, `Internal::GenerateVolumeNoise`.
- **Group 2, froxel volumes:** `FogVolumeData` (b8, 3744 B), noise t9/s10, `VolumetricMedia.glsl`, `DE_DirectionalShadowVolumeParts`, inject media path with drift reprojection, integrate cap.
- **Group 3, local lights:** `DE_LocalLightShadowVolume`, inject local loop, `LocalScatter`/`DirectionalScatter`.
- **Group 4, raymarch:** `VolumeRaymarch.glsl`, `VolumeRaymarchComposite.glsl`, resources, `PostProcessAccess::RefreshSceneDepth`, `DrawVolumeRaymarch`, froxel fallback.
- **Group 5, debug view, panel, docs:** `SetVolumetricDebugView` writes `FogVolumeData.Global.z` (in `PrepareVolumetrics`, so raymarch-only scenes get it too); `DE_ShadowSampleColor` (lit green, shadowed red, outside blue, unshadowed yellow) in `VolumetricMedia.glsl`, used by `DE_FroxelMediaSample` and `DE_RaymarchSource` scaled by the in-scatter coefficient; the inject takes the media path when the view is on, so with `None` the Phase 5 fast path is untouched (froxel 88/88). `VolumetricSlice.glsl` (+ `.inl`, `s_EmbeddedShaders` row, `VolumetricShaderKind::Slice`), `Internal::Renderer3DAccess::RequestVolumetricSlice`/`VolumetricSlice`, run once at the end of `DispatchVolumetrics` (not counted in `VolumetricDispatches`). New "Renderer3D volumetrics" block in `RendererStatsSection` (`ImGuiDebugPanels.cpp`): stats, on/off, grid, quarter resolution and debug-view toggles, slice picker with `ImGui::Image`, the `Volumetrics` and `VolumeRaymarch` timers.
- **Deviations from the design:**
  - `ResetLightScatter()` moved after `DispatchVolumetrics` (the inject reads the scatter values).
  - GLSL record members are `VolumeAlbedo`, `VolumeTint`, `VolumeLight` and `MediaGlobal` (the C++ names clash with locals in the shaders).
  - `Intensities.w` is the gating rule for the local-light loop: 0 unless a medium and a light both have a non-zero local scatter.
  - The raymarch textures need `.SetDimension(Texture2D)`; NVRHI makes them 3D otherwise.
  - New private `PostProcessAccess::RefreshSceneDepth` (G1).
  - `k_MaxPendingFogVolumes = 256` caps the submissions held per scene.
  - Check 14 (Fog Volume Test) does not assert the warn-once; the log is read by hand.
  - The Raymarch Fog Test's `Scene::SubmitLights` check works around the default sun and ambient the scene adds when it has no light.
  - The slice dispatch is one-shot per request, answered by the next dispatching scene; the panel re-requests every frame.
- **Tests:** Fog Volume Test (`--test=volume`) 78 PASS, Raymarch Fog Test (`--test=raymarch`) 34 PASS, `--test=froxel` still 88 PASS, on Vulkan, DX11 and DX12; 0 `VUID-`, 0 sync, 0 error lines; D3D12 shows only the known shadow-atlas barrier warning. Unit tests 257/257. Check 16 (Fog Volume) reads the debug false colours at known froxels (0,0.02,0 lit; 0,0,0.02 outside; 0.02,0.02,0 unshadowed), that `None` returns the normal light, and that the slice texture has the grid's x and y size and equals the scatter slice at the requested exposure. Check 18 (Raymarch) reads red/green/yellow pixels under a roof and an uncasting sun, and the slice size.
- **Docs:** `docs/volumetrics.md` (local volumes, raymarch, debug view, Headstone H2, H3, H5, H6, H10, H11, H13, H14, H15 answered; H9 open), `README.md`, `lighting.md`, `shadows.md`, `scenes-and-ecs.md`, `.claude/reference/rendering.md`. Known limits are listed in `volumetrics.md` "Limits": the drift reprojection is less exact with strong anisotropy (0.012 at g other than 0, against the 0.0078 limit), the raymarch ignores local lights and does no near-fade clipping of the ray start (the fades weight samples only), a failed raymarch shader drops the volumes with one logged error and no froxel fallback, translucents and particles do not see raymarch volumes, and froxel haze in front of a raymarch box also takes the box's T.
- **Needs testing on a real GPU (owner):** the raymarch look against the Headstone hall capture, the depth-aware upsample at quarter resolution, the static IGN pattern on smooth haze, the drift lag under overlapping noisy volumes, the F9 volumetrics panel by hand (slice image, toggles). H9, the mid-range cost, stays open.

#### Phase 6 review fixes (2026-11-27, uncommitted)

- **M1 fixed:** the froxel in-scatter multiplies each directional light by `DirectionalScatter[i]` in the fast path too (`VolumetricInject.glsl`); Fog Volume checks 15h (a sun at 0.5 against the CPU model) and 15i (a sun at 0 equals no sun); `--test=froxel` stays 88 PASS.
- **M2 fixed:** the global `LocalLightIntensity` multiplies each volume's local-light coefficient in the shader and gates the CPU `localMedium` loop; the CPU model multiplies the two as well (15b changed to match); check 15g (global 0, volume 1).
- **M3 fixed:** the raymarch low-res pair is cached per post scene target (`Renderer3D::AcquireRaymarchTarget`, keyed by the depth-copy id, resized in place, idle pairs freed after 120 frames); `Material::SetTexture` rebinds a same-format, same-size texture without dropping the pipeline; the passes rebind only when the depth id changes. Checks 14a and 17a now assert stable ids per target across frames (they ran several Begins in one frame before); new 16c to 16f: two Begins of different sizes in one frame, stable ids across frames, both match the model.
- **L1 fixed:** the composite takes `Full / divisor` and samples `grid = uv * Full / divisor - 0.5`; the quarter silhouette tolerance went from 0.12 to 0.08 (half's value); measured 0.046 (half 0.0015).
- **L2 fixed:** the `VolumeRaymarch` timer starts after AO, and `RefreshSceneDepth` skips the copy when AO's was just made.
- **L3, L4 fixed:** wording in `Renderer3D.h`, `Light.h` and `docs/volumetrics.md` (Noise and the clock, H16, the per-target textures).
- **L5 fixed:** the tiling unit check compares the wrap step with the worst interior step.
- **L6 fixed:** checks 2a, 2b, 3 and 8 also require every interior pixel's error to stay within 20% of the modelled haze there plus 0.004 (measured excess 0); the 0.03 and 0.05 floors are unchanged.
- **L7 documented:** thin bright haze where T rounds to 1 in RGBA16F is dropped by the composite (Limits in `docs/volumetrics.md`); no code change.
- **L8 fixed:** `m_RaymarchCompositeShader` is kept until `Destroy`.
- **Tests after the fixes:** `--test=volume` 81, `--test=raymarch` 38, `--test=froxel` 88, `--test=Post Test` 23 PASS on Vulkan, DX11 and DX12; 0 `VUID-`, 0 sync, 0 error lines; only the known shadow-atlas barrier warning on D3D12. Unit tests 257 cases PASS.
