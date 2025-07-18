#pragma once
#include "DingoEngine/Common.h"

#include <cstdint>

namespace Dingo
{

	struct GraphicsParams
	{
		GraphicsAPI GraphicsAPI;
		uint16_t FramesInFlight;
	};

}
