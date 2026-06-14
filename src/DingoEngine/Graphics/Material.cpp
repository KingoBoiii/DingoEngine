#include "depch.h"
#include "DingoEngine/Graphics/Material.h"

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

	void Material::Destroy()
	{
		InvalidatePipelineCache();

		if (m_UniformBuffer)
		{
			m_UniformBuffer->Destroy();
			m_UniformBuffer = nullptr;
		}
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
			if (m_UniformBuffer)
			{
				m_UniformBuffer->Destroy();
				m_UniformBuffer = nullptr;
			}

			const std::string name = m_Params.DebugName.empty()
				? "MaterialUBO"
				: m_Params.DebugName + "_UBO";

			m_UniformBuffer = GraphicsBuffer::CreateUniformBuffer(size, name);
			InvalidatePipelineCache();
		}
	}

	/**************************************************
	***		PIPELINE CACHE								***
	**************************************************/

	RenderPass* Material::GetOrCreateRenderPass(const VertexLayout& layout, Framebuffer* framebuffer)
	{
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

		// Bind textures and samplers.
		// Binding layout convention: UBO occupies binding 0.
		// Texture/sampler pairs are interleaved starting at binding 1:
		//   texture[i] → binding 1 + i*2
		//   sampler[i] → binding 2 + i*2
		for (uint32_t i = 0; i < k_MaxTextureSlots; ++i)
		{
			if (m_Textures[i])
				renderPass->SetTexture(1 + i * 2, m_Textures[i]);
		}
		for (uint32_t i = 0; i < k_MaxSamplerSlots; ++i)
		{
			if (m_Samplers[i])
				renderPass->SetSampler(2 + i * 2, m_Samplers[i]);
		}

		if (m_UniformBuffer)
			renderPass->SetUniformBuffer(0, m_UniformBuffer);

		renderPass->Bake();

		m_PipelineCache[key] = { pipeline, renderPass };
		return renderPass;
	}

	void Material::InvalidatePipelineCache()
	{
		for (auto& [key, entry] : m_PipelineCache)
		{
			if (entry.RenderPass) entry.RenderPass->Destroy();
			if (entry.Pipeline)   entry.Pipeline->Destroy();
		}
		m_PipelineCache.clear();
	}

}
