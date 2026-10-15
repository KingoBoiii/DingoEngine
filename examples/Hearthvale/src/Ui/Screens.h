#pragma once
#include "Ui/ScreenHost.h"
#include "Ui/UiKit.h"

namespace Dingo
{

	// Each declares one screen's DingoUI tree from the pure layout of the same name, so the widgets and their order
	// are what ScreenLayouts.h models. They read the host and call its actions; they hold no state of their own.
	void DrawInventoryScreen(const UiKit& kit, ScreenHost& host, const Inventory& inventory, const std::string& focusRequest);
	void DrawBuildPaletteScreen(const UiKit& kit, ScreenHost& host, const Inventory& inventory, const BuildController& controller, const std::string& focusRequest);
	// Every menu screen: the page the host built, drawn as rows of text, buttons and sliders.
	void DrawPage(const UiKit& kit, ScreenHost& host, const PageSpec& page, const std::string& focusRequest);

}
