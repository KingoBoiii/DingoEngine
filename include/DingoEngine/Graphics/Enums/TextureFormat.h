#pragma once

#include <cstdint>

namespace Dingo
{

	/// Pixel format of a Texture (TextureParams::Format). Not the same list as Format.
	enum class TextureFormat
	{
		Unknown, ///< Not set; give a texture a real format.

		RGBA8_UNORM, ///< 8 bits a channel, 0..1; the format for ordinary colour textures.
		RGBA = RGBA8_UNORM, ///< Alias of RGBA8_UNORM.

		RGBA32F, ///< 32-bit float channels.

		// Render-target formats (v0.9): HDR colour, single channels and a depth that can be
		// sampled.
		RGBA16F,
		R11G11B10F, ///< unsigned: no negative values
		R8,         ///< unorm
		R16F,
		R32F,
		D32, ///< 32-bit float depth; sampled only with TextureParams::IsShaderResource.

		Count ///< The number of formats, not a format.
	};

	/// The size of one pixel as uploaded or read back, in bytes; 0 for Unknown and Count.
	constexpr uint32_t GetBytesPerPixel(TextureFormat format)
	{
		switch (format)
		{
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
