#pragma once

#include <cstdint>

namespace Dingo
{

	enum class TextureFormat
	{
		Unknown,

		RGB,
		RGBA,

		RGBA8_UNORM,

		RGBA32F,

		// Render-target formats (v0.9): HDR colour, single channels and a depth that can be sampled.
		RGBA16F,
		R11G11B10F, // unsigned: no negative values
		R8,         // unorm
		R16F,
		R32F,
		D32,

		Count
	};

	// The size of one pixel as uploaded or read back. RGB counts 3: its uploads are packed 3 bytes a
	// pixel.
	constexpr uint32_t GetBytesPerPixel(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::RGB:         return 3;
			case TextureFormat::RGBA:
			case TextureFormat::RGBA8_UNORM: return 4;
			case TextureFormat::RGBA32F:     return 16;
			case TextureFormat::RGBA16F:     return 8;
			case TextureFormat::R11G11B10F:  return 4;
			case TextureFormat::R8:          return 1;
			case TextureFormat::R16F:        return 2;
			case TextureFormat::R32F:        return 4;
			case TextureFormat::D32:         return 4;
			default:                         return 0;
		}
	}

}
