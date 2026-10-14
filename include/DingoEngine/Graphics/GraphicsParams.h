#pragma once
#include "DingoEngine/Common.h"

#include <cstdint>
#include <optional>

namespace Dingo
{

	/// How the graphics device is created; part of ApplicationParams.
	struct GraphicsParams
	{
		/// Backend to run on; fixed for the run.
		Dingo::GraphicsAPI GraphicsAPI = Dingo::GraphicsAPI::Vulkan;

		/// Loads the Vulkan validation layer when it is installed. Empty = Debug builds only: it
		/// cost Release about 0.6 ms of render CPU a frame. `--vulkan-validation[=off]` sets it
		/// too.
		std::optional<bool> VulkanValidation;
	};

}
