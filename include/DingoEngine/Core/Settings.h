#pragma once
#include "DingoEngine/Core/SaveData.h"
#include "DingoEngine/Windowing/DisplayMode.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace Dingo
{

	struct WindowParams;

	/// The window options a player picks.
	struct DisplaySettings
	{
		WindowMode Mode = WindowMode::Windowed;	///< Windowed, borderless or exclusive fullscreen.
		int32_t Width = 1600;		///< The windowed size in pixels; loading clamps it to 16384.
		int32_t Height = 900;		///< The windowed height in pixels; loading clamps it to 16384.
		DisplayMode FullscreenMode;	///< ExclusiveFullscreen's; invalid = the desktop's
		bool VSync = true;			///< Whether presentation waits for the display's refresh.
	};

	/// The volume options a player picks.
	struct AudioSettings
	{
		float MasterVolume = 1.0f;	///< Linear gain, 1 = unchanged; loading clamps it to 0..4.
		/// By bus name. Apply sets the buses that exist then; create a game's buses before it.
		std::map<std::string, float, std::less<>> BusVolumes;
	};

	/// The input options a player picks.
	struct InputSettings
	{
		/// Fraction of stick and trigger travel ignored, 0 to 0.95 (see Input::SetGamepadDeadzone).
		float GamepadDeadzone = 0.15f;
	};

	/// The options a player sets, kept in a save slot ("settings" by default). Game holds the
	/// game's own options as they are.
	struct Settings
	{
		/// The slot Save and Load use when none is named.
		static constexpr std::string_view k_DefaultSlot = "settings";

		DisplaySettings Display;	///< Window mode, size and VSync.
		AudioSettings Audio;		///< Master and per-bus volumes.
		InputSettings Input;		///< Gamepad deadzone.
		/// The game's own options, an object saved and loaded untouched.
		SaveData Game = SaveData::MakeObject();

		/// The running application's window, master volume, deadzone and the volumes of the
		/// buses base names, over base. Without an application, base; a headless one keeps base's
		/// Display.
		///@{
		static Settings Capture();
		static Settings Capture(const Settings& base);
		///@}
		/// Sets the window mode and size, VSync, the volumes and the deadzone. Changes nothing that
		/// already matches, so the window doesn't blink when nothing changed. Logs a warning and
		/// does nothing without an application; a headless one skips Display.
		void Apply() const;
		/// For ApplicationParams before the window exists.
		void ApplyTo(WindowParams& params) const;

		/// The settings as a SaveData object, ready for SaveSlots::Write or WriteFile.
		SaveData ToSaveData() const;
		/// Missing or malformed entries keep defaults' values; out-of-range ones are clamped.
		///@{
		static Settings FromSaveData(const SaveData& data);
		static Settings FromSaveData(const SaveData& data, const Settings& defaults);
		///@}

		/// Writes ToSaveData() to the slot; false (logged) when the slot name is invalid or the
		/// write fails.
		bool Save(std::string_view slot = k_DefaultSlot) const;
		/// The slot over Capture(), or Capture() alone when there is no readable slot (found
		/// false).
		static Settings Load(std::string_view slot = k_DefaultSlot, bool* found = nullptr);
	};

}
