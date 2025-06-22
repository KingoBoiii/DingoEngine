#pragma once
#include "DingoEngine/Graphics/SwapChain.h"

#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace DingoEngine
{

	class VulkanSwapChain : public SwapChain
	{
	public:
		VulkanSwapChain(const SwapChainOptions& options);
		virtual ~VulkanSwapChain();

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void BeginFrame() override;
		virtual void Present() override;

	private:
		void CreateWindowSurface();
		void CreateSwapChain();

	private:
		vk::SurfaceKHR m_WindowSurface = nullptr;
		vk::SwapchainKHR m_SwapChain = nullptr;
		vk::SurfaceFormatKHR m_SwapChainFormat;

		std::vector<vk::Semaphore> m_AcquireSemaphores;
		std::vector<vk::Semaphore> m_PresentSemaphores;
		uint32_t m_AcquireSemaphoreIndex = 0;
		uint32_t m_PresentSemaphoreIndex = 0;

		struct SwapChainImage
		{
			vk::Image image;
			nvrhi::TextureHandle rhiHandle;
		};

		uint32_t m_SwapChainIndex = -1;
		std::vector<SwapChainImage> m_SwapChainImages;
	};

}
