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
		bool        FrontCounterClockwise  = true;
		// Baked into the material's pipelines, like CullMode (see PipelineParams).
		BlendMode    Blend                 = BlendMode::Alpha;
		bool         DepthTest             = true;
		bool         DepthWrite            = true;
		DepthCompare DepthFunction         = DepthCompare::Less;
		int32_t      DepthBias             = 0;
		float        SlopeScaledDepthBias  = 0.0f;

		// Surface settings read by Renderer3D's lit shader: the built-in default material and any
		// material from Renderer3D::CreateLitMaterial. A custom shader implements its own.
		// They are per-MATERIAL, not per-mesh: every mesh drawn with the default material
		// (MeshRendererComponent::Material == nullptr) shares one value, so give an entity its own
		// lit material for its own glow or shine.
		//
		// Emissive is additive glow, independent of any light. Specular is the strength of the
		// highlight (0, the default, is none) and Roughness its spread, from 0 (small and sharp)
		// to 1 (wide and soft).
		glm::vec3   EmissiveColor          = { 0.0f, 0.0f, 0.0f };
		float       EmissiveStrength       = 0.0f;
		float       Roughness              = 0.5f;
		float       Specular               = 0.0f;

		MaterialParams& SetDebugName(const std::string& name)             { DebugName = name; return *this; }
		MaterialParams& SetShader(Dingo::Shader* shader)                  { Shader = shader; return *this; }
		MaterialParams& SetCullMode(Dingo::CullMode mode)                 { CullMode = mode; return *this; }
		MaterialParams& SetFillMode(Dingo::FillMode mode)                 { FillMode = mode; return *this; }
		MaterialParams& SetFrontCounterClockwise(bool v)                  { FrontCounterClockwise = v; return *this; }
		MaterialParams& SetBlendMode(BlendMode mode)                      { Blend = mode; return *this; }
		MaterialParams& SetDepthTest(bool enabled)                        { DepthTest = enabled; return *this; }
		MaterialParams& SetDepthWrite(bool enabled)                       { DepthWrite = enabled; return *this; }
		MaterialParams& SetDepthCompare(DepthCompare compare)             { DepthFunction = compare; return *this; }
		MaterialParams& SetDepthBias(int32_t constant, float slopeScaled) { DepthBias = constant; SlopeScaledDepthBias = slopeScaled; return *this; }
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
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;

		void Destroy();

		// ── Resource bindings ─────────────────────────────────────────────────

		static constexpr uint32_t k_MaxTextureSlots = 16;
		static constexpr uint32_t k_MaxSamplerSlots = 16;

		// A change rebuilds the material's cached pipelines at its next draw, so avoid swapping a
		// slot every frame. A texture created at a freed one's address is noticed at the next draw
		// (Texture::GetGeneration), but don't draw the material while a slot holds a freed texture.
		// Samplers are told apart by pointer only: when a slot's sampler is freed, clear the slot
		// before putting a new sampler in it, which may reuse the freed address.
		void SetTexture(uint32_t slot, Texture* texture);
		void SetSampler(uint32_t slot, Sampler* sampler);

		Texture* GetTexture(uint32_t slot) const;
		Sampler* GetSampler(uint32_t slot) const;

		// Bumped by every SetTexture/SetSampler that changes a slot, so a copy of these bindings
		// can tell that a slot was cleared and refilled even when the new pointer equals the old.
		uint64_t GetBindingRevision() const { return m_BindingRevision; }

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
		// null (the default), the material's own UBO stays at binding 0. Render passes are
		// cached per scene buffer, so a material drawn by several renderers keeps one for each.
		void SetSceneUniformBuffer(GraphicsBuffer* buffer);

		// Binds Renderer3D's per-draw skinning buffer to the shader's uniform block named
		// SkinData, at whatever binding the shader gave it. Shaders without one ignore it.
		void SetSkinUniformBuffer(GraphicsBuffer* buffer);

		static constexpr const char* k_SkinDataBlockName = "SkinData";

		// Binds Renderer3D's shadows to the shader's ShadowData block, u_ShadowAtlas texture and
		// u_ShadowSampler sampler (Shadows.glsl), each at whatever binding the shader gave it. A shader
		// without them ignores this.
		void SetShadowResources(GraphicsBuffer* shadowData, Texture* atlas, Sampler* sampler);

		static constexpr const char* k_ShadowDataBlockName = "ShadowData";
		static constexpr const char* k_ShadowAtlasName = "u_ShadowAtlas";
		static constexpr const char* k_ShadowSamplerName = "u_ShadowSampler";

		GraphicsBuffer*             GetUniformBuffer()                       const { return m_UniformBuffer; }
		const std::vector<uint8_t>& GetUniformCPUData()                      const { return m_UniformCPUData; }
		bool                        NeedsUniformUpload(uint64_t frameIndex)  const { return m_UniformUploadFrame != frameIndex; }
		void                        MarkUniformUploaded(uint64_t frameIndex)       { m_UniformUploadFrame = frameIndex; }

		// ── Pipeline cache ────────────────────────────────────────────────────

		// Returns (or lazily creates) a baked RenderPass for the given vertex
		// layout and framebuffer.  Pipelines are cached per (layout, framebuffer)
		// combination so the same Material can be used with different mesh types
		// or render targets.
		RenderPass* GetOrCreateRenderPass(const VertexLayout& layout, Framebuffer* framebuffer);

		// ── Accessors ─────────────────────────────────────────────────────────

		Shader*               GetShader() const { return m_Params.Shader; }
		const MaterialParams& GetParams() const { return m_Params; }

		// Never reused, unlike the material's address, so a cache keyed on it cannot hand a freed
		// material's state to a new material allocated at the same address.
		uint64_t GetId() const { return m_Id; }

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
		static uint64_t AllocateId();

	private:
		MaterialParams m_Params;
		uint64_t m_Id = AllocateId();

		Texture* m_Textures[k_MaxTextureSlots] = {};
		Sampler* m_Samplers[k_MaxSamplerSlots] = {};
		uint64_t m_BindingRevision = 0;

		std::vector<uint8_t> m_UniformCPUData;
		GraphicsBuffer*      m_UniformBuffer      = nullptr;
		uint64_t             m_UniformUploadFrame = 0; // 0 = not uploaded since the last SetUniformData

		// Shared scene UBO (binding 0), owned by the renderer — not destroyed here.
		GraphicsBuffer*      m_SceneUniformBuffer = nullptr;
		GraphicsBuffer*      m_SkinUniformBuffer  = nullptr;
		GraphicsBuffer*      m_ShadowDataBuffer   = nullptr;
		Texture*             m_ShadowAtlas        = nullptr;
		Sampler*             m_ShadowSampler      = nullptr;

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
