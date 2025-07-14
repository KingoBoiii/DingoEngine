#pragma once
#include "DingoEngine/Graphics/SwapChain.h"

#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace Dingo
{

	class VulkanSwapChain : public SwapChain
	{
	public:
		VulkanSwapChain(const SwapChainParams& options);
		virtual ~VulkanSwapChain();

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Resize(int32_t width, int32_t height) override;

		virtual void AcquireNextImage() override;
		virtual void Present() override;

		virtual Framebuffer* SwapChain::GetCurrentFramebuffer() const
		{
			return GetFramebuffer(m_SwapChainIndex);
		}

		virtual uint32_t GetCurrentBackBufferIndex() const override
		{
			return m_SwapChainIndex;
		}

	private:

		void CreateWindowSurface();
		void CreateSwapChain();
		void CreateFramebuffers();
		void CreateSynchronizationObjects();
		void DestroySwapChain();

		void RecreateSwapChain();
	private:
		vk::SurfaceKHR m_WindowSurface = nullptr;
		vk::SwapchainKHR m_SwapChain = nullptr;
		vk::SurfaceFormatKHR m_SwapChainFormat;

		std::vector<vk::Semaphore> m_AcquireSemaphores;
		std::vector<vk::Semaphore> m_PresentSemaphores;
		uint32_t m_AcquireSemaphoreIndex = 0;
		uint32_t m_PresentSemaphoreIndex = 0;

		std::vector<vk::Fence> m_InFlightFences;

		struct SwapChainImage
		{
			vk::Image image;
			nvrhi::TextureHandle rhiHandle;
		};

		uint32_t m_SwapChainIndex = -1;
		std::vector<SwapChainImage> m_SwapChainImages;
	};

}
