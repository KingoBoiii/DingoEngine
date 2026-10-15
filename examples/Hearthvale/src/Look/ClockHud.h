#pragma once
#include <DingoEngine.h>

#include "Farm/Calendar.h"

namespace Dingo
{

	// The temporary text clock; P8 replaces it with the real HUD. GPU side: never constructed headless.
	class ClockHud
	{
	public:
		ClockHud();

		// Declares the root; call once per frame.
		void Draw(const Calendar& calendar);

	private:
		Font* m_Font = nullptr;
	};

}
