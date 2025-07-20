#include "depch.h"
#include "DingoEngine/Graphics/SwapChain.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h"

namespace Dingo
{

	SwapChain* SwapChain::Create(const SwapChainParams& params)
	{
		switch (GraphicsContext::Get().GetParams().GraphicsAPI)
		{
			case GraphicsAPI::Vulkan: return new VulkanSwapChain(params);
			default: break;
		}

		return nullptr;
	}

	SwapChain::SwapChain(const SwapChainParams& params)
		: m_Params(params)
	{}

}
