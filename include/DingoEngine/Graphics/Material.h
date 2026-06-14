#pragma once
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"

#include <unordered_map>
#include <vector>

namespace Dingo
{

	struct MaterialParams
	{
		std::string DebugName;
		Shader*     Shader                 = nullptr;
		CullMode    CullMode               = CullMode::Back;
		FillMode    FillMode               = FillMode::Solid;
		bool        FrontCounterClockwise  = false;

		MaterialParams& SetDebugName(const std::string& name)             { DebugName = name; return *this; }
		MaterialParams& SetShader(Dingo::Shader* shader)                  { Shader = shader; return *this; }
		MaterialParams& SetCullMode(Dingo::CullMode mode)                 { CullMode = mode; return *this; }
		MaterialParams& SetFillMode(Dingo::FillMode mode)                 { FillMode = mode; return *this; }
		MaterialParams& SetFrontCounterClockwise(bool v)                  { FrontCounterClockwise = v; return *this; }
	};

	// Material owns the pipeline cache and resource bindings for a shader.
	// It does NOT own the Shader — the caller manages Shader lifetime.
	class Material
	{
	public:
		static Material* Create(Shader* shader);
		static Material* Create(const MaterialParams& params);

	public:
		Material(const MaterialParams& params);
		~Material() = default;

		void Destroy();

		// ── Resource bindings ─────────────────────────────────────────────────

		void SetTexture(uint32_t slot, Texture* texture);
		void SetSampler(uint32_t slot, Sampler* sampler);

		Texture* GetTexture(uint32_t slot) const;
		Sampler* GetSampler(uint32_t slot) const;

		// ── Uniform data ──────────────────────────────────────────────────────

		// Stores data in a CPU buffer and marks the GPU buffer as needing upload.
		// If the required size grows, the GPU buffer is recreated and any cached
		// pipelines are invalidated so the new buffer is re-bound.
		void SetUniformData(const void* data, uint32_t size);

		template<typename T>
		void SetUniform(const T& data) { SetUniformData(&data, sizeof(T)); }

		GraphicsBuffer*             GetUniformBuffer()  const { return m_UniformBuffer; }
		const std::vector<uint8_t>& GetUniformCPUData() const { return m_UniformCPUData; }
		bool                        IsUniformDirty()    const { return m_UniformDirty; }
		void                        ClearUniformDirty()       { m_UniformDirty = false; }

		// ── Pipeline cache ────────────────────────────────────────────────────

		// Returns (or lazily creates) a baked RenderPass for the given vertex
		// layout and framebuffer.  Pipelines are cached per (layout, framebuffer)
		// combination so the same Material can be used with different mesh types
		// or render targets.
		RenderPass* GetOrCreateRenderPass(const VertexLayout& layout, Framebuffer* framebuffer);

		// ── Accessors ─────────────────────────────────────────────────────────

		Shader*               GetShader() const { return m_Params.Shader; }
		const MaterialParams& GetParams() const { return m_Params; }

	private:
		void InvalidatePipelineCache();

	private:
		MaterialParams m_Params;

		static constexpr uint32_t k_MaxTextureSlots = 16;
		static constexpr uint32_t k_MaxSamplerSlots = 16;
		Texture* m_Textures[k_MaxTextureSlots] = {};
		Sampler* m_Samplers[k_MaxSamplerSlots] = {};

		std::vector<uint8_t> m_UniformCPUData;
		GraphicsBuffer*      m_UniformBuffer = nullptr;
		bool                 m_UniformDirty  = false;

		struct PipelineCacheEntry
		{
			Pipeline*   Pipeline   = nullptr;
			RenderPass* RenderPass = nullptr;
		};
		std::unordered_map<size_t, PipelineCacheEntry> m_PipelineCache;
	};

}
