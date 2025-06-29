#include "depch.h"
#include "VulkanSwapChain.h"

#include <glfw/glfw3.h>
#include "VulkanGraphicsContext.h"

#undef max

namespace DingoEngine
{

	namespace Utils
	{

		template <typename T>
		static std::vector<T> setToVector(const std::unordered_set<T>& set)
		{
			std::vector<T> ret;
			for (const auto& s : set)
			{
				ret.push_back(s);
			}

			return ret;
		}

	}

	VulkanSwapChain::VulkanSwapChain(const SwapChainOptions& options)
		: SwapChain(options)
	{}

	VulkanSwapChain::~VulkanSwapChain()
	{}

	void VulkanSwapChain::Initialize()
	{
		CreateWindowSurface();
		CreateSwapChain();
		CreateFramebuffers();
		CreateSynchronizationObjects();
	}

	void VulkanSwapChain::Destroy()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		DestroySwapChain();

		for (auto& semaphore : m_PresentSemaphores)
		{
			if (semaphore)
			{
				graphicsContext.m_VulkanDevice.destroySemaphore(semaphore);
				semaphore = nullptr;
			}
		}

		for (auto& semaphore : m_AcquireSemaphores)
		{
			if (semaphore)
			{
				graphicsContext.m_VulkanDevice.destroySemaphore(semaphore);
				semaphore = nullptr;
			}
		}

		for (auto& fence : m_InFlightFences)
		{
			if (fence)
			{
				graphicsContext.m_VulkanDevice.destroyFence(fence);
				fence = nullptr;
			}
		}

		m_PresentSemaphores.clear();
		m_AcquireSemaphores.clear();
		m_InFlightFences.clear();

