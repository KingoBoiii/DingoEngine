# Compute shaders and storage buffers

*(v0.9)*

A **compute shader** runs a kernel over a grid of threads, outside any draw: it can fill a buffer
or an image that later draws read, which is how the engine's GPU particles are simulated. A
**storage buffer** is GPU memory a shader both reads and writes, laid out as a GLSL `std430` block.

## A kernel

```glsl
#type compute
#version 450
layout(local_size_x = 64) in;
layout(std140, binding = 0) uniform Params { uvec4 Count; };
layout(std430, binding = 1) buffer Values { uint values[]; };
layout(binding = 2, rgba8) uniform writeonly image2D u_Image;

void main()
{
	uint i = gl_GlobalInvocationID.x;
	if (i < Count.x)
		values[i] = i * 3u + 1u;
}
```

```cpp
Shader* shader = Shader::CreateFromSource("Fill", source);
GraphicsBuffer* params = GraphicsBuffer::CreateUniformBuffer(sizeof(glm::uvec4));
GraphicsBuffer* values = GraphicsBuffer::CreateStorageBuffer(256 * sizeof(uint32_t));
Texture* image = Texture::Create(TextureParams().SetWidth(64).SetHeight(64)
	.SetFormat(TextureFormat::RGBA8_UNORM).SetDimension(TextureDimension::Texture2D).SetIsStorage(true));

ComputePass* fill = ComputePass::Create(ComputePassParams().SetShader(shader));
fill->SetUniformBuffer(0, params);
fill->SetStorageBuffer(1, values);
fill->SetStorageTexture(2, image);

// Every frame it runs (the uniform buffer is volatile, so it is written each frame it is bound):
Renderer::Upload(params, &count, sizeof(count));
Renderer::Dispatch(fill, (256 + 63) / 64); // thread groups
```

`Renderer::Dispatch` records the kernel into the frame's command list, in order with the draws
around it, with the barriers put in: a draw after a dispatch reads what it wrote, and so does the
same pass dispatched again (an iterative kernel), even with an upload or a `ReadBack` of its buffer
in between. NVRHI alone would skip the barrier when the bindings haven't changed, so every dispatch,
and every draw with a storage binding, asks for its storage resources' states again. A frame that
renders nothing (`Renderer::IsFrameSkipped`) dispatches nothing. `Dispatch` returns false when it
recorded nothing: a skipped frame, or a pass whose pipeline or bindings can't be built (a compute
shader broken by a hot-reload), so work that must happen once can wait for one that ran.

## Reading a storage buffer in a draw

A vertex or fragment stage reads a storage buffer through a **readonly** block; a material binds it
at the block's binding:

```glsl
layout(std430, binding = 0) readonly buffer Values { uint values[]; };
```

```cpp
material->SetStorageBuffer(0, values);
Renderer::Draw(material, 6, 16); // 16 instances: gl_InstanceIndex picks each one's data
```

`RenderPass::SetStorageBuffer` does the same for a pass of your own.

## Push constants

A compute shader may declare one push-constant block for small per-dispatch values, and the pass
sets it:

```glsl
layout(push_constant) uniform PushData { uvec4 Slot; uvec4 Extra; };
```

```cpp
const glm::uvec4 slot(1, 0xB0B, 0, 0);
pass->SetPushConstants(&slot, sizeof(slot)); // the rest of the block is zeroed
Renderer::Dispatch(pass, 1);
```

`ComputePass::SetPushConstants` copies the bytes at once and every later `Renderer::Dispatch` of the
pass pushes them, so two dispatches with different values each set their own first. A shader has at
most one block, at most 128 bytes and a multiple of 4 (`Shader::GetPushConstantBlockSize`). The call
logs an error, changes nothing and returns false when the shader declares no block or the data is
larger than it. On D3D11 and D3D12 the block takes the lowest constant-buffer register (`b0` to
`b13`) that no uniform block of the shader uses, so leave one of the fourteen free.

## Rules

- **Readonly or writable is the shader's choice.** A block every stage declares `readonly` binds as a
  shader-resource view (HLSL `ByteAddressBuffer`, a `t` register); one any stage writes binds as an
  unordered-access view (`RWByteAddressBuffer`, a `u` register). `Shader::IsStorageBufferReadOnly`
  says which.
- **Writes happen in compute.** A draw only reads storage buffers: D3D11 has no writable views in
  the vertex stage and puts a fragment stage's after its render targets, and the Vulkan device isn't
  created with `fragmentStoresAndAtomics`. Write in a kernel, read in the draw.
- **D3D11 has 8 writable slots in compute** (`u0`..`u7`): keep writable bindings below 8.
- **One binding numbering.** GLSL binding numbers are used as they are on every backend: on Vulkan
  every kind of resource shares them, on D3D each kind (`b`, `t`, `s`, `u`) has its own registers
  with the same number. Give every resource of a shader its own binding.
- **Storage buffers aren't volatile**: they keep their contents across frames. `Renderer::Upload`
  writes one inside a frame; `GraphicsBuffer::Upload` and `CreateStorageBuffer`'s `initialData` write
  it at any time, with `ReadBack`'s timing: into the frame's command list inside a frame, at once
  before the first frame, else at the next frame's start. The bytes are copied, so the source can go
  as soon as the call returns; the one-argument `Renderer::Upload(buffer)` has nothing to re-send.
- A storage image's format comes from its GLSL layout qualifier (`rgba8`, `rgba16f`, `r32f`...) and
  must match the texture's. RGBA8, RGBA16F, RGBA32F and R32F work on every backend; R8, R16F and
  R11G11B10F need Vulkan's `shaderStorageImageExtendedFormats`, which the device enables where the
  GPU has it.

## Checking it

The test app's **Compute Test** (`--test=compute`) fills a buffer and a storage image in one kernel,
sums neighbours through a readonly block in a second, reads both buffers in a fragment stage into an
R32F strip, and places sixteen instanced quads from the buffer in the vertex stage, then checks every
value, sum, pixel and quad by readback. It also dispatches one incrementing pass four times, reads
its buffer back, dispatches it four more times, uploads into its first element and dispatches four
more, checking each count; it reads back a buffer made with initial data and patched by
`GraphicsBuffer::Upload`, and draws from it in a fragment stage. It dispatches a push-constant
kernel twice with different values and draws a push-only material twice, checking each result.

## Limits

- Draws only read storage buffers; every write is a dispatch (see [Rules](#rules)).
- D3D11 has 8 writable slots in compute (`u0`..`u7`).
- R8, R16F and R11G11B10F storage images need Vulkan's `shaderStorageImageExtendedFormats`, so
  they can fail on a GPU without it.
- A dispatch needs a frame to record into: a skipped frame (minimized window) dispatches nothing,
  and a dropped `Renderer::Upload` is not redone.
