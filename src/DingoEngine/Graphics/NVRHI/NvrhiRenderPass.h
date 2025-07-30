#pragma once
#include "DingoEngine/Graphics/RenderPass.h"

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

		virtual void SetTexture(uint32_t slot, Texture* texture) override;

		virtual void Bake() override;

	private:
		nvrhi::BindingSetDesc m_BindingSetDesc;
		nvrhi::BindingSetHandle m_BindingSetHandle;

		friend class NvrhiCommandList; // Allow CommandList to access private members
	};

}
