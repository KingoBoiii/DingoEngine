#pragma once
#include "DingoEngine/Graphics/Framebuffer.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiFramebuffer : public Framebuffer
	{
	public:
		NvrhiFramebuffer(const FramebufferParams& params) : Framebuffer(params), m_Width(params.Width), m_Height(params.Height)
		{}
		virtual ~NvrhiFramebuffer() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual Texture* GetAttachment(uint32_t index) const override { return index < m_Attachments.size() ? m_Attachments[index] : nullptr; }
		virtual Texture* GetDepthAttachment() const override { return m_Params.DepthSampleable ? m_DepthAttachment : nullptr; }

	private:
		TextureParams MakeColorParams(uint32_t index) const;
		TextureParams MakeDepthParams() const;
		void CreateHandle();

	protected:
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		nvrhi::FramebufferHandle m_FramebufferHandle;
		nvrhi::Viewport m_Viewport;

		std::vector<Texture*> m_Attachments;
		Texture* m_DepthAttachment = nullptr; // NvrhiFramebuffer's own; the swap-chain subclasses set m_DepthTextureHandle alone
		nvrhi::TextureHandle m_DepthTextureHandle;

		friend class NvrhiPipeline; // Allow NvrhiPipeline to access private members
		friend class NvrhiCommandList; // Allow CommandList to access private members
		friend class ImGuiRenderer; // Allow NvrhiGraphicsContext to access private members
	};

}
