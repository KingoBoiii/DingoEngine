#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Dingo::Internal
{

	inline float HalfToFloat(uint16_t half)
	{
		const uint32_t sign = (half >> 15) & 1u;
		const uint32_t exponent = (half >> 10) & 0x1Fu;
		const uint32_t mantissa = half & 0x3FFu;
		float value;
		if (exponent == 0)
			value = std::ldexp(static_cast<float>(mantissa), -24);
		else if (exponent == 31)
			value = mantissa ? NAN : INFINITY;
		else
			value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
		return sign ? -value : value;
	}

	// Round to nearest even. NaN and negative values (and -0) become 0; anything from 65504 up,
	// +infinity included, becomes 65504, so a half never holds an infinity or a NaN.
	inline uint16_t FloatToHalf(float value)
	{
		if (!(value > 0.0f))
			return 0;
		if (value >= 65504.0f)
			return 0x7BFFu;

		uint32_t bits;
		std::memcpy(&bits, &value, sizeof(bits));
		const int exponent = static_cast<int>((bits >> 23) & 0xFFu) - 127;
		const uint32_t mantissa = (bits & 0x7FFFFFu) | 0x800000u;

		if (exponent >= -14)
		{
			uint32_t half = (static_cast<uint32_t>(exponent + 15) << 10) | ((mantissa >> 13) & 0x3FFu);
			const uint32_t remainder = mantissa & 0x1FFFu;
			if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1u)))
				++half;
			return static_cast<uint16_t>(half);
		}

		const int shift = -exponent - 1;
		if (shift >= 32)
			return 0;
		uint32_t half = mantissa >> shift;
		const uint32_t remainder = mantissa & ((1u << shift) - 1u);
		const uint32_t halfway = 1u << (shift - 1);
		if (remainder > halfway || (remainder == halfway && (half & 1u)))
			++half;
		return static_cast<uint16_t>(half);
	}

}
