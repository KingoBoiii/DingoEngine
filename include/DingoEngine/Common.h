#pragma once

/// A 32-bit unsigned value with only bit x set, for flag enums; x must be below 32.
#define DE_BIT(x) (1u << (x))

// For code that must still read a [[deprecated]] member while it lives (the engine itself, a
// migration shim).
#if defined(_MSC_VER)
	/// Starts a region with deprecation warnings off; close it with DE_SUPPRESS_DEPRECATED_END.
	#define DE_SUPPRESS_DEPRECATED_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
	/// Ends the region DE_SUPPRESS_DEPRECATED_BEGIN opened and restores the warning state.
	#define DE_SUPPRESS_DEPRECATED_END __pragma(warning(pop))
#else
	/// Starts a region with deprecation warnings off; close it with DE_SUPPRESS_DEPRECATED_END.
	#define DE_SUPPRESS_DEPRECATED_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
	/// Ends the region DE_SUPPRESS_DEPRECATED_BEGIN opened and restores the warning state.
	#define DE_SUPPRESS_DEPRECATED_END _Pragma("GCC diagnostic pop")
#endif

/// Wraps the member function fn of the current object as a lambda that forwards its arguments and
/// returns its result, e.g. dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(OnResize)).
/// Only valid inside a member function; the lambda captures this.
#define DE_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Dingo
{

	/// The graphics back-ends the engine can run on; the DirectX ones exist on Windows only.
	/// Store a back-end by name, not by its integer value.
	enum class GraphicsAPI
	{
		Vulkan,		///< Vulkan; the default, on Windows and Linux.
		DirectX11,	///< Direct3D 11, Windows only.
		DirectX12,	///< Direct3D 12, Windows only.
		/// No window, GPU or renderer: Run() ticks the layers at 60 Hz, for a dedicated server or
		/// a CI job. Last, so a value-initialised GraphicsAPI{} stays Vulkan.
		Headless
	};

}

#include "DingoEngine/Log.h"
#include "DingoEngine/Assertion.h"