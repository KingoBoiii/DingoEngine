#pragma once

namespace Dingo
{

	/// Number of dimensions a texture has (TextureParams::Dimension).
	enum class TextureDimension
	{
		Unknown, ///< Not set; give a texture a real dimension.

		Texture1D,
		Texture2D,
		Texture3D,
		/// Six square faces in layers 0..5: +X, -X, +Y, -Y, +Z, -Z (TextureParams::ArrayLayers is 6).
		TextureCube,

		Count ///< The number of dimensions, not a dimension.
	};

}
