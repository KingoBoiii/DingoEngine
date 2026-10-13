#pragma once

namespace Dingo
{

	/// A rectangle of the bound framebuffer, in pixels from its top-left corner. Draws are
	/// scissored to it as well.
	struct Viewport
	{
		float X = 0.0f;      ///< Left edge in pixels.
		float Y = 0.0f;      ///< Top edge in pixels.
		float Width = 0.0f;  ///< Width in pixels.
		float Height = 0.0f; ///< Height in pixels.
	};

}
