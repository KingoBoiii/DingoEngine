#pragma once
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/SurfaceOverride.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

namespace Dingo
{

	namespace Internal
	{
		inline constexpr const char* k_SkinDataBlockName = "SkinData";
		struct MaterialAccess;
	}

	struct MaterialParams
	{
		std::string DebugName;
		Dingo::Shader*  Shader             = nullptr;
		Dingo::CullMode CullMode           = Dingo::CullMode::Back;
		Dingo::FillMode FillMode           = Dingo::FillMode::Solid;
		bool        FrontCounterClockwise  = true;
		// Baked into the material's pipelines, like CullMode (see PipelineParams).
		Dingo::BlendMode BlendMode         = Dingo::BlendMode::Alpha;
		bool         DepthTest             = true;
		bool         DepthWrite            = true;
		Dingo::DepthCompare DepthCompare   = Dingo::DepthCompare::Less;
		int32_t      DepthBias             = 0;
		float        SlopeScaledDepthBias  = 0.0f;
		// Depth-tested but never depth-writing, whatever DepthWrite says, so what draws after it
		// behind it still shows through. It blends with BlendMode, the "over" of BlendMode::Alpha by default.
		bool         Translucent           = false;
		// Renderer3D leaves out a mesh whose bounds the camera can't see. A vertex shader that moves
		// vertices off them (wind, waves, outlines) turns this off. Shadows come from the renderer's own
		// depth pass, which never runs the material's shader, so casters are culled either way, and so
		// is a mesh past its CullDistance.
		bool         FrustumCulled         = true;
		// How many SetUniform uploads a frame the material's volatile uniform buffer holds on Vulkan
		// (GraphicsBufferParams::MaxWritesPerFrame): one a scene for a material drawn in many scenes
		// a frame with different params.
		uint32_t     UniformWritesPerFrame = 8;
		// A custom shader that places each copy of a mesh itself, from Renderer3D's instance records
		// (Instancing.glsl): its vertex stage reads the mesh's own vertices, a_Position (0), a_Normal (1)
		// and a_TexCoord (2), and the DE_InstanceData and DE_InstanceDraw blocks. Renderer3D then draws
		// every mesh given the material instanced, however few copies, even with instancing off, and skips
		// them past Renderer3D::k_MaxInstancedDrawsPerFrame. The built-in lit materials ignore it: the
		// renderer instances them when it pays.
		bool         Instancing            = false;

		// Surface settings read by Renderer3D's lit shader: the built-in default material and any
		// material from Renderer3D::CreateLitMaterial. A custom shader implements its own.
		// Every mesh drawn with the material shares them, unless the mesh has a SurfaceOverride.
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
		MaterialParams& SetBlendMode(Dingo::BlendMode mode)               { BlendMode = mode; return *this; }
		MaterialParams& SetDepthTest(bool enabled)                        { DepthTest = enabled; return *this; }
		MaterialParams& SetDepthWrite(bool enabled)                       { DepthWrite = enabled; return *this; }
		MaterialParams& SetDepthCompare(Dingo::DepthCompare compare)      { DepthCompare = compare; return *this; }
		MaterialParams& SetDepthBias(int32_t constant, float slopeScaled) { DepthBias = constant; SlopeScaledDepthBias = slopeScaled; return *this; }
		MaterialParams& SetTranslucent(bool translucent)                  { Translucent = translucent; return *this; }
		MaterialParams& SetFrustumCulled(bool culled)                     { FrustumCulled = culled; return *this; }
		MaterialParams& SetUniformWritesPerFrame(uint32_t writes)         { UniformWritesPerFrame = writes; return *this; }
		MaterialParams& SetInstancing(bool instancing)                    { Instancing = instancing; return *this; }
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
		[[nodiscard]] static Material* Create(Shader* shader);
		[[nodiscard]] static Material* Create(const MaterialParams& params);

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

		[[deprecated("the SkinData block is deprecated: declare Skinning.glsl's DE_SKIN_PALETTE blocks; removed in 2.0")]]
		static constexpr const char* k_SkinDataBlockName = "SkinData";
		static constexpr const char* k_SkinPaletteBlockName = "DE_SkinPalette";
		static constexpr const char* k_SkinDrawBlockName = "DE_SkinDraw";

		// A storage buffer at a binding the shader's own numbering gives it (Shader::FindStorageBufferBinding);
		// null clears it. A vertex stage reads it only from a readonly block. Adding or clearing one
		// rebuilds the material's passes; replacing one only re-points their binding sets, so
		// ping-ponging two buffers compiles no pipelines.
		void SetStorageBuffer(uint32_t binding, GraphicsBuffer* buffer);
		static constexpr uint32_t k_MaxStorageBuffers = 4;

