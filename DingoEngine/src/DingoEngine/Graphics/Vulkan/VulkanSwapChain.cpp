#include "depch.h"
#include "VulkanSwapChain.h"

#include <glfw/glfw3.h>
#include "VulkanGraphicsContext.h"

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
#if 0
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		m_SwapChainFormat = {
			vk::Format(nvrhi::vulkan::convertFormat(nvrhi::Format::RGBA8_UNORM)),
			vk::ColorSpaceKHR::eSrgbNonlinear
		};

		vk::Extent2D extent = vk::Extent2D(m_Options.Width, m_Options.Height);

		const bool vsyncEnabled = false;
		const bool m_SwapChainMutableFormatSupported = false;

		auto indices = graphicsContext.m_PhysicalDevice->GetQueueFamilyIndices();

		std::unordered_set<uint32_t> uniqueQueues = {
			uint32_t(indices.GraphicsFamilyIndex.value()) // ,
			//uint32_t(m_PresentQueueFamily) 
		};

		std::vector<uint32_t> queues = Utils::setToVector(uniqueQueues);

		const bool enableSwapChainSharing = queues.size() > 1;

		vk::SwapchainCreateInfoKHR swapChainCreateInfoKhr = vk::SwapchainCreateInfoKHR()
			.setSurface(*graphicsContext.m_WindowSurface)
			.setMinImageCount(3)
			.setImageFormat(m_SwapChainFormat.format)
			.setImageColorSpace(m_SwapChainFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
			.setImageSharingMode(enableSwapChainSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
			.setFlags(m_SwapChainMutableFormatSupported ? vk::SwapchainCreateFlagBitsKHR::eMutableFormat : vk::SwapchainCreateFlagBitsKHR(0))
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
			default:
				break;
		}

		auto imageFormatListCreateInfo = vk::ImageFormatListCreateInfo()
			.setViewFormats(imageFormats);

		if (m_SwapChainMutableFormatSupported)
		{
			swapChainCreateInfoKhr.pNext = &imageFormatListCreateInfo;
		}
#endif
	}

}
