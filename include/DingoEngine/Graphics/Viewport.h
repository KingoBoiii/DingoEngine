#pragma once

namespace Dingo
{

	// A rectangle of the bound framebuffer, in pixels from its top-left corner. Draws are scissored
	// to it as well.
	struct Viewport
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
	};

}
