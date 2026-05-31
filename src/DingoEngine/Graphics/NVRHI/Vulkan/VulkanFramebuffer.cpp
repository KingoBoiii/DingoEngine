#include "depch.h"
#include "VulkanFramebuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.h"

namespace Dingo
{

	VulkanFramebuffer* VulkanFramebuffer::Create(nvrhi::ITexture* texture, const FramebufferParams& params)
	{
		return new VulkanFramebuffer(texture, params);
	}

	void VulkanFramebuffer::Initialize()
	{
		VulkanGraphicsContext& graphicsContext = GraphicsContext::Get().As<VulkanGraphicsContext>();

		// Color image view
		auto colorViewObj = m_Texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView, m_Texture->getDesc().format);
		VkImageView colorView = reinterpret_cast<VkImageView>(colorViewObj.pointer);

		// Depth texture (if requested)
		if (m_Params.EnableDepth)
		{
			nvrhi::TextureDesc depthDesc = nvrhi::TextureDesc()
				.setDebugName("SwapChain DepthBuffer")
				.setWidth(m_Texture->getDesc().width)
				.setHeight(m_Texture->getDesc().height)
				.setFormat(nvrhi::Format::D32)
				.setDimension(nvrhi::TextureDimension::Texture2D)
				.setIsRenderTarget(true)
				.setInitialState(nvrhi::ResourceStates::DepthWrite)
				.setKeepInitialState(true);

			m_DepthTextureHandle = graphicsContext.GetNvrhiDevice()->createTexture(depthDesc);
		}

		vk::RenderPass renderPass = CreateRenderPass();

		// Build attachment list for the VkFramebuffer
		std::vector<vk::ImageView> attachmentViews = { vk::ImageView(colorView) };
		if (m_Params.EnableDepth)
		{
			auto depthViewObj = m_DepthTextureHandle->getNativeView(nvrhi::ObjectTypes::VK_ImageView, nvrhi::Format::D32);
			VkImageView depthView = reinterpret_cast<VkImageView>(depthViewObj.pointer);
			attachmentViews.push_back(vk::ImageView(depthView));
		}

		vk::FramebufferCreateInfo framebufferCreateInfo = vk::FramebufferCreateInfo()
			.setRenderPass(renderPass)
			.setAttachmentCount(static_cast<uint32_t>(attachmentViews.size()))
			.setPAttachments(attachmentViews.data())
			.setWidth(m_Texture->getDesc().width)
			.setHeight(m_Texture->getDesc().height)
			.setLayers(1);

		vk::Framebuffer framebuffer = graphicsContext.GetDeviceHandle().createFramebuffer(framebufferCreateInfo);

		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_Texture);
		if (m_Params.EnableDepth)
			framebufferDesc.setDepthAttachment(m_DepthTextureHandle);

		m_FramebufferHandle = graphicsContext.GetNvrhiDevice()->createHandleForNativeFramebuffer(renderPass, framebuffer, framebufferDesc, true);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	vk::RenderPass VulkanFramebuffer::CreateRenderPass()
	{
		VulkanGraphicsContext& graphicsContext = GraphicsContext::Get().As<VulkanGraphicsContext>();

		vk::AttachmentDescription colorAttachment = vk::AttachmentDescription()
			.setFormat(vk::Format(nvrhi::vulkan::convertFormat(m_Texture->getDesc().format)))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eNone)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

		vk::AttachmentReference colorRef = vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		// LoadOp = eLoad so the render pass reads the value written by the explicit
		// ClearDepthStencilAttachment call (which leaves the image in TransferDstOptimal).
		// NVRHI always passes setClearValueCount(0), so eClear would use garbage values.
		vk::AttachmentDescription depthAttachment = vk::AttachmentDescription()
			.setFormat(vk::Format::eD32Sfloat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eTransferDstOptimal)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::AttachmentReference depthRef = vk::AttachmentReference()
			.setAttachment(1)
			.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::SubpassDescription subpassDescription = vk::SubpassDescription()
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachmentCount(1)
			.setPColorAttachments(&colorRef)
			.setPDepthStencilAttachment(m_Params.EnableDepth ? &depthRef : nullptr);

		// Source: any prior writes (color output + depth clear via transfer)
		// Destination: color and depth/stencil attachment usage in the subpass
		vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		vk::AccessFlags        srcAccess = vk::AccessFlagBits::eNone;
		vk::AccessFlags        dstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
		if (m_Params.EnableDepth)
		{
			srcStage  |= vk::PipelineStageFlagBits::eTransfer;
			dstStage  |= vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
			srcAccess |= vk::AccessFlagBits::eTransferWrite;
			dstAccess |= vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		}

		vk::SubpassDependency subpassDependency = vk::SubpassDependency()
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(srcStage)
			.setDstStageMask(dstStage)
			.setSrcAccessMask(srcAccess)
			.setDstAccessMask(dstAccess);

		std::vector<vk::AttachmentDescription> allAttachments = { colorAttachment };
		if (m_Params.EnableDepth)
			allAttachments.push_back(depthAttachment);

		vk::RenderPassCreateInfo renderPassCreateInfo = vk::RenderPassCreateInfo()
			.setAttachmentCount(static_cast<uint32_t>(allAttachments.size()))
			.setPAttachments(allAttachments.data())
			.setSubpassCount(1)
			.setPSubpasses(&subpassDescription)
			.setDependencyCount(1)
			.setPDependencies(&subpassDependency);

		return graphicsContext.GetDeviceHandle().createRenderPass(renderPassCreateInfo);
	}

}
