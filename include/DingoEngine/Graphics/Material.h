#pragma once
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"

#include <glm/glm.hpp>

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

		// Additive glow, independent of any light. Default black/0 so existing materials
		// (and the built-in default) render identically to before this was added.
		// NOTE: emissive is per-MATERIAL, not per-mesh. Every mesh drawn with the built-in
		// default material (MeshRendererComponent::Material == nullptr) shares Renderer3D's
		// single default-material value — give an entity its own Material for a per-mesh glow.
		glm::vec3   EmissiveColor          = { 0.0f, 0.0f, 0.0f };
		float       EmissiveStrength       = 0.0f;

		MaterialParams& SetDebugName(const std::string& name)             { DebugName = name; return *this; }
		MaterialParams& SetShader(Dingo::Shader* shader)                  { Shader = shader; return *this; }
		MaterialParams& SetCullMode(Dingo::CullMode mode)                 { CullMode = mode; return *this; }
		MaterialParams& SetFillMode(Dingo::FillMode mode)                 { FillMode = mode; return *this; }
		MaterialParams& SetFrontCounterClockwise(bool v)                  { FrontCounterClockwise = v; return *this; }
		MaterialParams& SetEmissiveColor(const glm::vec3& color)          { EmissiveColor = color; return *this; }
		MaterialParams& SetEmissiveStrength(float strength)               { EmissiveStrength = strength; return *this; }
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

		// Binds a shared, engine-owned uniform buffer at binding 0 — the "scene" UBO
		// (e.g. Renderer3D's camera + light data). When set, the material's own
		// SetUniform data binds at binding 1 and textures/samplers shift to 2+. When
		// null (the default), the material's own UBO stays at binding 0. Changing it
		// invalidates the pipeline cache (bindings are baked into the render pass).
		void SetSceneUniformBuffer(GraphicsBuffer* buffer);

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

		// Emissive is runtime-tweakable (unlike CullMode/FillMode, which are baked into the
		// cached pipeline) — it only ever feeds uniform data, so changing it does not
		// invalidate the pipeline cache.
		const glm::vec3& GetEmissiveColor()    const { return m_Params.EmissiveColor; }
		float             GetEmissiveStrength() const { return m_Params.EmissiveStrength; }
		void SetEmissiveColor(const glm::vec3& color)  { m_Params.EmissiveColor = color; }
		void SetEmissiveStrength(float strength)       { m_Params.EmissiveStrength = strength; }

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

		// Shared scene UBO (binding 0), owned by the renderer — not destroyed here.
		GraphicsBuffer*      m_SceneUniformBuffer = nullptr;

		struct PipelineCacheEntry
		{
			Pipeline*   Pipeline   = nullptr;
			RenderPass* RenderPass = nullptr;
		};
		std::unordered_map<size_t, PipelineCacheEntry> m_PipelineCache;
	};

}
