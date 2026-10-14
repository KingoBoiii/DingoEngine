#pragma once

#include <cstdint>

namespace Dingo
{

	/// A vertex attribute or texel format, named like the graphics APIs': channels with bit widths,
	/// then UINT/SINT (integer), UNORM/SNORM (normalised to 0..1 or -1..1) or FLOAT.
	enum class Format : uint8_t
	{
		UNKNOWN, ///< No format; an attribute left at this is not valid.

		R8_UINT,
		R8_SINT,
		R8_UNORM,
		R8_SNORM,
		RG8_UINT,
		RG8_SINT,
		RG8_UNORM,
		RG8_SNORM,
		R16_UINT,
		R16_SINT,
		R16_UNORM,
		R16_SNORM,
		R16_FLOAT,
		BGRA4_UNORM,
		B5G6R5_UNORM,
		B5G5R5A1_UNORM,
		RGBA8_UINT,
		RGBA8_SINT,
		RGBA8_UNORM,
		RGBA8_SNORM,
		BGRA8_UNORM,
		SRGBA8_UNORM, ///< RGBA8 with sRGB encoding, decoded to linear when sampled.
		SBGRA8_UNORM, ///< BGRA8 with sRGB encoding, decoded to linear when sampled.
		R10G10B10A2_UNORM,
		R11G11B10_FLOAT,
		RG16_UINT,
		RG16_SINT,
		RG16_UNORM,
		RG16_SNORM,
		RG16_FLOAT,
		R32_UINT,
		R32_SINT,
		R32_FLOAT,
		RGBA16_UINT,
		RGBA16_SINT,
		RGBA16_FLOAT,
		RGBA16_UNORM,
		RGBA16_SNORM,
		RG32_UINT,
		RG32_SINT,
		RG32_FLOAT,
		RGB32_UINT,
		RGB32_SINT,
		RGB32_FLOAT,
		RGBA32_UINT,
		RGBA32_SINT,
		RGBA32_FLOAT,

		D16,        ///< Depth formats: D<depth bits>[S<stencil bits>].
		D24S8,
		X24G8_UINT, ///< The stencil channel of D24S8, read as integers.
		D32,
		D32S8,
		X32G8_UINT, ///< The stencil channel of D32S8, read as integers.

		BC1_UNORM, ///< Block-compressed formats (BC1 to BC7), 4x4 texel blocks.
		BC1_UNORM_SRGB,
		BC2_UNORM,
		BC2_UNORM_SRGB,
		BC3_UNORM,
		BC3_UNORM_SRGB,
		BC4_UNORM,
		BC4_SNORM,
		BC5_UNORM,
		BC5_SNORM,
		BC6H_UFLOAT,
		BC6H_SFLOAT,
		BC7_UNORM,
		BC7_UNORM_SRGB,

		COUNT, ///< The number of formats, not a format.
	};

}
