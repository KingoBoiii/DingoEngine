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

		// Surface settings read by Renderer3D's lit shader: the built-in default material and any
		// material from Renderer3D::CreateLitMaterial. A custom shader implements its own.
		// They are per-MATERIAL, not per-mesh: every mesh drawn with the default material
		// (MeshRendererComponent::Material == nullptr) shares one value, so give an entity its own
		// lit material for its own glow or shine.
		//
		// Emissive is additive glow, independent of any light. Specular is the strength of the
		// highlight (0, the default, is none, so existing materials look as before) and Roughness
		// its spread, from 0 (small and sharp) to 1 (wide and soft).
		glm::vec3   EmissiveColor          = { 0.0f, 0.0f, 0.0f };
		float       EmissiveStrength       = 0.0f;
		float       Roughness              = 0.5f;
		float       Specular               = 0.0f;

		MaterialParams& SetDebugName(const std::string& name)             { DebugName = name; return *this; }
		MaterialParams& SetShader(Dingo::Shader* shader)                  { Shader = shader; return *this; }
		MaterialParams& SetCullMode(Dingo::CullMode mode)                 { CullMode = mode; return *this; }
		MaterialParams& SetFillMode(Dingo::FillMode mode)                 { FillMode = mode; return *this; }
		MaterialParams& SetFrontCounterClockwise(bool v)                  { FrontCounterClockwise = v; return *this; }
		MaterialParams& SetEmissiveColor(const glm::vec3& color)          { EmissiveColor = color; return *this; }
		MaterialParams& SetEmissiveStrength(float strength)               { EmissiveStrength = strength; return *this; }
		MaterialParams& SetRoughness(float roughness)                     { Roughness = roughness; return *this; }
		MaterialParams& SetSpecular(float specular)                       { Specular = specular; return *this; }
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
		// Routes through Destroy() so `delete material` without a prior Destroy() still
		// frees the pipeline cache and the uniform buffer. Both are idempotent.
		~Material();

		void Destroy();

		// ── Resource bindings ─────────────────────────────────────────────────

		// A change rebuilds the material's cached pipelines at its next draw, so avoid swapping a
		// slot every frame. Changes are detected by pointer: when a slot's texture is freed, clear
		// the slot before putting a new texture in it, which may reuse the freed address.
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

		// The surface settings are runtime-tweakable (unlike CullMode/FillMode, which are baked
		// into the cached pipeline) — they only ever feed uniform data, so changing them does
		// not invalidate the pipeline cache.
		const glm::vec3& GetEmissiveColor()    const { return m_Params.EmissiveColor; }
		float             GetEmissiveStrength() const { return m_Params.EmissiveStrength; }
		float             GetRoughness()        const { return m_Params.Roughness; }
		float             GetSpecular()         const { return m_Params.Specular; }
		void SetEmissiveColor(const glm::vec3& color)  { m_Params.EmissiveColor = color; }
		void SetEmissiveStrength(float strength)       { m_Params.EmissiveStrength = strength; }
		void SetRoughness(float roughness)             { m_Params.Roughness = roughness; }
		void SetSpecular(float specular)               { m_Params.Specular = specular; }

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

		// Shader generation the cached passes laid out their bindings for; a hot-reload can
		// change the binding layout, so a mismatch drops the whole cache. Likewise the swap
		// chain generation the cache keys were built against — a resize frees the
		// framebuffers those keys name.
		uint32_t m_BuiltShaderGeneration = 0;
		uint64_t m_BuiltResizeGeneration = 0;
	};

}
