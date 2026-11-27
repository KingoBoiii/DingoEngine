#pragma once

#include "DingoEngine/Graphics/TextureLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Dingo::Internal
{

	enum class MipFilter
	{
		Color,  // plain box average of all four channels
		Srgb,   // rgb is sRGB-encoded colour: decoded, averaged linearly and encoded again; alpha averaged as is
		Normal  // rgb is a tangent-space normal: averaged as a vector and renormalised
	};

	inline float DecodeSrgb8(uint8_t value)
	{
		static const std::array<float, 256> table = []
		{
			std::array<float, 256> result{};
			for (size_t i = 0; i < result.size(); ++i)
			{
				const float c = static_cast<float>(i) / 255.0f;
				result[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
			}
			return result;
		}();
		return table[value];
	}

	inline uint8_t EncodeSrgb8(float linear)
	{
		linear = std::clamp(linear, 0.0f, 1.0f);
		const float c = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
		return static_cast<uint8_t>(std::clamp(std::lround(c * 255.0f), 0L, 255L));
	}

	// The next mip of a tightly packed RGBA8 image: each destination texel averages the source texels
	// [x * w / dw, (x + 1) * w / dw), so an odd side loses no texel and a power of two is the 2 x 2 box.
	inline std::vector<uint8_t> DownsampleRgba8(const uint8_t* source, uint32_t width, uint32_t height, MipFilter filter)
	{
		const TextureExtent extent = GetMipExtent(width, height, 1, 1);
		std::vector<uint8_t> result(static_cast<size_t>(extent.Width) * extent.Height * 4);

		for (uint32_t y = 0; y < extent.Height; ++y)
		{
			const uint32_t y0 = y * height / extent.Height;
			const uint32_t y1 = (std::max)(y0 + 1, (y + 1) * height / extent.Height);
			for (uint32_t x = 0; x < extent.Width; ++x)
			{
				const uint32_t x0 = x * width / extent.Width;
				const uint32_t x1 = (std::max)(x0 + 1, (x + 1) * width / extent.Width);
				const uint32_t count = (x1 - x0) * (y1 - y0);

				uint32_t sum[4] = {};
				float accum[3] = {};
				for (uint32_t sy = y0; sy < y1; ++sy)
				{
					for (uint32_t sx = x0; sx < x1; ++sx)
					{
						const uint8_t* texel = source + (static_cast<size_t>(sy) * width + sx) * 4;
						for (uint32_t c = 0; c < 4; ++c)
							sum[c] += texel[c];
						if (filter == MipFilter::Srgb)
						{
							for (uint32_t c = 0; c < 3; ++c)
								accum[c] += DecodeSrgb8(texel[c]);
						}
						else if (filter == MipFilter::Normal)
						{
							for (uint32_t c = 0; c < 3; ++c)
								accum[c] += texel[c] / 255.0f * 2.0f - 1.0f;
						}
					}
				}

				uint8_t* out = result.data() + (static_cast<size_t>(y) * extent.Width + x) * 4;
				out[3] = static_cast<uint8_t>((sum[3] + count / 2) / count);
				if (filter == MipFilter::Color)
				{
					for (uint32_t c = 0; c < 3; ++c)
						out[c] = static_cast<uint8_t>((sum[c] + count / 2) / count);
					continue;
				}
				if (filter == MipFilter::Srgb)
				{
					for (uint32_t c = 0; c < 3; ++c)
						out[c] = EncodeSrgb8(accum[c] / static_cast<float>(count));
					continue;
				}

				float (&normal)[3] = accum;
				const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
				if (length < 1e-6f)
				{
					normal[0] = 0.0f;
					normal[1] = 0.0f;
					normal[2] = 1.0f;
				}
				else
				{
					for (float& component : normal)
						component /= length;
				}
				for (uint32_t c = 0; c < 3; ++c)
					out[c] = static_cast<uint8_t>(std::clamp(std::lround((normal[c] * 0.5f + 0.5f) * 255.0f), 0L, 255L));
			}
		}
		return result;
	}

	// Mips 1 to the last of an RGBA8 image, in order; mip 0 is the source itself.
	inline std::vector<std::vector<uint8_t>> BuildMipChain(const uint8_t* rgba, uint32_t width, uint32_t height, MipFilter filter)
	{
		std::vector<std::vector<uint8_t>> mips;
		const uint8_t* previous = rgba;
		uint32_t previousWidth = width;
		uint32_t previousHeight = height;
		while (previousWidth > 1 || previousHeight > 1)
		{
			mips.push_back(DownsampleRgba8(previous, previousWidth, previousHeight, filter));
			const TextureExtent extent = GetMipExtent(previousWidth, previousHeight, 1, 1);
			previousWidth = extent.Width;
			previousHeight = extent.Height;
			previous = mips.back().data();
		}
		return mips;
	}

}