		if (m_WindowSurface)
		{
			graphicsContext.m_VulkanInstance.destroySurfaceKHR(m_WindowSurface);
			m_WindowSurface = nullptr;
		}
	}

	void VulkanSwapChain::Resize(int32_t width, int32_t height)
	{
		m_Options.Width = width;
		m_Options.Height = height;

		RecreateSwapChain();
	}

	void VulkanSwapChain::AcquireNextImage()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		// Wait for the previous frame to finish
		{
			vk::Result result;
			result = graphicsContext.m_VulkanDevice.waitForFences(1, &m_InFlightFences[m_AcquireSemaphoreIndex], VK_TRUE, UINT64_MAX);
			DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to wait for fence.");

			result = graphicsContext.m_VulkanDevice.resetFences(1, &m_InFlightFences[m_AcquireSemaphoreIndex]);
			DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to reset fence.");
		}

		vk::Result result;

		const int32_t maxAttempts = 3;
		for (size_t attempt = 0; attempt < maxAttempts; ++attempt)
		{
			result = graphicsContext.m_VulkanDevice.acquireNextImageKHR(m_SwapChain, std::numeric_limits<uint64_t>::max(), m_AcquireSemaphores[m_AcquireSemaphoreIndex], {}, &m_SwapChainIndex);
			if (result == vk::Result::eErrorOutOfDateKHR && attempt < maxAttempts)
			{
				DE_CORE_WARN("Swapchain is out of date, recreating it.");
				RecreateSwapChain();
				break;
			}
			else
			{
				break;
			}
		}

		if (result == vk::Result::eSuccess)
		{
			// Schedule the wait. The actual wait operation will be submitted when the app executes any command list.
			graphicsContext.m_NvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, m_AcquireSemaphores[m_AcquireSemaphoreIndex], 0);
		}
	}

	void VulkanSwapChain::Present()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		graphicsContext.m_NvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, m_AcquireSemaphores[m_AcquireSemaphoreIndex], 0);

		graphicsContext.m_NvrhiDevice->executeCommandLists(nullptr, 0);

		vk::SubmitInfo submitInfo = vk::SubmitInfo()
			.setPWaitDstStageMask(&waitStageMask)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&m_AcquireSemaphores[m_AcquireSemaphoreIndex])
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&m_PresentSemaphores[m_PresentSemaphoreIndex]);


		const vk::Result r = graphicsContext.m_GraphicsQueue.submit(1, &submitInfo, m_InFlightFences[m_PresentSemaphoreIndex]);
		DE_CORE_ASSERT(r == vk::Result::eSuccess);

		vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&m_PresentSemaphores[m_PresentSemaphoreIndex])
			.setSwapchainCount(1)
			.setPSwapchains(&m_SwapChain)
			.setPImageIndices(&m_SwapChainIndex);

		const vk::Result result = graphicsContext.m_PresentQueue.presentKHR(&presentInfo);
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			DE_CORE_WARN("Swapchain is out of date, recreating it.");
			RecreateSwapChain();
			//return;
		}
		DE_CORE_ASSERT(result == vk::Result::eSuccess);

		m_PresentSemaphoreIndex = (m_PresentSemaphoreIndex + 1) % m_PresentSemaphores.size();
		m_AcquireSemaphoreIndex = (m_AcquireSemaphoreIndex + 1) % m_AcquireSemaphores.size();

		if (true) // vsync 
		{
			graphicsContext.m_PresentQueue.waitIdle();
		}
	}

	Framebuffer* SwapChain::GetFramebuffer(uint32_t index) const
	{
		return m_SwapChainFramebuffers[index];
	}

	void VulkanSwapChain::CreateWindowSurface()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		const VkResult result = glfwCreateWindowSurface(graphicsContext.m_VulkanInstance, m_Options.NativeWindowHandle, nullptr, (VkSurfaceKHR*)&m_WindowSurface);
		if (result != VK_SUCCESS)
		{
			DE_CORE_ERROR("Failed to create a GLFW window surface, error code = {}", std::string(nvrhi::vulkan::resultToString(result)));
			DE_CORE_ASSERT(true);
		}
	}

	void VulkanSwapChain::CreateSwapChain()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		m_SwapChainFormat = {
			vk::Format(nvrhi::vulkan::convertFormat(nvrhi::Format::RGBA8_UNORM)),
			vk::ColorSpaceKHR::eSrgbNonlinear
		};

		vk::Extent2D extent = vk::Extent2D(m_Options.Width, m_Options.Height);

		std::unordered_set<uint32_t> uniqueQueues = {
			uint32_t(graphicsContext.m_QueueFamilyIndices.Graphics),
			uint32_t(graphicsContext.m_QueueFamilyIndices.Present)
		};

		std::vector<uint32_t> queues = Utils::setToVector(uniqueQueues);

		const bool enableSwapChainSharing = queues.size() > 1;

		const uint32_t swapChainBufferCount = 2; // 3
		const bool vsyncEnabled = false;
		const bool swapChainMutableFormatSupported = false;

		vk::SwapchainCreateInfoKHR swapChainCreateInfo = vk::SwapchainCreateInfoKHR()
			.setSurface(m_WindowSurface)
			.setMinImageCount(swapChainBufferCount)
			.setImageFormat(m_SwapChainFormat.format)
			.setImageColorSpace(m_SwapChainFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
			.setImageSharingMode(enableSwapChainSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
			.setFlags(swapChainMutableFormatSupported ? vk::SwapchainCreateFlagBitsKHR::eMutableFormat : vk::SwapchainCreateFlagBitsKHR(0))
			.setQueueFamilyIndexCount(enableSwapChainSharing ? uint32_t(queues.size()) : 0)
			.setPQueueFamilyIndices(enableSwapChainSharing ? queues.data() : nullptr)
			.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(vsyncEnabled ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate)
			.setClipped(true)
			.setOldSwapchain(nullptr);

		std::vector<vk::Format> imageFormats = { m_SwapChainFormat.format };
		switch (m_SwapChainFormat.format)
		{
			case vk::Format::eR8G8B8A8Unorm:
				imageFormats.push_back(vk::Format::eR8G8B8A8Srgb);
				break;
			case vk::Format::eR8G8B8A8Srgb:
				imageFormats.push_back(vk::Format::eR8G8B8A8Unorm);
				break;
			case vk::Format::eB8G8R8A8Unorm:
				imageFormats.push_back(vk::Format::eB8G8R8A8Srgb);
				break;
			case vk::Format::eB8G8R8A8Srgb:
				imageFormats.push_back(vk::Format::eB8G8R8A8Unorm);
				break;
		}

		vk::ImageFormatListCreateInfo imageFormatListCreateInfo = vk::ImageFormatListCreateInfo()
			.setViewFormats(imageFormats);

		if (swapChainMutableFormatSupported)
			swapChainCreateInfo.pNext = &imageFormatListCreateInfo;

		const vk::Result result = graphicsContext.m_VulkanDevice.createSwapchainKHR(&swapChainCreateInfo, nullptr, &m_SwapChain);
		DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create Vulkan swapchain.");

		std::vector<vk::Image> images = graphicsContext.m_VulkanDevice.getSwapchainImagesKHR(m_SwapChain);
		for (vk::Image image : images)
		{
			nvrhi::TextureDesc textureDesc;
			textureDesc.width = m_Options.Width;
			textureDesc.height = m_Options.Height;
			textureDesc.format = nvrhi::Format::RGBA8_UNORM; // deviceParams.swapChainFormat;
			textureDesc.debugName = "Swap chain image";
			textureDesc.initialState = nvrhi::ResourceStates::Present;
			textureDesc.keepInitialState = true;
			textureDesc.isRenderTarget = true;

			m_SwapChainImages.push_back(SwapChainImage{
				.image = image,
				.rhiHandle = GraphicsContext::GetDeviceHandle()->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(image), textureDesc)
			});
		}

		m_SwapChainIndex = 0;
	}

	void VulkanSwapChain::CreateFramebuffers()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();
		m_SwapChainFramebuffers.resize(m_SwapChainImages.size());
		for (size_t index = 0; index < m_SwapChainImages.size(); index++)
		{
			FramebufferParams framebufferParams = FramebufferParams()
				.SetWidth(m_Options.Width)
				.SetHeight(m_Options.Height)
				.SetTexture(m_SwapChainImages[index].rhiHandle);

			m_SwapChainFramebuffers[index] = Framebuffer::Create(framebufferParams);
			m_SwapChainFramebuffers[index]->Initialize();
		}
	}

	void VulkanSwapChain::CreateSynchronizationObjects()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		const uint32_t maxFramesInFlight = 2;

		m_PresentSemaphores.reserve(maxFramesInFlight + 1);
		m_AcquireSemaphores.reserve(maxFramesInFlight + 1);
		m_InFlightFences.resize(maxFramesInFlight + 1);
		for (uint32_t i = 0; i < maxFramesInFlight + 1; ++i)
		{
			m_PresentSemaphores.push_back(graphicsContext.m_VulkanDevice.createSemaphore(vk::SemaphoreCreateInfo()));
			m_AcquireSemaphores.push_back(graphicsContext.m_VulkanDevice.createSemaphore(vk::SemaphoreCreateInfo()));
			m_InFlightFences[i] = graphicsContext.m_VulkanDevice.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
		}

		m_AcquireSemaphoreIndex = 0;
		m_PresentSemaphoreIndex = 0;
	}

	void VulkanSwapChain::DestroySwapChain()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		if (graphicsContext.m_VulkanDevice)
		{
			graphicsContext.m_VulkanDevice.waitIdle();
		}

		for (size_t index = 0; index < m_SwapChainImages.size(); index++)
		{
			m_SwapChainFramebuffers[index]->Destroy();
		}
		m_SwapChainFramebuffers.clear();

		if (m_SwapChain)
		{
			graphicsContext.m_VulkanDevice.destroySwapchainKHR(m_SwapChain);
			m_SwapChain = nullptr;
		}

		m_SwapChainImages.clear();
	}

	void VulkanSwapChain::RecreateSwapChain()
	{
		DestroySwapChain();

		CreateSwapChain();
		CreateFramebuffers();
	}

}
