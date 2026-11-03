#pragma once
#include <DingoEngine/Core/Settings.h>

#include <string>

namespace Dingo
{

	inline constexpr const char* k_BusMusic = "Music";
	inline constexpr const char* k_BusAmbience = "Ambience";
	inline constexpr const char* k_BusSfx = "SFX";
	inline constexpr const char* k_BusUi = "UI";

	inline constexpr float OPTION_VOLUME_STEP = 0.05f;
	inline constexpr float OPTION_CAMERA_MIN = 0.5f;
	inline constexpr float OPTION_CAMERA_MAX = 2.0f;
	inline constexpr float OPTION_CAMERA_STEP = 0.1f;

	// What the options screen edits, with no engine object in it: the screen, the Settings round trip and --check all
	// work on this, and only the layer applies it to the window and the audio buses.
	struct OptionsState
	{
		float Master = 1.0f;
		float Music = 0.7f;
		float Ambience = 0.7f;
		float Sfx = 1.0f;
		float Ui = 1.0f;
		bool VSync = true;
		bool Fullscreen = false;
		bool Exclusive = false;
		float CameraSpeed = 1.0f;
		bool HoldToPlace = false;

		bool operator==(const OptionsState&) const = default;
	};

	// Volumes to 0..1 on the step, the camera speed to its range on the step.
	OptionsState ClampOptions(const OptionsState& state);

	// `base` supplies everything the screen does not edit (window size, gamepad deadzone, other Game keys).
	Settings OptionsToSettings(const OptionsState& state, const Settings& base);
	OptionsState OptionsFromSettings(const Settings& settings);

	std::string VolumeText(float volume);
	std::string CameraSpeedText(float speed);
	std::string DisplayModeText(const OptionsState& state);

}
