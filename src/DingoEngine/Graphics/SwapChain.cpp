#include "depch.h"
#include "DingoEngine/Graphics/SwapChain.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/Vulkan/VulkanSwapChain.h"

#ifdef DE_PLATFORM_WINDOWS
#include "DingoEngine/Graphics/NVRHI/DirectX11/DirectX11SwapChain.h"
#include "DingoEngine/Graphics/NVRHI/DirectX12/DirectX12SwapChain.h"
#endif

namespace Dingo
{

	SwapChain* SwapChain::Create(const SwapChainParams& params)
	{
		switch (GraphicsContext::Get().GetParams().GraphicsAPI)
		{
			case GraphicsAPI::Vulkan: return new VulkanSwapChain(params);
#ifdef DE_PLATFORM_WINDOWS
			case GraphicsAPI::DirectX11: return new DirectX11SwapChain(params);
			case GraphicsAPI::DirectX12: return new DirectX12SwapChain(params);
#endif
			default: break;
		}

		DE_CORE_ASSERT(false, "Unsupported or unavailable GraphicsAPI on this platform.");
		return nullptr;
	}

	SwapChain::SwapChain(const SwapChainParams& params)
		: m_Params(params)
	{}

}
