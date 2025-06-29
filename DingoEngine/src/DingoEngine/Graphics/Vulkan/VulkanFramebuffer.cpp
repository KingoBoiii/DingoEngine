#include "depch.h"
#include "VulkanFramebuffer.h"
#include "VulkanGraphicsContext.h"

#include <nvrhi/vulkan.h>

namespace DingoEngine
{

	VulkanFramebuffer::VulkanFramebuffer(const FramebufferParams& params)
		: Framebuffer(params)
	{}

	void VulkanFramebuffer::Initialize()
	{
		// 1. Get the Vulkan image view and format from the nvrhi texture
		auto textureViewObj = m_Params.Texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView, m_Params.Texture->getDesc().format);
		VkImageView imageView = reinterpret_cast<VkImageView>(textureViewObj.pointer);

		VkFormat colorFormat = static_cast<VkFormat>(m_Params.Texture->getDesc().format);

		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		vk::AttachmentDescription attachmentDescription = vk::AttachmentDescription()
			.setFormat(vk::Format(nvrhi::vulkan::convertFormat(m_Params.Texture->getDesc().format)))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eNone)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

		vk::AttachmentReference attachmentReference = vk::AttachmentReference()
			.setAttachment(0) // Assuming this is the first attachment
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		vk::SubpassDescription subpassDescription = vk::SubpassDescription()
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachmentCount(1)
			.setPColorAttachments(&attachmentReference);

		vk::SubpassDependency subpassDependency = vk::SubpassDependency()
			.setSrcSubpass(VK_SUBPASS_EXTERNAL) // No source subpass
			.setDstSubpass(0) // This is the first subpass
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eNone)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		vk::RenderPassCreateInfo renderPassCreateInfo = vk::RenderPassCreateInfo()
			.setAttachmentCount(1)
			.setPAttachments(&attachmentDescription)
			.setSubpassCount(1)
			.setPSubpasses(&subpassDescription)
			.setDependencyCount(1)
			.setPDependencies(&subpassDependency);

		vk::RenderPass renderPass = graphicsContext.GetDeviceHandle().createRenderPass(renderPassCreateInfo);

		vk::FramebufferCreateInfo framebufferCreateInfo = vk::FramebufferCreateInfo()
			.setRenderPass(renderPass)
			.setAttachmentCount(1)
			.setPAttachments((vk::ImageView*)&imageView)
			.setWidth(m_Params.Texture->getDesc().width)
			.setHeight(m_Params.Texture->getDesc().height)
			.setLayers(1);

		//framebufferCreateInfo.pAttachments = &imageView;

		vk::Framebuffer framebuffer = graphicsContext.GetDeviceHandle().createFramebuffer(framebufferCreateInfo);

		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_Params.Texture);

		m_FramebufferHandle = graphicsContext.GetNvrhiDevice()->createHandleForNativeFramebuffer(renderPass, framebuffer, framebufferDesc, true);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

}
