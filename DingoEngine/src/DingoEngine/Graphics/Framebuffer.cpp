#include "depch.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "Vulkan/VulkanFramebuffer.h"

namespace DingoEngine
{

	Framebuffer* Framebuffer::Create(nvrhi::ITexture* texture)
	{
		if(GraphicsContext::GetApi() == GraphicsAPI::Vulkan)
		{
			return new VulkanFramebuffer(texture);
		}

		return new Framebuffer(texture);
	}

	Framebuffer::Framebuffer(nvrhi::ITexture* texture)
		: m_Texture(texture)
	{}

	void Framebuffer::Initialize()
	{
		const auto device = GraphicsContext::GetDeviceHandle();
		const bool targetSwapChain = true;

		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_Texture);

		m_FramebufferHandle = device->createFramebuffer(framebufferDesc);
	}

	void Framebuffer::Destroy()
	{
		if (m_FramebufferHandle)
		{
			m_FramebufferHandle->Release();
		}
	}

}
