#include "depch.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "Vulkan/VulkanFramebuffer.h"

namespace DingoEngine
{

	Framebuffer* Framebuffer::Create(nvrhi::ITexture* texture)
	{
		FramebufferParams params = FramebufferParams()
			.SetTexture(texture);

		return Create(params);
	}

	Framebuffer* Framebuffer::Create(const FramebufferParams& params)
	{
		if (GraphicsContext::GetApi() == GraphicsAPI::Vulkan)
		{
			return new VulkanFramebuffer(params);
		}

		return new Framebuffer(params);
	}

	Framebuffer::Framebuffer(const FramebufferParams& params)
		: m_Params(params)
	{}

	void Framebuffer::Initialize()
	{
		const auto device = GraphicsContext::GetDeviceHandle();
		const bool targetSwapChain = true;

		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_Params.Texture);

		m_FramebufferHandle = device->createFramebuffer(framebufferDesc);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	void Framebuffer::Destroy()
	{
		if (m_FramebufferHandle)
		{
			m_FramebufferHandle->Release();
		}
	}

}
