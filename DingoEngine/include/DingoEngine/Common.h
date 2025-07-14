#pragma once

#define BIT(x) (1u << x)

#define DE_EXPAND_MACRO(x) x
#define DE_STRINGIFY_MACRO(x) #x

namespace Dingo
{

	enum class GraphicsAPI
	{
		Headless,
		Vulkan,
		DirectX11,
		DirectX12
	};

}
