#pragma once

namespace Dingo
{

	/// What a sampler reads for texture coordinates outside 0..1; the lower names are aliases.
	enum class SamplerAddressMode
	{
		Clamp,      ///< Repeats the edge texel.
		Wrap,       ///< Tiles the texture.
		Border,     ///< Reads SamplerParams::BorderColor.
		Mirror,     ///< Tiles the texture, flipping every other tile.
		MirrorOnce, ///< Mirrors once about 0, then clamps.

		ClampToEdge = Clamp,
		Repeat = Wrap,
		ClampToBorder = Border,
		MirroredRepeat = Mirror,
		MirrorClampToEdge = MirrorOnce
	};

}
