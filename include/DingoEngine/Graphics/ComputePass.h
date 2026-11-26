#pragma once
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Texture.h"

#include <string>

namespace Dingo
{

	/// Describes a compute pass for ComputePass::Create.
	struct ComputePassParams
	{
		std::string DebugName; ///< Name shown in graphics debuggers and in error messages.
		/// A shader with a "#type compute" stage and nothing else. Required; the pass doesn't own
		/// it.
		Dingo::Shader* Shader = nullptr;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		ComputePassParams& SetDebugName(const std::string& name)
		{
			DebugName = name;
			return *this;
		}

		ComputePassParams& SetShader(Dingo::Shader* shader)
		{
			Shader = shader;
			return *this;
		}
		///@}
	};

	/// A compute shader and what it binds, run with Renderer::Dispatch. Bindings are the shader's
	/// own GLSL binding numbers; setting a binding again replaces it. Storage buffers and images
	/// bind as the shader declares them: readonly as shader-resource views, the rest writable.
	/// NVRHI orders a dispatch against the draws and dispatches around it, so a buffer a dispatch
	/// writes can be read by the next draw. D3D11 has 8 writable slots in compute: keep writable
	/// bindings below 8. The pipeline and bindings rebuild when the shader hot-reloads or a bound
	/// texture is reinitialized. The pass doesn't own the shader or the resources bound to it: keep
	/// them alive while it is dispatched. The caller owns the pass.
	class ComputePass
	{
	public:
		/// Creates the pass; the caller deletes it. Returns nullptr (with an error) when there is no
		/// graphics device, as in a headless app. Asserts when params.Shader is null.
		[[nodiscard]] static ComputePass* Create(const ComputePassParams& params);

	public:
		/// Stores the params; use Create(), which also makes the GPU objects.
		ComputePass(const ComputePassParams& params)
			: m_Params(params)
		{}
		/// Frees the pass's GPU objects, not the shader or the resources bound to it.
		virtual ~ComputePass() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;

		/// Binds a uniform buffer (BufferType::UniformBuffer) at the shader's `binding`. The buffer
		/// is volatile: write it with Renderer::Upload in every frame that dispatches.
		virtual void SetUniformBuffer(uint32_t binding, GraphicsBuffer* buffer) = 0;
		/// Binds a storage buffer (BufferType::StorageBuffer); readonly or writable as the shader
		/// declares the block.
		virtual void SetStorageBuffer(uint32_t binding, GraphicsBuffer* buffer) = 0;
		/// Binds a texture the shader samples or reads. `baseMip` and `mipCount` pick the mip range
		/// the shader sees; mipCount 0 means every mip from baseMip. A range that doesn't fit the
		/// texture is kept as an invalid binding: Renderer::Dispatch logs an error and skips the pass
		/// until the slot is set again with valid arguments. One texture bound both here and with
		/// SetStorageTexture needs the sampled range and the storage mip to be disjoint, or the
		/// dispatch is skipped the same way.
		virtual void SetTexture(uint32_t binding, Texture* texture, uint32_t baseMip = 0, uint32_t mipCount = 0) = 0;
		/// A texture made with TextureParams::IsStorage, as an image the shader writes, at `mipLevel`
		/// (all array layers or cube faces). An out-of-range mip, or any mip above 0 of a Texture3D
		/// (D3D builds a 3D storage view over mip 0's depth), is kept as an invalid binding that
		/// makes Renderer::Dispatch log an error and skip the pass until the slot is set again.
		virtual void SetStorageTexture(uint32_t binding, Texture* texture, uint32_t mipLevel = 0) = 0;
		/// Binds a sampler at the shader's `binding`.
		virtual void SetSampler(uint32_t binding, Sampler* sampler) = 0;
		/// Copies `size` bytes to the start of the shader's push-constant block and zeroes the rest;
		/// size 0 zeroes it all. Every later Renderer::Dispatch of the pass pushes them, so two
		/// dispatches with different values each set their own first. Logs an error, changes nothing
		/// and returns false when the shader declares no push-constant block or size is larger than it.
		bool SetPushConstants(const void* data, uint32_t size);

		/// The params the pass was created with.
		const ComputePassParams& GetParams() const { return m_Params; }

	protected:
		ComputePassParams m_Params;	///< The params the pass was created with.
		uint8_t m_PushConstants[Shader::k_MaxPushConstantSize] = {}; ///< What SetPushConstants set.
	};

}
