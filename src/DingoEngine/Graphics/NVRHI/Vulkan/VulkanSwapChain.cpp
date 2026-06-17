#include "depch.h"
#include "VulkanSwapChain.h"

#include <glfw/glfw3.h>
#include "VulkanGraphicsContext.h"

#include <algorithm>

#undef max

namespace Dingo
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

		// Pick a surface format the surface actually reports. Prefer RGBA8_UNORM so pipelines
		// built against the swap-chain framebuffer keep their render-target format on hardware
		// that supports it; otherwise accept BGRA8 (the common Windows surface format) and let
		// it propagate to the swap-chain textures so framebuffers/pipelines stay consistent.
		static vk::SurfaceFormatKHR ChooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available)
		{
			// Only formats VkToNvrhiFormat can map, so the swap-chain images, framebuffer color
			// attachment and any pipelines built against them stay in agreement. Order = preference;
			// RGBA8_UNORM first so pipelines keep their render-target format on hardware that has it.
			const vk::Format mappable[] = {
				vk::Format::eR8G8B8A8Unorm,
				vk::Format::eB8G8R8A8Unorm,
				vk::Format::eR8G8B8A8Srgb,
				vk::Format::eB8G8R8A8Srgb,
			};

			for (vk::Format want : mappable)
			{
				for (const vk::SurfaceFormatKHR& format : available)
				{
					if (format.format == want && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
					{
						return format;
					}
				}
			}

			// Nothing we can map cleanly; fall back to the surface's first format (VkToNvrhiFormat
			// will log if it cannot map it). Not expected on desktop GPUs, which always expose an
			// 8-bit BGRA/RGBA sRGB-nonlinear format.
			DE_CORE_ASSERT(!available.empty(), "Surface reports no supported formats.");
			DE_CORE_WARN("Surface does not expose a preferred 8-bit UNORM/SRGB format; falling back to the first reported format.");
			return available.front();
		}

		// FIFO is the only present mode guaranteed to exist; anything else must be confirmed
		// against the surface (eImmediate in particular is optional and absent on some drivers).
		static vk::PresentModeKHR ChoosePresentMode(const std::vector<vk::PresentModeKHR>& available, bool vsync)
		{
			if (vsync)
			{
				return vk::PresentModeKHR::eFifo;
			}

			const auto supports = [&](vk::PresentModeKHR mode)
			{
				return std::find(available.begin(), available.end(), mode) != available.end();
			};

			if (supports(vk::PresentModeKHR::eMailbox))
			{
				return vk::PresentModeKHR::eMailbox;
			}
			if (supports(vk::PresentModeKHR::eImmediate))
			{
				return vk::PresentModeKHR::eImmediate;
			}

			return vk::PresentModeKHR::eFifo;
		}

		static nvrhi::Format VkToNvrhiFormat(vk::Format format)
		{
			switch (format)
			{
				case vk::Format::eR8G8B8A8Unorm: return nvrhi::Format::RGBA8_UNORM;
				case vk::Format::eR8G8B8A8Srgb:  return nvrhi::Format::SRGBA8_UNORM;
				case vk::Format::eB8G8R8A8Unorm: return nvrhi::Format::BGRA8_UNORM;
				case vk::Format::eB8G8R8A8Srgb:  return nvrhi::Format::SBGRA8_UNORM;
				default:
					DE_CORE_ERROR("Unsupported swap-chain surface format ({}); defaulting to RGBA8_UNORM.", uint32_t(format));
					return nvrhi::Format::RGBA8_UNORM;
			}
		}

	}

	VulkanSwapChain::VulkanSwapChain(const SwapChainParams& params)
		: SwapChain(params)
	{}

	VulkanSwapChain::~VulkanSwapChain()
	{}

	void VulkanSwapChain::Initialize()
	{
		if (m_Params.VulkanSurface == nullptr)
		{
			CreateWindowSurface();
		}
		else
		{
			m_WindowSurface = static_cast<vk::SurfaceKHR>(*m_Params.VulkanSurface);
			DE_CORE_ASSERT(m_WindowSurface, "Vulkan surface must be valid.");
		}

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
		if (width == 0 || height == 0)
		{
			DE_CORE_WARN("Swap chain resize called with zero dimensions, ignoring.");
			return;
		}

		m_Params.Width = width;
		m_Params.Height = height;

		RecreateSwapChain();
	}

	void VulkanSwapChain::AcquireNextImage()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		// Wait for the previous use of this frame slot to finish. The matching fence reset now
		// lives in Present(), right before the submit that re-signals it -- so an AcquireNextImage()
		// that is not followed by a Present() (e.g. a minimized ImGui viewport) leaves the fence
		// signaled and cannot deadlock the next waitForFences.
		{
			const vk::Result result = graphicsContext.m_VulkanDevice.waitForFences(1, &m_InFlightFences[m_AcquireSemaphoreIndex], VK_TRUE, UINT64_MAX);
			DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to wait for fence.");
		}

		m_ImageAcquired = false;

		// Acquire the next image. If the swap chain is out of date (e.g. the window moved to a
		// display with a different size/DPI), recreate it and retry so we return a valid image
		// index rather than the stale one the old code left behind after a bare `break`.
		vk::Result result = vk::Result::eErrorOutOfDateKHR;
		const int32_t maxAttempts = 3;
		for (int32_t attempt = 0; attempt < maxAttempts; ++attempt)
		{
			result = graphicsContext.m_VulkanDevice.acquireNextImageKHR(m_SwapChain, std::numeric_limits<uint64_t>::max(), m_AcquireSemaphores[m_AcquireSemaphoreIndex], {}, &m_SwapChainIndex);

			if (result == vk::Result::eErrorOutOfDateKHR)
			{
				DE_CORE_WARN("Swapchain is out of date on acquire, recreating it.");
				RecreateSwapChain();
				continue; // retry against the freshly created swap chain
			}

			break; // eSuccess or eSuboptimalKHR: an image was acquired
		}

		// eSuboptimalKHR still yields a usable image (it differs from the surface only in
		// presentation quality), so render this frame and let Present() recreate afterwards.
		if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR)
		{
			m_ImageAcquired = true;
			// Schedule the wait. The actual wait operation will be submitted when the app executes any command list.
			graphicsContext.m_NvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, m_AcquireSemaphores[m_AcquireSemaphoreIndex], 0);
		}
		else
		{
			// No image was acquired (e.g. minimized window, or a swapchain on a display this GPU
			// cannot present to). Leave m_ImageAcquired false so Present() skips this frame instead
			// of submitting/presenting a stale image index.
			DE_CORE_ERROR("Failed to acquire swapchain image, error code = {}", std::string(nvrhi::vulkan::resultToString(VkResult(result))));
		}
	}

	void VulkanSwapChain::Present()
	{
		// AcquireNextImage() did not hand us a valid image (minimized window, or a display this
		// GPU cannot present to). Skip the frame rather than presenting a stale index; the fence
		// for this slot is left untouched (still signaled), so the next acquire cannot deadlock.
		if (!m_ImageAcquired)
		{
			return;
		}

		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		const vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		graphicsContext.m_NvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, m_AcquireSemaphores[m_AcquireSemaphoreIndex], 0);

		graphicsContext.m_NvrhiDevice->executeCommandLists(nullptr, 0);

		{
			vk::SubmitInfo submitInfo = vk::SubmitInfo()
				.setPWaitDstStageMask(&waitStageMask)
				.setWaitSemaphoreCount(1)
				.setPWaitSemaphores(&m_AcquireSemaphores[m_AcquireSemaphoreIndex])
				.setSignalSemaphoreCount(1)
				.setPSignalSemaphores(&m_PresentSemaphores[m_PresentSemaphoreIndex]);

			// Reset the fence immediately before the submit that signals it (paired with the
			// waitForFences in AcquireNextImage), so the fence is only ever cleared when this
			// submit is guaranteed to re-signal it.
			const vk::Result resetResult = graphicsContext.m_VulkanDevice.resetFences(1, &m_InFlightFences[m_PresentSemaphoreIndex]);
			DE_CORE_ASSERT(resetResult == vk::Result::eSuccess, "Failed to reset fence.");

			const vk::Result result = graphicsContext.m_GraphicsQueue.submit(1, &submitInfo, m_InFlightFences[m_PresentSemaphoreIndex]);
			DE_CORE_ASSERT(result == vk::Result::eSuccess);
		}

		{
			vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
				.setWaitSemaphoreCount(1)
				.setPWaitSemaphores(&m_PresentSemaphores[m_PresentSemaphoreIndex])
				.setSwapchainCount(1)
				.setPSwapchains(&m_SwapChain)
				.setPImageIndices(&m_SwapChainIndex);

			const vk::Result result = graphicsContext.m_PresentQueue.presentKHR(&presentInfo);
			if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
			{
				// The surface changed (resized, moved to another monitor, DPI change, ...).
				// These results are expected here, so recreate instead of asserting.
				DE_CORE_WARN("Swapchain is out of date or suboptimal on present, recreating it.");
				RecreateSwapChain();
			}
			else
			{
				DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to present swapchain image.");
			}
		}

		m_PresentSemaphoreIndex = (m_PresentSemaphoreIndex + 1) % m_PresentSemaphores.size();
		m_AcquireSemaphoreIndex = (m_AcquireSemaphoreIndex + 1) % m_AcquireSemaphores.size();

		if (m_Params.VSync) // vsync 
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

		const VkResult result = glfwCreateWindowSurface(graphicsContext.m_VulkanInstance, m_Params.NativeWindowHandle, nullptr, (VkSurfaceKHR*)&m_WindowSurface);
		if (result != VK_SUCCESS)
		{
			DE_CORE_ERROR("Failed to create a GLFW window surface, error code = {}", std::string(nvrhi::vulkan::resultToString(result)));
			DE_CORE_ASSERT(false, "Failed to create a GLFW window surface.");
		}
	}

	void VulkanSwapChain::CreateSwapChain()
	{
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		const vk::PhysicalDevice physicalDevice = graphicsContext.m_VulkanPhysicalDevice;

		// A Vulkan surface is bound to one window, and its capabilities differ per display and
		// per GPU. Query them on every (re)creation instead of assuming fixed values -- this is
		// what makes the swap chain correct when the window moves to another monitor (different
		// resolution/DPI), a display with a different color space, or a different GPU.
		const vk::SurfaceCapabilitiesKHR surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(m_WindowSurface);
		const std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(m_WindowSurface);
		const std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(m_WindowSurface);

		// The window may live on a display driven by a GPU other than the one we picked
		// (hybrid/multi-GPU laptops). Presenting requires the chosen device to support THIS
		// surface; if it does not, swap-chain creation/presentation will fail on that display.
		// NOTE: fully supporting that case needs surface-aware physical-device selection, which
		// happens before any window exists today (see VulkanGraphicsContext::PickPhysicalDevice).
		const vk::Bool32 presentSupported = physicalDevice.getSurfaceSupportKHR(uint32_t(graphicsContext.m_QueueFamilyIndices.Present), m_WindowSurface);
		if (!presentSupported)
		{
			DE_CORE_ERROR("GPU '{}' cannot present to this window's surface -- the window is likely on a display "
				"driven by a different GPU. Surface-aware device selection is required to render on that display.", graphicsContext.m_RendererString);
		}

		m_SwapChainFormat = Utils::ChooseSurfaceFormat(availableFormats);
		m_SwapChainTextureFormat = Utils::VkToNvrhiFormat(m_SwapChainFormat.format);

		const vk::PresentModeKHR presentMode = Utils::ChoosePresentMode(availablePresentModes, m_Params.VSync);

		// currentExtent is the surface's real size in pixels. On Windows it is always defined
		// and authoritative, so using it (rather than the window size in screen coordinates)
		// is what makes rendering correct on monitors with per-monitor DPI scaling. Only when
		// it is the "deferred" sentinel (0xFFFFFFFF) do we fall back to the requested size.
		vk::Extent2D extent;
		if (surfaceCaps.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			extent = surfaceCaps.currentExtent;
		}
		else
		{
			extent.width = std::clamp(uint32_t(m_Params.Width), surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width);
			extent.height = std::clamp(uint32_t(m_Params.Height), surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height);
		}
		// A zero extent (e.g. a minimized window) is invalid for swap-chain creation.
		extent.width = std::max(extent.width, 1u);
		extent.height = std::max(extent.height, 1u);
		m_SwapChainExtent = extent;

		// Respect the surface's supported image-count range.
		uint32_t imageCount = std::max(2u, surfaceCaps.minImageCount);
		if (surfaceCaps.maxImageCount > 0)
		{
			imageCount = std::min(imageCount, surfaceCaps.maxImageCount);
		}

		const vk::CompositeAlphaFlagBitsKHR compositeAlpha =
			(surfaceCaps.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)
			? vk::CompositeAlphaFlagBitsKHR::eOpaque
			: vk::CompositeAlphaFlagBitsKHR::eInherit;
		const vk::SurfaceTransformFlagBitsKHR preTransform =
			(surfaceCaps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
			? vk::SurfaceTransformFlagBitsKHR::eIdentity
			: surfaceCaps.currentTransform;

		std::unordered_set<uint32_t> uniqueQueues = {
			uint32_t(graphicsContext.m_QueueFamilyIndices.Graphics),
			uint32_t(graphicsContext.m_QueueFamilyIndices.Present)
		};

		std::vector<uint32_t> queues = Utils::setToVector(uniqueQueues);

		const bool enableSwapChainSharing = queues.size() > 1;

		vk::SwapchainCreateInfoKHR swapChainCreateInfo = vk::SwapchainCreateInfoKHR()
			.setSurface(m_WindowSurface)
			.setMinImageCount(imageCount)
			.setImageFormat(m_SwapChainFormat.format)
			.setImageColorSpace(m_SwapChainFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
			.setImageSharingMode(enableSwapChainSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
			.setQueueFamilyIndexCount(enableSwapChainSharing ? uint32_t(queues.size()) : 0)
			.setPQueueFamilyIndices(enableSwapChainSharing ? queues.data() : nullptr)
			.setPreTransform(preTransform)
			.setCompositeAlpha(compositeAlpha)
			.setPresentMode(presentMode)
			.setClipped(true)
			.setOldSwapchain(nullptr);

		const vk::Result result = graphicsContext.m_VulkanDevice.createSwapchainKHR(&swapChainCreateInfo, nullptr, &m_SwapChain);
		DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create Vulkan swapchain.");

		std::vector<vk::Image> images = graphicsContext.m_VulkanDevice.getSwapchainImagesKHR(m_SwapChain);
		for (vk::Image image : images)
		{
			nvrhi::TextureDesc textureDesc;
			textureDesc.width = extent.width;
			textureDesc.height = extent.height;
			textureDesc.format = m_SwapChainTextureFormat;
			textureDesc.debugName = "Swap chain image";
			textureDesc.initialState = nvrhi::ResourceStates::Present;
			textureDesc.keepInitialState = true;
			textureDesc.isRenderTarget = true;

			m_SwapChainImages.push_back(SwapChainImage{
				.image = image,
				.rhiHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(image), textureDesc)
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
				.SetWidth(int32_t(m_SwapChainExtent.width))
				.SetHeight(int32_t(m_SwapChainExtent.height))
				.SetEnableDepth(true);

			m_SwapChainFramebuffers[index] = VulkanFramebuffer::Create(m_SwapChainImages[index].rhiHandle, framebufferParams);
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
		VulkanGraphicsContext& graphicsContext = (VulkanGraphicsContext&)GraphicsContext::Get();

		// A minimized window reports a (0,0) surface extent, which is invalid for a swap chain.
		// Keep the existing swap chain (and its framebuffers) instead of destroying it into an
		// empty state that GetCurrentFramebuffer() would index out of bounds. Present() is skipped
		// while no image can be acquired; a later resize event recreates at a real size.
		const vk::SurfaceCapabilitiesKHR surfaceCaps = graphicsContext.m_VulkanPhysicalDevice.getSurfaceCapabilitiesKHR(m_WindowSurface);
		if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0)
		{
			m_ImageAcquired = false;
			return;
		}

		DestroySwapChain();

		CreateSwapChain();
		CreateFramebuffers();
	}

}
