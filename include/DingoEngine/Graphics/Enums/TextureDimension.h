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

		Count ///< The number of dimensions, not a dimension.
	};

}
