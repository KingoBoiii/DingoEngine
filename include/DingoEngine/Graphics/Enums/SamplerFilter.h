#pragma once

#include <cstdint>

namespace Dingo
{

	/// How a sampler blends texels (SamplerParams::MinFilter, MagFilter and MipFilter).
	enum class SamplerFilter : uint8_t
	{
		Nearest, ///< The closest texel; blocky when magnified.
		Linear,  ///< Interpolates between neighbouring texels.
	};

}
