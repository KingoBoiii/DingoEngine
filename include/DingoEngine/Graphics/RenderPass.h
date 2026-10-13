#pragma once
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Sampler.h"

namespace Dingo
{

	/// Describes a render pass for RenderPass::Create.
	struct RenderPassParams
	{
		/// The pipeline the pass draws with; required. The pass doesn't own it: keep it alive as
		/// long as the pass is used.
		Dingo::Pipeline* Pipeline = nullptr;

		/// Sets Pipeline and returns *this, for chaining.
		RenderPassParams& SetPipeline(Dingo::Pipeline* pipeline)
		{
			Pipeline = pipeline;
			return *this;
		}
	};

	/// A pipeline plus the resources it draws with (uniform buffers, textures, samplers, storage
	/// buffers), drawn with Renderer::Draw and Renderer::DrawIndexed. Slots are the shader's own
	/// GLSL binding numbers, and the set must match the shader's reflection exactly. The pass holds
	/// the resources it is given without owning them: keep them alive while it draws. The caller
	/// owns the pass.
	class RenderPass
	{
	public:
		/// Creates the pass; the caller deletes it. Asserts when params.Pipeline is null.
		[[nodiscard]] static RenderPass* Create(const RenderPassParams& params);

	public:
		/// Stores the params; use Create(), which also makes the GPU objects.
		RenderPass(const RenderPassParams& params)
			: m_Params(params)
		{}
		/// Frees the pass's GPU objects, not the resources bound to it.
		virtual ~RenderPass() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;

		/// Binds a uniform buffer (BufferType::UniformBuffer) at the shader's binding `slot`.
		/// Takes effect at the next Bake().
		virtual void SetUniformBuffer(uint32_t slot, GraphicsBuffer* buffer) = 0;

		/// Binds a texture at `slot`; `arrayElement` picks the element of a sampler array. Setting
		/// a slot again replaces its texture, and a texture that is reinitialized or hot-reloaded
		/// is rebound at the next draw.
		virtual void SetTexture(uint32_t slot, Texture* texture, uint32_t arrayElement = 0) = 0;

		/// Binds a sampler at the shader's binding `slot`. Takes effect at the next Bake().
		virtual void SetSampler(uint32_t slot, Sampler* sampler) = 0;

		/// A storage buffer the shader reads, through a readonly block. Draws don't write storage
		/// buffers: Vulkan isn't asked for fragmentStoresAndAtomics, and D3D11 binds a pixel
		/// shader's writable views after its render targets; write in a ComputePass.
		virtual void SetStorageBuffer(uint32_t slot, GraphicsBuffer* buffer) = 0;

		/// Builds the binding set from what the Set* calls gave it. Call it after the last change
		/// and before drawing: a pass with bindings that was never baked draws nothing. Logs an
		/// error when the bindings don't match the shader's layout.
		virtual void Bake() = 0;

		/// The params the pass was created with.
		const RenderPassParams& GetParams() const { return m_Params; }
		/// The pipeline the pass draws with.
		Pipeline* GetPipeline() const { return m_Params.Pipeline; }
		/// The framebuffer the pass's pipeline renders into.
		Framebuffer* GetTargetFramebuffer() const { return m_Params.Pipeline->GetTargetFramebuffer(); }

	protected:
		RenderPassParams m_Params;	///< The params the pass was created with.
	};

}
