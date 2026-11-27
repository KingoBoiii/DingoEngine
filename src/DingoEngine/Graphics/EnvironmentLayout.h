#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

namespace Dingo::Internal
{

	constexpr uint32_t k_EnvironmentAutoFaceMin = 32;
	constexpr uint32_t k_EnvironmentAutoFaceMax = 1024;
	constexpr uint32_t k_EnvironmentFaceMin = 16;
	constexpr uint32_t k_EnvironmentFaceMax = 2048;

	// The cube face size for an equirect `equirectWidth` wide: requested 0 picks the largest power of
	// two not above a quarter of the width, within [32, 1024]; an explicit size must be a power of two
	// in [16, 2048]. Returns 0 for an explicit size that isn't, or a zero width.
	constexpr uint32_t ChooseEnvironmentFaceSize(uint32_t equirectWidth, uint32_t requested)
	{
		if (requested != 0)
			return std::has_single_bit(requested) && requested >= k_EnvironmentFaceMin && requested <= k_EnvironmentFaceMax ? requested : 0;
		if (equirectWidth == 0)
			return 0;
		const uint32_t quarter = (std::max)(equirectWidth / 4, 1u);
		return (std::clamp)(std::bit_floor(quarter), k_EnvironmentAutoFaceMin, k_EnvironmentAutoFaceMax);
	}

}
