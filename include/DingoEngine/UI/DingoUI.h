#pragma once

namespace Dingo
{

	/// Options for the engine's game UI (DingoUI); set on ApplicationParams::DingoUI.
	struct DingoUIParams
	{
		/// Create the game UI. It costs nothing while no element is declared; off, every DingoUI
		/// call returns an empty result. A headless application never has one.
		bool Enabled = true;
	};

	/// The game UI: HUDs and menus declared every frame from OnUpdate, laid out, drawn over the
	/// scene and its post chain, and taking the pointer from the game where it lands on them.
	/// Static, like Input; call it from the main thread. ImGui stays the debug UI (Dingo::UI).
	class DingoUI
	{
	public:
		/// DingoUI is never instantiated; everything is static.
		DingoUI() = delete;

		/// True while the pointer is over an element that blocks input (as of the last frame's
		/// layout), for a game that reads the raw mouse and wants to ignore clicks on the UI.
		static bool IsPointerOverUI();
		/// True while the UI holds the pointer or the keyboard (a press it took and has not
		/// released, or a modal), so the game should leave that input alone.
		static bool IsCapturingInput();
	};

}