		static constexpr const char* k_ObjectDataBlockName = "DE_ObjectData";
		static constexpr const char* k_InstanceDataBlockName = "DE_InstanceData";
		static constexpr const char* k_InstanceDrawBlockName = "DE_InstanceDraw";
		static constexpr const char* k_ShadowDataBlockName = "ShadowData";
		static constexpr const char* k_ShadowAtlasName = "u_ShadowAtlas";
		static constexpr const char* k_ShadowSamplerName = "u_ShadowSampler";

		// ── Accessors ─────────────────────────────────────────────────────────

		Shader*               GetShader() const { return m_Params.Shader; }
		const MaterialParams& GetParams() const { return m_Params; }
		bool                  IsTranslucent() const { return m_Params.Translucent; }

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
		// The renderers drive these through Internal::MaterialAccess.

		// Binds a shared, engine-owned uniform buffer at binding 0 — the "scene" UBO
		// (e.g. Renderer3D's camera + light data). When set, the material's own
		// SetUniform data binds at binding 1 and textures/samplers shift to 2+. When
		// null (the default), the material's own UBO stays at binding 0. Render passes are
		// cached per scene buffer, so a material drawn by several renderers keeps one for each.
		void SetSceneUniformBuffer(GraphicsBuffer* buffer);

		// Binds Renderer3D's per-draw skinning buffer to the shader's uniform block named
		// SkinData, at whatever binding the shader gave it. Shaders without one ignore it.
		void SetSkinUniformBuffer(GraphicsBuffer* buffer);

		// Binds Renderer3D's skin arena to the shader's DE_SkinPalette storage block and its per-draw
		// base to the DE_SkinDraw uniform block (Skinning.glsl with DE_SKIN_PALETTE), each at whatever
		// binding the shader gave it. Another arena only re-points the cached passes' binding sets.
		void SetSkinPaletteBuffers(GraphicsBuffer* palette, GraphicsBuffer* draw);

		// Binds Renderer3D's shadows to the shader's ShadowData block, u_ShadowAtlas texture and
		// u_ShadowSampler sampler (Shadows.glsl), each at whatever binding the shader gave it.
		void SetShadowResources(GraphicsBuffer* shadowData, Texture* atlas, Sampler* sampler);

		// Binds Renderer3D's per-mesh surface records to the shader's DE_ObjectData storage block.
		// Another buffer only re-points the cached passes' binding sets, so the renderer can grow it
		// without new pipelines.
		void SetObjectBuffer(GraphicsBuffer* buffer);

		// Binds Renderer3D's instance records to the shader's DE_InstanceData storage block and its
		// per-draw base to the DE_InstanceDraw uniform block (Instancing.glsl).
		void SetInstanceBuffers(GraphicsBuffer* records, GraphicsBuffer* draw);

		GraphicsBuffer*             GetUniformBuffer()                       const { return m_UniformBuffer; }
		const std::vector<uint8_t>& GetUniformCPUData()                      const { return m_UniformCPUData; }
		bool                        NeedsUniformUpload(uint64_t frameIndex)  const { return m_UniformUploadFrame != frameIndex; }
		void                        MarkUniformUploaded(uint64_t frameIndex)       { m_UniformUploadFrame = frameIndex; }

		// Returns (or lazily creates) a baked RenderPass for the given vertex layout and framebuffer.
		RenderPass* GetOrCreateRenderPass(const VertexLayout& layout, Framebuffer* framebuffer);

		friend struct Internal::MaterialAccess;

	private:
		void InvalidatePipelineCache();
		// Points the cached passes' storage binding found by blockName at another buffer.
		void RepointStorageBuffer(const char* blockName, GraphicsBuffer*& current, uint64_t& currentId, GraphicsBuffer* buffer);
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
		GraphicsBuffer*      m_ObjectBuffer       = nullptr;
		uint64_t             m_ObjectBufferId     = 0;
		GraphicsBuffer*      m_InstanceBuffer     = nullptr;
		uint64_t             m_InstanceBufferId   = 0;
		GraphicsBuffer*      m_InstanceDrawBuffer = nullptr;
		GraphicsBuffer*      m_SkinPaletteBuffer  = nullptr;
		uint64_t             m_SkinPaletteBufferId = 0;
		GraphicsBuffer*      m_SkinDrawBuffer     = nullptr;

		struct StorageBinding
		{
			uint32_t Binding = 0;
			GraphicsBuffer* Buffer = nullptr;
			uint64_t BufferId = 0; // a buffer made at a freed one's address is a change
		};
		std::vector<StorageBinding> m_StorageBuffers;

		struct PipelineCacheEntry
		{
			Dingo::Pipeline*   Pipeline   = nullptr;
			Dingo::RenderPass* RenderPass = nullptr;
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
