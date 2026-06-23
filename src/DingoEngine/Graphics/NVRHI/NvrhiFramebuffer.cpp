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

		m_DepthTextureHandle = nullptr;
		m_FramebufferHandle = nullptr;
	}

	void NvrhiFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Width = m_Params.Width = width;
		m_Height = m_Params.Height = height;

		for (auto& attachment : m_Attachments)
		{
			if (attachment)
			{
				attachment->Destroy();
			}
		}
		m_Attachments.clear();

		m_DepthTextureHandle = nullptr;

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

		if (m_Params.EnableDepth)
		{
			const auto device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

			nvrhi::TextureDesc depthDesc = nvrhi::TextureDesc()
				.setDebugName(m_Params.DebugName + " (Depth)")
				.setWidth(m_Params.Width)
				.setHeight(m_Params.Height)
				.setFormat(nvrhi::Format::D32)
				.setDimension(nvrhi::TextureDimension::Texture2D)
				.setIsRenderTarget(true)
				.setInitialState(nvrhi::ResourceStates::DepthWrite)
				.setKeepInitialState(true);
			// Depth-only target: not sampled. D3D rejects SHADER_RESOURCE on a non-typeless
			// depth format (D32), and isShaderResource defaults to true — set it off so this
			// offscreen depth attachment is valid on the D3D back-ends too (no setter exists).
			depthDesc.isShaderResource = false;

			m_DepthTextureHandle = device->createTexture(depthDesc);
			framebufferDesc.setDepthAttachment(m_DepthTextureHandle);
		}
	}

}
