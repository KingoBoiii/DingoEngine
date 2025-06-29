#include "depch.h"
#include "DingoEngine/Graphics/SwapChain.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "Vulkan/VulkanSwapChain.h"

namespace DingoEngine
{

	SwapChain* SwapChain::Create(const SwapChainParams& params)
	{
		switch (GraphicsContext::GetApi())
		{
			case DingoEngine::GraphicsAPI::Vulkan: return new VulkanSwapChain(params);
			default: break;
		}

		return nullptr;
	}

	SwapChain::SwapChain(const SwapChainParams& params)
		: m_Params(params)
	{}

}
