#pragma once

#define BIT(x) (1u << x)

#define DE_EXPAND_MACRO(x) x
#define DE_STRINGIFY_MACRO(x) #x

#define DE_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

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
