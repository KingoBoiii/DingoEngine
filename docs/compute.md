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
renders nothing (`Renderer::IsFrameSkipped`) dispatches nothing.

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

## Rules

- **Readonly or writable is the shader's choice.** A block every stage declares `readonly` binds as a
  shader-resource view (HLSL `ByteAddressBuffer`, a `t` register); one any stage writes binds as an
  unordered-access view (`RWByteAddressBuffer`, a `u` register). `Shader::IsStorageBufferReadOnly`
  says which.
- **Writes happen in compute.** D3D11 has no writable views in the vertex stage; in the fragment
  stage they share slots with render targets. Write in a kernel, read in the draw.
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
  must match the texture's.

## Checking it

The test app's **Compute Test** (`--test=compute`) fills a buffer and a storage image in one kernel,
sums neighbours through a readonly block in a second, reads both buffers in a fragment stage into an
R32F strip, and places sixteen instanced quads from the buffer in the vertex stage, then checks every
value, sum, pixel and quad by readback. It also dispatches one incrementing pass four times, reads
its buffer back, dispatches it four more times and checks both counts, and reads back a buffer made with initial
data and patched by `GraphicsBuffer::Upload`.
