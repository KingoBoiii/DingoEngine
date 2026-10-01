#pragma once
#include "GameTuning.h"

#include <cmath>

namespace Dingo
{

	inline float FlameFlicker(float clock, float phase, float depth)
	{
		return 1.0f + depth * 0.5f * (std::sin(clock * FLICKER_RATE_A + phase) + std::sin(clock * FLICKER_RATE_B + phase * FLICKER_RATE_B_PHASE));
	}

}
