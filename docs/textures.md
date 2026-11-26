# Textures: mips, arrays, cubes and volumes

*(v1.3)*

A `Texture` is more than one image. It can have a mip chain, several array layers, six cube faces
or a 3D volume. This page covers creating them, filling and reading one mip or layer at a time,
and loading an HDR image. Writing them from a shader is in [Compute](compute.md).

## Creating one

```cpp
// A 256x256 colour texture with a full mip chain (9 levels).
Texture* mips = Texture::Create(TextureParams().SetWidth(256).SetHeight(256)
	.SetFormat(TextureFormat::RGBA8_UNORM).SetDimension(TextureDimension::Texture2D).SetMipLevels(0));

// A 2D array of 4 layers.
Texture* layers = Texture::Create(TextureParams().SetWidth(64).SetHeight(64).SetArrayLayers(4)
	.SetFormat(TextureFormat::RGBA16F).SetDimension(TextureDimension::Texture2D));

// A cube: six square faces.
Texture* cube = Texture::Create(TextureParams().SetWidth(128).SetHeight(128).SetMipLevels(0)
	.SetFormat(TextureFormat::RGBA16F).SetDimension(TextureDimension::TextureCube).SetIsStorage(true));

// A 3D volume.
Texture* volume = Texture::Create(TextureParams().SetWidth(32).SetHeight(32).SetDepth(16)
	.SetFormat(TextureFormat::RGBA16F).SetDimension(TextureDimension::Texture3D).SetIsStorage(true));
```

| Param | Meaning |
|---|---|
| `Depth` | Slices of a `Texture3D`. Default 1; above 1 needs `Texture3D`. |
| `MipLevels` | Mip levels. Default 1. 0 means the full chain down to 1x1x1. |
| `ArrayLayers` | Layers of a `Texture2D`, which makes it a 2D array. Default 1. |

- `GetMipLevelCount(width, height, depth = 1)` is the length of the full chain: the number of
  halvings of the largest side, plus one. Pass `depth` only for a 3D texture.
- A mip's size is each side halved per level, never below 1.
- `TextureDimension::TextureCube` has six faces, always, in layers 0 to 5: **+X, -X, +Y, -Y, +Z,
  -Z**. It must be square. `ArrayLayers` may be left at 1 or set to 6.
- `Texture::Create` and `Texture::Reinitialize` check the params. A bad combination logs an error
  and `Create` returns `nullptr` (`Reinitialize` keeps the old texture and its generation). Rejected: a missing dimension or size, `Depth` above 1 on
  anything but `Texture3D`, `ArrayLayers` above 1 on anything but `Texture2D` or `TextureCube`, a
  non-square cube, `MipLevels` longer than the chain, and a render target or `D32` depth texture
  that is not a single-layer `Texture2D` (a depth texture also has one mip).

## Filling and reading a subresource

A **subresource** is one mip of one layer. Layer means the array layer, the cube face, or (for a
3D texture, where it is only used when reading) the z slice.

```cpp
// Mip 0 of every layer, in layer order: all six faces back to back.
cube->Upload(allFaces);

// One mip of one face.
cube->UploadSubresource(face2Mip1, 1, 2);

cube->ReadSubresource(1, 2, [](const TexturePixels& p)
{
	// p.Width x p.Height: the size of mip 1. p.GetPixel(x, y) decodes the format.
});
```

- **Data layout.** Pixels are packed, first row first, with no row padding, in the texture's
  format. A mip of a 2D texture is `max(1, Width >> mip)` by `max(1, Height >> mip)` pixels.
- `Upload(data)` and `TextureParams::InitialData` write **mip 0 of every layer**: layer 0 first,
  then layer 1, and so on. Other mips are not written; fill them with `UploadSubresource` or from
  a shader.
- `UploadSubresource(data, mip, layer = 0)` writes one mip of one layer. For a **3D** texture it
  writes **every slice of that mip** in one call (slice 0 first, each slice one packed image), and
  `layer` must be 0. An out-of-range mip or layer logs an error and uploads nothing.
- Both join the frame's command list when one is recording, else they upload at once, like
  `Upload` always did.
- `ReadSubresource(mip, layer, done)` has the timing of `ReadPixels`: `done` runs after the frame
  that holds the copy, or at once outside a frame while the render thread is parked (see
  [Render targets](rendering-2d.md)). The pixels are one image: for a 3D texture, the slice `layer` of that mip.
- `ReadPixels(done)` is `ReadSubresource(0, 0, done)` and now works for every dimension but 1D
  (a cube gives face +X, a 3D texture slice 0). It reads RGBA8, RGBA16F, RGBA32F, R11G11B10F, R8,
  R16F and R32F, not `D32`.
- An out-of-range mip or layer, or a format or dimension it can't read, logs an error and calls
  `done` with empty pixels.
- Only the subresource that is copied, uploaded or bound changes state, so reading mip 2 of a
  cube mid-frame leaves the other mips alone.

## HDR images

```cpp
Texture* sky = Texture::CreateFromHdrFile("env/studio.hdr");                         // RGBA16F
Texture* exact = Texture::CreateFromHdrFile("env/studio.hdr", TextureFormat::RGBA32F, "Studio");
```

- Loads a Radiance `.hdr` file as a sampled 2D texture. The format must be `RGBA16F` (the
  default) or `RGBA32F`. A file that is not a `.hdr`, a failed load or another format logs an
  error and returns `nullptr`.
- It reads through the virtual file system like `CreateFromFile`: the asset root, the working
  directory and a mounted [asset package](distribution.md). Add the `.hdr` to the pack's inputs
  like any other file.
- The alpha is 1. The rows are flipped on load, exactly as `CreateFromFile` does, so row 0 of a
  readback is the bottom of the picture.
- `RGBA16F` is the default because a half takes half the memory and every GPU filters it
  linearly. The conversion rounds to nearest even and **clamps to 65504**, the largest half: a sun
  brighter than that is cut off. Negative values and NaNs become 0. Use `RGBA32F` to keep the
  exact floats.
- `CreateFromFile` and the `AssetManager` still decode an `.hdr` as 8-bit; use `CreateFromHdrFile`
  for the real values.

## Limits

- **Cube and 3D textures belong to compute and your own shaders.** `Renderer2D`, ImGui and
  DingoUI draw a texture as a 2D image and can't take a cube or a 3D volume: the view fails
  validation. Read one in a `samplerCube` or `sampler3D` of a material or compute pass, or copy a
  slice into a 2D texture.
- A render target and a depth texture are single-layer 2D textures (see the rules above).
- A texture does not generate its own mips. Write each level with `UploadSubresource` or a
  compute pass, such as a down-sample that reads mip N-1 and writes mip N.
- `Texture::Reinitialize` takes the same checks as `Create`, and a compute pass that bound a mip
  range that no longer fits makes that pass's dispatch log an error and skip until the slot is set again.

## Checking it

The test app's **Texture Subresource Test** (`--test=subresource`) round-trips every mip and face of
a cube in RGBA8 and RGBA16F, draws a cube in a pixel shader, fills cube mips, a mip chain and a 3D
volume from compute, draws then dispatches then draws with one binding set, and loads an HDR file
it writes itself. The checks read the textures back. The doctests cover mip counts, extents, the
params rules and the half-float conversion.
