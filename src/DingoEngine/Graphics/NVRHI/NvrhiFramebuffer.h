#pragma once
#include "DingoEngine/Graphics/Framebuffer.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiFramebuffer : public Framebuffer
	{
	public:
		NvrhiFramebuffer(const FramebufferParams& params) : Framebuffer(params)
		{}
		virtual ~NvrhiFramebuffer() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetAttachment(uint32_t index) const override { return m_Attachments[index]; }

	private:
		void CreateAttachments(nvrhi::FramebufferDesc& framebufferDesc);

	protected:
		nvrhi::FramebufferHandle m_FramebufferHandle;
		nvrhi::Viewport m_Viewport;

		std::vector<Texture*> m_Attachments;

		friend class NvrhiPipeline; // Allow NvrhiPipeline to access private members
		friend class CommandList; // Allow CommandList to access private members
		friend class ImGuiRenderer; // Allow NvrhiGraphicsContext to access private members
	};

}
