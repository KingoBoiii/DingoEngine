#pragma once

#define BIT(x) (1u << x)

#define DE_EXPAND_MACRO(x) x
#define DE_STRINGIFY_MACRO(x) #x

// For code that must still read a [[deprecated]] member while it lives (the engine itself, a
// migration shim).
#if defined(_MSC_VER)
	#define DE_SUPPRESS_DEPRECATED_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
	#define DE_SUPPRESS_DEPRECATED_END __pragma(warning(pop))
#else
	#define DE_SUPPRESS_DEPRECATED_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
	#define DE_SUPPRESS_DEPRECATED_END _Pragma("GCC diagnostic pop")
#endif

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

#include "DingoEngine/Log.h"
#include "DingoEngine/Assertion.h"