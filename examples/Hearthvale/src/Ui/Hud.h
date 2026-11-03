#pragma once
#include "Ui/ScreenHost.h"
#include "Ui/UiKit.h"

namespace Dingo
{

	// The always-on HUD: clock and day bar, coins and materials, the hotbar, a two-line hint strip and the toast.
	// Every root is non-blocking, so the world still gets the mouse.
	class Hud
	{
	public:
		void Draw(const UiKit& kit, const HudContent& content, const std::string& toast, bool showHints);

	private:
		void DrawClock(const UiKit& kit, const HudContent& content);
		void DrawResources(const UiKit& kit, const HudContent& content);
		void DrawBottom(const UiKit& kit, const HudContent& content, const std::string& toast, bool showHints);
	};

}
