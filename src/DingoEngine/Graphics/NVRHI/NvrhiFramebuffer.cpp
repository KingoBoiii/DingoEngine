#include "depch.h"
#include "NvrhiFramebuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

namespace Dingo
{

	void NvrhiFramebuffer::Initialize()
	{
		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc();

		CreateAttachments(framebufferDesc);

		m_FramebufferHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createFramebuffer(framebufferDesc);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	void NvrhiFramebuffer::Destroy()
	{
		for(auto& attachment : m_Attachments)
		{
			if (attachment)
			{
				attachment->Destroy();
			}
		}
		m_Attachments.clear();

		if (m_FramebufferHandle)
		{
			m_FramebufferHandle->Release();
		}
	}

	void NvrhiFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Params.Width = width;
		m_Params.Height = height;

		for (auto& attachment : m_Attachments)
		{
			if (attachment)
			{
				attachment->Destroy();
			}
		}
		m_Attachments.clear();

		//if (m_FramebufferHandle)
		//{
		//	m_FramebufferHandle->Release();
		//}

		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc();

		CreateAttachments(framebufferDesc);

		m_FramebufferHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createFramebuffer(framebufferDesc);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	void NvrhiFramebuffer::CreateAttachments(nvrhi::FramebufferDesc& framebufferDesc)
	{
		uint32_t index = 0;
		for (const auto& attachment : m_Params.Attachments)
		{
			TextureParams textureParams = TextureParams()
				.SetDebugName(std::format("{} ({})", m_Params.DebugName, index))
				.SetWidth(m_Params.Width)
				.SetHeight(m_Params.Height)
				.SetFormat(attachment.Format)
				.SetDimension(TextureDimension::Texture2D)
				.SetIsRenderTarget(true);

			Texture* texture = Texture::Create(textureParams);
			texture->Initialize();

			framebufferDesc.addColorAttachment(static_cast<nvrhi::ITexture*>(texture->GetTextureHandle()));

			m_Attachments.push_back(texture);

			index++;
		}
	}

}
