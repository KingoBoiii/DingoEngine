#pragma once
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiRenderPass : public RenderPass
	{
	public:
		NvrhiRenderPass(const RenderPassParams& params)
			: RenderPass(params)
		{}
		virtual ~NvrhiRenderPass() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void SetUniformBuffer(uint32_t slot, GraphicsBuffer* buffer) override;

		virtual void SetTexture(uint32_t slot, Texture* texture, uint32_t arrayElement = 0) override;

		virtual void SetSampler(uint32_t slot, Sampler* sampler) override;

		virtual void Bake() override;

	private:
		enum class RenderPassResourceType
		{
			UniformBuffer,
			Texture,
			Sampler
		};
		struct RenderPassInput
		{
			uint32_t Slot; // Slot index for the resource
			uint32_t ArrayElement = 0; // Array element index for textures
			RenderPassResourceType Type;
			IBindableShaderResource* Handle;
		};

		std::vector<RenderPassInput> m_Resources = {}; // Store resources for the binding set

		bool m_Valid = false;

		nvrhi::BindingSetDesc m_BindingSetDesc;
		nvrhi::BindingSetHandle m_BindingSetHandle;

		friend class NvrhiCommandList; // Allow CommandList to access private members
	};

}
