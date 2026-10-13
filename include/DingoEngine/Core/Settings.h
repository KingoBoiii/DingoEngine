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

	struct DisplaySettings
	{
		WindowMode Mode = WindowMode::Windowed;
		int32_t Width = 1600;		// the windowed size
		int32_t Height = 900;
		DisplayMode FullscreenMode;	// ExclusiveFullscreen's; invalid = the desktop's
		bool VSync = true;
	};

	struct AudioSettings
	{
		float MasterVolume = 1.0f;
		// By bus name. Apply sets the buses that exist then; create a game's buses before it.
		std::map<std::string, float, std::less<>> BusVolumes;
	};

	struct InputSettings
	{
		float GamepadDeadzone = 0.15f;
	};

	// The options a player sets, kept in a save slot ("settings" by default). Game holds the
	// game's own options as they are.
	struct Settings
	{
		static constexpr std::string_view k_DefaultSlot = "settings";

		DisplaySettings Display;
		AudioSettings Audio;
		InputSettings Input;
		SaveData Game = SaveData::MakeObject();

		// The running application's window, master volume, deadzone and the volumes of the
		// buses base names, over base. Without an application, base.
		static Settings Capture();
		static Settings Capture(const Settings& base);
		// Sets the window mode and size, VSync, the volumes and the deadzone. Changes nothing that
		// already matches, so the window doesn't blink when nothing changed.
		void Apply() const;
		// For ApplicationParams before the window exists.
		void ApplyTo(WindowParams& params) const;

		SaveData ToSaveData() const;
		// Missing or malformed entries keep defaults' values; out-of-range ones are clamped.
		static Settings FromSaveData(const SaveData& data);
		static Settings FromSaveData(const SaveData& data, const Settings& defaults);

		bool Save(std::string_view slot = k_DefaultSlot) const;
		// The slot over Capture(), or Capture() alone when there is no readable slot (found false).
		static Settings Load(std::string_view slot = k_DefaultSlot, bool* found = nullptr);
	};

}
