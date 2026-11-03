#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// A full-screen black panel at the front of the DingoUI roots; the sleep fade drives its alpha. GPU side:
	// never constructed headless.
	class FadeOverlay
	{
	public:
		// Declares the root while alpha is above zero; call once per frame.
		void Draw(float alpha);
	};

}
