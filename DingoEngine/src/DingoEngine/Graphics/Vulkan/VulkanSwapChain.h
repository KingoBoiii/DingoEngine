#pragma once
#include "DingoEngine/Graphics/SwapChain.h"

#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

namespace vk
{
	class SurfaceKHR;
}

namespace DingoEngine
{

	class VulkanSwapChain : public SwapChain
	{
	public:
		VulkanSwapChain(const SwapChainOptions& options);
		virtual ~VulkanSwapChain();

	public:
		virtual void Initialize() override;

	//private:
		//void CreateWindowSurface();

	private:
		vk::SurfaceKHR* m_WindowSurface = nullptr;
		vk::SurfaceFormatKHR m_SwapChainFormat;
	};

}
