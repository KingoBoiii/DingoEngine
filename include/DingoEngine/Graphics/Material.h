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

	/// Everything a Material is made from. The pipeline state (cull, fill, blend, depth) is fixed
	/// when the material is made; only the surface settings at the end can change later
	/// (Material::SetRoughness and friends).
	struct MaterialParams
	{
		/// Name for logs and GPU captures; the uniform buffer is named "<name>_UBO".
		std::string DebugName;
		/// The shader to draw with; required. The material does not own it, so it must outlive the
		/// material.
		Dingo::Shader*  Shader             = nullptr;
		/// Which faces are culled; baked into the pipelines.
		Dingo::CullMode CullMode           = Dingo::CullMode::Back;
		/// Filled triangles or wireframe; baked in.
		Dingo::FillMode FillMode           = Dingo::FillMode::Solid;
		/// Whether counter-clockwise triangles are the front faces.
		bool        FrontCounterClockwise  = true;
		/// Baked into the material's pipelines, like CullMode (see PipelineParams).
		Dingo::BlendMode BlendMode         = Dingo::BlendMode::Alpha;
		bool         DepthTest             = true; ///< Whether fragments are depth-tested.
		/// Whether passing fragments write depth; off for a Translucent material.
		bool         DepthWrite            = true;
		/// The depth test's comparison.
		Dingo::DepthCompare DepthCompare   = Dingo::DepthCompare::Less;
		/// Constant depth bias in units of the smallest depth step, for shadow-style passes (see
		/// PipelineParams::DepthBias); 0 is none.
		int32_t      DepthBias             = 0;
		/// Bias times the triangle's depth slope; see DepthBias.
		float        SlopeScaledDepthBias  = 0.0f;
		/// Depth-tested but never depth-writing, whatever DepthWrite says, so what draws after it
		/// behind it still shows through. It blends with BlendMode, the "over" of BlendMode::Alpha
		/// by default.
		bool         Translucent           = false;
		/// Renderer3D leaves out a mesh whose bounds the camera can't see. A vertex shader that
		/// moves vertices off them (wind, waves, outlines) turns this off. Shadows come from the
		/// renderer's own depth pass, which never runs the material's shader, so casters are culled
		/// either way, and so is a mesh past its CullDistance.
		bool         FrustumCulled         = true;
		/// How many SetUniform uploads a frame the material's volatile uniform buffer holds on
		/// Vulkan (GraphicsBufferParams::MaxWritesPerFrame): one a scene for a material drawn in
		/// many scenes a frame with different params.
		uint32_t     UniformWritesPerFrame = 8;
		/// A custom shader that places each copy of a mesh itself, from Renderer3D's instance
		/// records (Instancing.glsl): its vertex stage reads the mesh's own vertices, a_Position
		/// (0), a_Normal (1) and a_TexCoord (2), and the DE_InstanceData and DE_InstanceDraw
		/// blocks. Renderer3D then draws every mesh given the material instanced, however few
		/// copies, even with instancing off, and skips them past
		/// Renderer3D::k_MaxInstancedDrawsPerFrame. The built-in lit materials ignore it: the
		/// renderer instances them when it pays.
		bool         Instancing            = false;

		// Surface settings read by Renderer3D's lit shader: the built-in default material and any
		// material from Renderer3D::CreateLitMaterial. A custom shader implements its own.
		// Every mesh drawn with the material shares them, unless the mesh has a SurfaceOverride.
		//
		// Emissive is additive glow, independent of any light. Specular is the strength of the
		// highlight (0, the default, is none) and Roughness its spread, from 0 (small and sharp)
		// to 1 (wide and soft).
		/// Colour of the glow; black is none.
		glm::vec3   EmissiveColor          = { 0.0f, 0.0f, 0.0f };
		float       EmissiveStrength       = 0.0f; ///< Multiplier on EmissiveColor; 0 is no glow.
		/// Highlight spread, 0 (small, sharp) to 1 (wide, soft).
		float       Roughness              = 0.5f;
		float       Specular               = 0.0f; ///< Highlight strength; 0 is none.

		///@{
		/// Each setter sets the field of the same name and returns *this, for chaining.
		MaterialParams& SetDebugName(const std::string& name)             { DebugName = name; return *this; }
		MaterialParams& SetShader(Dingo::Shader* shader)                  { Shader = shader; return *this; }
		MaterialParams& SetCullMode(Dingo::CullMode mode)                 { CullMode = mode; return *this; }
		MaterialParams& SetFillMode(Dingo::FillMode mode)                 { FillMode = mode; return *this; }
		MaterialParams& SetFrontCounterClockwise(bool v)                  { FrontCounterClockwise = v; return *this; }
		MaterialParams& SetBlendMode(Dingo::BlendMode mode)               { BlendMode = mode; return *this; }
		MaterialParams& SetDepthTest(bool enabled)                        { DepthTest = enabled; return *this; }
		MaterialParams& SetDepthWrite(bool enabled)                       { DepthWrite = enabled; return *this; }
		MaterialParams& SetDepthCompare(Dingo::DepthCompare compare)      { DepthCompare = compare; return *this; }
		///@}
		/// Sets DepthBias to constant and SlopeScaledDepthBias to slopeScaled, and returns *this.
		MaterialParams& SetDepthBias(int32_t constant, float slopeScaled) { DepthBias = constant; SlopeScaledDepthBias = slopeScaled; return *this; }
		///@{
		/// Each setter sets the field of the same name and returns *this, for chaining.
		MaterialParams& SetTranslucent(bool translucent)                  { Translucent = translucent; return *this; }
		MaterialParams& SetFrustumCulled(bool culled)                     { FrustumCulled = culled; return *this; }
		MaterialParams& SetUniformWritesPerFrame(uint32_t writes)         { UniformWritesPerFrame = writes; return *this; }
		MaterialParams& SetInstancing(bool instancing)                    { Instancing = instancing; return *this; }
		MaterialParams& SetEmissiveColor(const glm::vec3& color)          { EmissiveColor = color; return *this; }
		MaterialParams& SetEmissiveStrength(float strength)               { EmissiveStrength = strength; return *this; }
		MaterialParams& SetRoughness(float roughness)                     { Roughness = roughness; return *this; }
		MaterialParams& SetSpecular(float specular)                       { Specular = specular; return *this; }
		///@}
	};

	/// Material owns the pipeline cache and resource bindings for a shader. It does NOT own the
	/// Shader - the caller manages Shader lifetime. It does not own the textures, samplers or
	/// storage buffers set on it either.
	///
	/// Binding layout that Renderer3D draws with: binding 0 is the scene UBO, binding 1 the
	/// material's own SetUniform block, and textures and samplers follow interleaved from binding 2
	/// (texture slot i at 2 + 2i, sampler slot i at 3 + 2i). A material drawn without a scene UBO
	/// has none, so its SetUniform block is binding 0 and the slots start at 1. The binding set
	/// must match the shader's reflection exactly.
	class Material
	{
	public:
		///@{
		/// Makes a material; the caller owns it and deletes it, and Destroy only frees its GPU
		/// objects early. The shader overload uses the default MaterialParams around it. The
		/// shader is borrowed, not owned.
		[[nodiscard]] static Material* Create(Shader* shader);
		[[nodiscard]] static Material* Create(const MaterialParams& params);
		///@}

	public:
		/// Prefer Create(); it builds the same material. Pipelines are made lazily at the first
		/// draw.
		Material(const MaterialParams& params);
		/// Routes through Destroy() so `delete material` without a prior Destroy() still
		/// frees the pipeline cache and the uniform buffer. Both are idempotent.
		~Material();
		///@{
		/// Copies would share an id and GPU buffers.
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;
		///@}

		/// Frees the cached pipelines and the uniform buffer (GPU teardown: do it in
		/// Layer::OnDetach, never while a frame that draws the material is in flight). Textures,
		/// samplers and the shader are not freed.
		void Destroy();

		// ---- Resource bindings -----------------------------------------------

		/// How many texture slots a material has; SetTexture asserts that slot is below it.
		static constexpr uint32_t k_MaxTextureSlots = 16;
		/// How many sampler slots a material has; SetSampler asserts that slot is below it.
		static constexpr uint32_t k_MaxSamplerSlots = 16;

		///@{
		/// Puts a texture or sampler in a slot (null clears it); the material does not take
		/// ownership. A change rebuilds the material's cached pipelines at its next draw, so avoid
		/// swapping a slot every frame. A texture created at a freed one's address is noticed at
		/// the next draw (Texture::GetGeneration), and a sampler at the next SetSampler
		/// (Sampler::GetId), but don't draw the material while a slot holds a freed texture or sampler.
		void SetTexture(uint32_t slot, Texture* texture);
		void SetSampler(uint32_t slot, Sampler* sampler);
		///@}

		///@{
		/// The texture or sampler in a slot; null when empty.
		Texture* GetTexture(uint32_t slot) const;
		Sampler* GetSampler(uint32_t slot) const;
		///@}

		/// Bumped by every SetTexture/SetSampler that changes a slot, so a copy of these bindings
		/// can tell that a slot was cleared and refilled even when the new pointer equals the old.
		uint64_t GetBindingRevision() const { return m_BindingRevision; }

		// ---- Uniform data ----------------------------------------------------

		/// Copies `size` bytes into the material's uniform block (laid out as the shader declares
		/// the block) and marks the GPU buffer for upload. The data reaches the GPU at the next
		/// draw that binds it, so call it before drawing, not per draw. If the required size grows,
		/// the GPU buffer is recreated and any cached pipelines are invalidated so the new buffer
		/// is re-bound.
		void SetUniformData(const void* data, uint32_t size);

		/// Uploads a struct as the uniform block; see SetUniformData. T must match the shader's
		/// block layout byte for byte.
		template<typename T>
		void SetUniform(const T& data) { SetUniformData(&data, sizeof(T)); }

		/// Copies `size` bytes to the start of the shader's push-constant block and zeroes the rest;
		/// size 0 zeroes it all. A draw pushes the bytes as they are when it is recorded: at the
		/// Renderer::Draw/DrawIndexed call, or at Renderer3D::EndScene for every mesh of that scene.
		/// Logs an error, changes nothing and returns false when the shader declares no push-constant
		/// block, when its block is DE_SkinDraw (the renderer's), or when size is larger than the block
		/// (Shader::GetPushConstantBlockSize).
		bool SetPushConstants(const void* data, uint32_t size);

		/// The old per-draw skinning uniform block's name.
		[[deprecated("the SkinData block is deprecated: declare Skinning.glsl's DE_SKIN_PALETTE blocks; removed in 2.0")]]
		static constexpr const char* k_SkinDataBlockName = "SkinData";
		/// Name of the storage block Renderer3D binds its skin palette arena to (Skinning.glsl with
		/// DE_SKIN_PALETTE).
		static constexpr const char* k_SkinPaletteBlockName = "DE_SkinPalette";
		/// Name of the push-constant block holding a draw's base index into the skin palette
		/// (Skinning.glsl with DE_SKIN_PALETTE).
		static constexpr const char* k_SkinDrawBlockName = "DE_SkinDraw";

		/// A storage buffer at a binding the shader's own numbering gives it
		/// (Shader::FindStorageBufferBinding); null clears it. A vertex stage reads it only from a
		/// readonly block. Adding or clearing one rebuilds the material's passes; replacing one
		/// only re-points their binding sets, so ping-ponging two buffers compiles no pipelines.
		/// The material does not own the buffer.
		void SetStorageBuffer(uint32_t binding, GraphicsBuffer* buffer);
		/// How many storage buffers SetStorageBuffer takes at once.
		static constexpr uint32_t k_MaxStorageBuffers = 4;

		/// Name of the storage block Renderer3D fills with per-mesh surface records.
		static constexpr const char* k_ObjectDataBlockName = "DE_ObjectData";
		/// Name of the storage block holding Renderer3D's instance records (Instancing.glsl).
		static constexpr const char* k_InstanceDataBlockName = "DE_InstanceData";
		/// Name of the uniform block holding an instanced draw's base index (Instancing.glsl).
		static constexpr const char* k_InstanceDrawBlockName = "DE_InstanceDraw";
		/// Name of the uniform block Renderer3D fills with the shadow data (Shadows.glsl).
		static constexpr const char* k_ShadowDataBlockName = "ShadowData";
		/// Name of the shadow atlas texture a shader declares (Shadows.glsl).
		static constexpr const char* k_ShadowAtlasName = "u_ShadowAtlas";
		/// Name of the shadow atlas sampler a shader declares (Shadows.glsl).
		static constexpr const char* k_ShadowSamplerName = "u_ShadowSampler";

		// ---- Accessors -------------------------------------------------------

		/// The shader the material draws with; not owned by the material.
		Shader*               GetShader() const { return m_Params.Shader; }
		/// The settings the material was made with; the surface settings show later setter calls.
		const MaterialParams& GetParams() const { return m_Params; }
		/// Whether the material was made Translucent.
		bool                  IsTranslucent() const { return m_Params.Translucent; }

		/// Never reused, unlike the material's address, so a cache keyed on it cannot hand a freed
		/// material's state to a new material allocated at the same address.
		uint64_t GetId() const { return m_Id; }

		// The surface settings are runtime-tweakable (unlike CullMode/FillMode, which are baked
		// into the cached pipeline) - they only ever feed uniform data, so changing them does
		// not invalidate the pipeline cache.
		/// The emissive glow colour; black is none.
		const glm::vec3& GetEmissiveColor()    const { return m_Params.EmissiveColor; }
		/// The multiplier on the emissive colour.
		float             GetEmissiveStrength() const { return m_Params.EmissiveStrength; }
		/// The highlight spread, 0 (small, sharp) to 1 (wide, soft).
		float             GetRoughness()        const { return m_Params.Roughness; }
		/// The highlight strength; 0 is none.
		float             GetSpecular()         const { return m_Params.Specular; }
		///@{
		/// Change the lit shader's surface settings at run time; free to call every frame and read
		/// by every mesh drawn with this material that has no SurfaceOverride.
		void SetEmissiveColor(const glm::vec3& color)  { m_Params.EmissiveColor = color; }
		void SetEmissiveStrength(float strength)       { m_Params.EmissiveStrength = strength; }
		void SetRoughness(float roughness)             { m_Params.Roughness = roughness; }
		void SetSpecular(float specular)               { m_Params.Specular = specular; }
		///@}

	private:
		// The renderers drive these through Internal::MaterialAccess.

		// Binds a shared, engine-owned uniform buffer at binding 0 - the "scene" UBO
		// (e.g. Renderer3D's camera + light data). When set, the material's own
		// SetUniform data binds at binding 1 and textures/samplers shift to 2+. When
		// null (the default), the material's own UBO stays at binding 0. Render passes are
		// cached per scene buffer, so a material drawn by several renderers keeps one for each.
		void SetSceneUniformBuffer(GraphicsBuffer* buffer);

		// Binds Renderer3D's per-draw skinning buffer to the shader's uniform block named
		// SkinData, at whatever binding the shader gave it. Shaders without one ignore it.
		void SetSkinUniformBuffer(GraphicsBuffer* buffer);

		// Binds Renderer3D's skin arena to the shader's DE_SkinPalette storage block (Skinning.glsl
		// with DE_SKIN_PALETTE), at whatever binding the shader gave it. Another arena only re-points
		// the cached passes' binding sets. A draw's base goes in its draw constants.
		void SetSkinPaletteBuffer(GraphicsBuffer* palette);

		// Pushed with each draw of this material to the shader's push-constant block (DE_SkinDraw);
		// Renderer3D sets them just before the draw.
		void SetDrawConstants(const glm::uvec4& constants);
		const uint8_t* GetPushConstantData() const { return m_PushConstants; }

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
		// The same with another cull mode than the material's own, cached beside its own passes.
		RenderPass* GetOrCreateRenderPass(const VertexLayout& layout, Framebuffer* framebuffer, CullMode cullMode);

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
		uint64_t m_SamplerIds[k_MaxSamplerSlots] = {};
		uint64_t m_BindingRevision = 0;

		std::vector<uint8_t> m_UniformCPUData;
		GraphicsBuffer*      m_UniformBuffer      = nullptr;
		uint64_t             m_UniformUploadFrame = 0; // 0 = not uploaded since the last SetUniformData

		// Shared scene UBO (binding 0), owned by the renderer - not destroyed here.
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
		uint8_t              m_PushConstants[Shader::k_MaxPushConstantSize] = {};

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
		bool m_MissingPipelineInputWarned = false;

		// Shader generation the cached passes laid out their bindings for; a hot-reload can
		// change the binding layout, so a mismatch drops the whole cache. Likewise the swap
		// chain generation the cache keys were built against - a resize frees the
		// framebuffers those keys name.
		uint32_t m_BuiltShaderGeneration = 0;
		uint64_t m_BuiltResizeGeneration = 0;
	};

}
