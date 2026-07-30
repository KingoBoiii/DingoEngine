#include "depch.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Renderer.h"

namespace Dingo
{

	namespace
	{
		// Combine a value into a running hash seed (FNV-style mix).
		void HashCombine(size_t& seed, size_t value)
		{
			seed ^= value + 0x9e3779b9ull + (seed << 6) + (seed >> 2);
		}

		// Produce a cache key from a vertex layout and a framebuffer pointer.
		size_t MakeCacheKey(const VertexLayout& layout, Framebuffer* framebuffer)
		{
			size_t seed = 0;
			HashCombine(seed, reinterpret_cast<uintptr_t>(framebuffer));
			HashCombine(seed, static_cast<size_t>(layout.Stride));
			HashCombine(seed, layout.Attributes.size());
			for (const auto& attr : layout.Attributes)
			{
				HashCombine(seed, static_cast<size_t>(attr.Format));
				HashCombine(seed, static_cast<size_t>(attr.Offset));
			}
			return seed;
		}
	}

	/**************************************************
	***		FACTORY										***
	**************************************************/

	Material* Material::Create(Shader* shader)
	{
		return Create(MaterialParams().SetShader(shader));
	}

	Material* Material::Create(const MaterialParams& params)
	{
		return new Material(params);
	}

	Material::Material(const MaterialParams& params)
		: m_Params(params)
	{}

	Material::~Material()
	{
		Destroy();
	}

	void Material::Destroy()
	{
		InvalidatePipelineCache();
		DestroyAndDelete(m_UniformBuffer);
	}

	/**************************************************
	***		RESOURCE BINDINGS							***
	**************************************************/

	void Material::SetTexture(uint32_t slot, Texture* texture)
	{
		DE_CORE_ASSERT(slot < k_MaxTextureSlots, "Texture slot out of range");
		m_Textures[slot] = texture;
	}

	void Material::SetSampler(uint32_t slot, Sampler* sampler)
	{
		DE_CORE_ASSERT(slot < k_MaxSamplerSlots, "Sampler slot out of range");
		m_Samplers[slot] = sampler;
	}

	Texture* Material::GetTexture(uint32_t slot) const
	{
		DE_CORE_ASSERT(slot < k_MaxTextureSlots, "Texture slot out of range");
		return m_Textures[slot];
	}

	Sampler* Material::GetSampler(uint32_t slot) const
	{
		DE_CORE_ASSERT(slot < k_MaxSamplerSlots, "Sampler slot out of range");
		return m_Samplers[slot];
	}

	/**************************************************
	***		UNIFORM DATA								***
	**************************************************/

	void Material::SetUniformData(const void* data, uint32_t size)
	{
		m_UniformCPUData.assign(
			static_cast<const uint8_t*>(data),
			static_cast<const uint8_t*>(data) + size);

		m_UniformDirty = true;

		// Recreate the GPU buffer if it doesn't exist or is too small.
		// This also invalidates the pipeline cache so new render passes will
		// bind the fresh buffer object.
		if (!m_UniformBuffer || m_UniformBuffer->GetByteSize() < size)
		{
			DestroyAndDelete(m_UniformBuffer);

			const std::string name = m_Params.DebugName.empty()
				? "MaterialUBO"
				: m_Params.DebugName + "_UBO";

			m_UniformBuffer = GraphicsBuffer::CreateUniformBuffer(size, name);
			InvalidatePipelineCache();
		}
	}

	void Material::SetSceneUniformBuffer(GraphicsBuffer* buffer)
	{
		if (m_SceneUniformBuffer == buffer)
			return;

		// The scene UBO is bound into the baked render pass, so a change must rebuild it.
		m_SceneUniformBuffer = buffer;
		InvalidatePipelineCache();
	}

	/**************************************************
	***		PIPELINE CACHE								***
	**************************************************/

	RenderPass* Material::GetOrCreateRenderPass(const VertexLayout& layout, Framebuffer* framebuffer)
	{
		// A hot-reloaded shader can have a different binding layout, and a cached pass only
		// knows the bindings this function set the first time. Re-baking the old desc
		// against the new layout writes descriptors NVRHI cannot validate, so throw the
		// cache away instead and let the bindings be laid out again from scratch.
		const uint32_t shaderGeneration = m_Params.Shader ? m_Params.Shader->GetGeneration() : 0;
		if (shaderGeneration != m_BuiltShaderGeneration)
		{
			InvalidatePipelineCache();
			m_BuiltShaderGeneration = shaderGeneration;
		}

		// The framebuffer in the key is usually the rotating swap-chain one, whose objects
		// are freed and reallocated on every resize. Without eviction the cache grew by a
		// pipeline + render pass per material per resize, and a reused address could even
		// return an entry built against a destroyed framebuffer. Drop it on a generation
		// change, as ImGuiRenderer's pipeline cache does.
		const uint64_t resizeGeneration = Renderer::GetSwapChainResizeGeneration();
		if (resizeGeneration != m_BuiltResizeGeneration)
		{
			InvalidatePipelineCache();
			m_BuiltResizeGeneration = resizeGeneration;
		}

		const size_t key = MakeCacheKey(layout, framebuffer);

		auto it = m_PipelineCache.find(key);
		if (it != m_PipelineCache.end())
			return it->second.RenderPass;

		Pipeline* pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName(m_Params.DebugName)
			.SetShader(m_Params.Shader)
			.SetFramebuffer(framebuffer)
			.SetVertexLayout(layout)
			.SetCullMode(m_Params.CullMode)
			.SetFillMode(m_Params.FillMode));

		RenderPass* renderPass = RenderPass::Create(RenderPassParams().SetPipeline(pipeline));
		renderPass->Initialize();

		// Binding convention:
		//   binding 0 = scene UBO (engine-provided camera/light) when SetSceneUniformBuffer is used
		//   binding 1 = the material's own uniform params (binding 0 when there is no scene UBO)
		//   textures/samplers follow, interleaved: texture[i] -> base + i*2, sampler[i] -> base+1 + i*2
		const uint32_t materialUboSlot = m_SceneUniformBuffer ? 1u : 0u;
		const uint32_t textureBase     = m_SceneUniformBuffer ? 2u : 1u;

		if (m_SceneUniformBuffer)
			renderPass->SetUniformBuffer(0, m_SceneUniformBuffer);

		if (m_UniformBuffer)
			renderPass->SetUniformBuffer(materialUboSlot, m_UniformBuffer);

		for (uint32_t i = 0; i < k_MaxTextureSlots; ++i)
		{
			if (m_Textures[i])
				renderPass->SetTexture(textureBase + i * 2, m_Textures[i]);
		}
		for (uint32_t i = 0; i < k_MaxSamplerSlots; ++i)
		{
			if (m_Samplers[i])
				renderPass->SetSampler(textureBase + 1 + i * 2, m_Samplers[i]);
		}

		renderPass->Bake();

		m_PipelineCache[key] = { pipeline, renderPass };
		return renderPass;
	}

	void Material::InvalidatePipelineCache()
	{
		// Nothing outlives the call: GetOrCreateRenderPass' result is used within one draw.
		for (auto& [key, entry] : m_PipelineCache)
		{
			DestroyAndDelete(entry.RenderPass);
			DestroyAndDelete(entry.Pipeline);
		}
		m_PipelineCache.clear();
	}

}
