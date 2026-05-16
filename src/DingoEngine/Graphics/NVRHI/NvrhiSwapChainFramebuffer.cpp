#include "depch.h"
#include "NvrhiSwapChainFramebuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

namespace Dingo
{

	NvrhiSwapChainFramebuffer* NvrhiSwapChainFramebuffer::Create(nvrhi::ITexture* texture, const FramebufferParams& params)
	{
		return new NvrhiSwapChainFramebuffer(texture, params);
	}

	void NvrhiSwapChainFramebuffer::Initialize()
	{
		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_Texture);

		m_FramebufferHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createFramebuffer(framebufferDesc);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	void NvrhiSwapChainFramebuffer::Destroy()
	{
		// m_Texture is owned by the swap chain — do not destroy it here
		if (m_FramebufferHandle)
		{
			m_FramebufferHandle = nullptr;
		}
	}

}
