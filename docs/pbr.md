# PBR materials

Status: v1.3. Analytic lights (directional, point, spot) with shadows, fog and AO. A skybox is
in ([Environment and skybox](environment.md)), and a scene's environment lights PBR materials; without
one the ambient term is the scene's ambient colour.

Lighting is GGX with height-correlated Smith visibility and Schlick Fresnel. Albedo and emissive maps,
the mesh colour, the ambient colour and every light's colour are authored in sRGB, as for lit
materials: the shader decodes them, lights in linear space and encodes the result back, so a white
PBR surface under full-strength white light or ambient reads the same as a white lit surface. Mid-tone
light and ambient colours fall off gamma-correct, so they come out brighter than the lit path's
display-space maths gives. With a post chain, bloom and tone mapping see the display-space values. Normal maps
need no tangents: the frame comes from screen-space derivatives, works with mirrored UVs, and falls
back to the mesh normal when the mesh has no UVs. Skinned, instanced and static-batched meshes all
take PBR materials.

PBR is opt-in and sits beside the lit materials (see [Lighting & Shading](lighting.md)): a mesh
keeps the lit material unless you give it a material from `Renderer3D::CreatePbrMaterial`. A PBR
material is metallic-roughness, as in glTF, with five texture slots; a null map reads as white
(a flat normal for the normal map). Back faces are culled by default, so a mirrored mesh (a negative
scale on one axis) needs a material whose `Material.CullMode` is `CullMode::None`.

```cpp
Material* steel = renderer.CreatePbrMaterial(PbrMaterialParams()
    .SetBaseColor({ 0.8f, 0.8f, 0.85f, 1.0f })
    .SetMetallic(1.0f)
    .SetRoughness(0.3f)
    .SetNormalMap(normalTexture));
```

| Slot (`PbrTextureSlot`) | Reads |
|---|---|
| `Albedo` | sRGB-encoded RGBA8; multiplies `BaseColor`. |
| `Normal` | Tangent space, green = +v. |
| `MetallicRoughness` | G = roughness, B = metallic. |
| `Occlusion` | R. |
| `Emissive` | sRGB-encoded RGBA8; multiplies `Emissive`. |

`SurfaceOverride::Metallic` overrides a mesh's metallic factor on a PBR material.
`Renderer3D::Statistics::PbrDraws` counts PBR draws.

## Ambient and image-based lighting

What a PBR surface gets from the sky side depends on the scene:

| Scene | Diffuse ambient | Specular ambient |
|---|---|---|
| No environment, or `Lighting` off | `AmbientColor` | A fitted approximation of the BRDF scale and bias (flat, from the ambient colour). |
| Environment with `Lighting` on | The environment's irradiance (SH9) | Its prefiltered reflections times the BRDF lookup. |

With an environment, `AmbientColor` does not reach PBR materials; lit materials keep using it. Uniform
radiance 1 at intensity 1 gives the same diffuse as a white `AmbientColor` of 1. Details and
limits are in [Environment and skybox](environment.md#image-based-lighting).

- **Occlusion** (the `Occlusion` map and `Ao`) multiplies the diffuse term and, through a specular
  occlusion curve (Lagarde and de Rousiers) that also depends on roughness and the view angle, the
  reflections, so a crevice does not mirror the sky. It does not touch direct lights.
- **Horizon.** Reflections that point below the geometric surface fade out, so a bumpy normal map does
  not reflect the ground into the sky.
- The post chain's [ambient occlusion](post-processing.md#ambient-occlusion) is separate and darkens the finished pixel.

## Importing a glTF model

`ModelLoadParams().SetPbrMaterials(true)` makes `Model::LoadFromFile` build a PBR material for every
submesh instead of the plain one, so `Renderer3D` draws the model lit by the BRDF with no code of your own:

```cpp
Model* helmet = Model::LoadFromFile("models/Helmet/Helmet.gltf", ModelLoadParams().SetPbrMaterials(true));
for (const SubMesh& submesh : helmet->GetSubMeshes())
    renderer.SubmitMesh(submesh.Mesh, transform, glm::vec4(1.0f), submesh.Material);
```

Pass white as the mesh colour: the material already applies the file's base colour factor.
`SubMesh::DiffuseTexture` is the albedo image, as without the flag. A skinned model's meshes take the
material the same way (`SubmitSkinnedMesh`, or a `SkinnedMeshRendererComponent` whose own `Material` is null).

| glTF | Becomes |
|---|---|
| `baseColorTexture`, `baseColorFactor` | Albedo map, `BaseColor` |
| `normalTexture`, its `scale` | Normal map, `NormalScale` |
| `metallicRoughnessTexture`, `metallicFactor`, `roughnessFactor` | Metallic-roughness map, `Metallic`, `Roughness` (both 1 when the file has neither) |
| `occlusionTexture`, its `strength` | Occlusion map, `Ao` |
| `emissiveTexture`, `emissiveFactor`, `KHR_materials_emissive_strength` | Emissive map, `Emissive`, `EmissiveStrength` |
| `alphaMode` `BLEND` | `MaterialParams::Translucent` |
| `alphaMode` `MASK` | Draws opaque, with one warning per model |
| `doubleSided` | `CullMode::None`; otherwise back faces are culled (a mirrored mesh needs `doubleSided`, or a material with `CullMode::None`) |

An image two slots point at, such as one packed occlusion-roughness-metallic image, loads once. Every
image gets a full mip chain built on the CPU (a box filter; a normal map is averaged as vectors and
renormalised, so distant bumps fade to flat instead of darkening; albedo and emissive are averaged in
linear light, so distant texels keep their brightness), and the materials sample with
the PBR sampler (repeat, 8x anisotropic). A file that is not glTF loads whatever its importer
maps to the same slots. Without a graphics device the flag changes nothing. `Model::Reload` keeps the
flag and the same `Material` objects, updated in place; their translucency and cull mode are baked in
when they are created, so a reload that changes `alphaMode` or `doubleSided` keeps the first values and
logs a warning naming the material.
