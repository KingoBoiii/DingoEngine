#include "depch.h"
#include "DingoEngine/Graphics/SwapChain.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "Vulkan/VulkanSwapChain.h"

namespace DingoEngine
{

	SwapChain* SwapChain::Create(const SwapChainOptions& options)
	{
		switch (GraphicsContext::GetApi())
		{
			case DingoEngine::GraphicsAPI::Vulkan: return new VulkanSwapChain(options);
			default: break;
		}

		return nullptr;
	}

	SwapChain::SwapChain(const SwapChainOptions& options)
		: m_Options(options)
	{}

}
