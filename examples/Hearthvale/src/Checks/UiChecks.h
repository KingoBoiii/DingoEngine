#pragma once
#include "Checks/CheckReport.h"

#include <cstdint>

namespace Dingo
{

	class GameUi;
	class Inventory;
	class BuildController;
	class ScreenHost;
	class ValleyDirector;

	// The UI suite's windowed half: measures every label with the real font, then opens each screen on the
	// live host and reads what DingoUI reports. The pad cannot be driven from a game (Input's simulated pad is
	// private), so focus moves are covered by the pure suite's mirror of DingoUI's navigation, and a pad pass is
	// P18's. Needs a window and the renderer; never run headless.
	class UiCheckRun
	{
	public:
		// Call once a frame before the UI draws. Returns true once the suite has logged its summary.
		bool Tick(CheckReport& report, GameUi& ui, ScreenHost& host, Inventory& inventory, BuildController& controller, ValleyDirector& director);

	private:
		enum class Step : uint8_t { Start, OpenInventory, InventoryOpen, Moved, Palette, PaletteOpen, Chosen, Resumed, MenuSweep, Done };

		void CheckMenuFonts(CheckReport& report, GameUi& ui);
		bool SweepMenus(CheckReport& report, ScreenHost& host, ValleyDirector& director);
		void CheckFonts(CheckReport& report, GameUi& ui, const Inventory& inventory, BuildController& controller, const ValleyDirector& director);
		bool Finish(CheckReport& report);

	private:
		Step m_Step = Step::Start;
		int m_Frames = 0;
		int m_TickAtOpen = 0;
		int m_MenuIndex = 0;
		bool m_MenuOpen = false;
		int m_PassedAtStart = 0;
		int m_FailedAtStart = 0;
	};

}
