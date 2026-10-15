#include "Ui/OptionsModel.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_KeyCameraSpeed = "cameraSpeed";
		constexpr const char* k_KeyHoldToPlace = "holdToPlace";
		constexpr const char* k_KeyExclusive = "exclusiveFullscreen";

		float Snap(float value, float min, float max, float step)
		{
			if (!std::isfinite(value))
				return min;
			const float clamped = (std::clamp)(value, min, max);
			return (std::clamp)(min + std::round((clamped - min) / step) * step, min, max);
		}

		float BusVolume(const Settings& settings, const char* bus, float fallback)
		{
			const auto found = settings.Audio.BusVolumes.find(bus);
			return found == settings.Audio.BusVolumes.end() ? fallback : found->second;
		}
	}

	OptionsState ClampOptions(const OptionsState& state)
	{
		OptionsState out = state;
		out.Master = Snap(state.Master, 0.0f, 1.0f, OPTION_VOLUME_STEP);
		out.Music = Snap(state.Music, 0.0f, 1.0f, OPTION_VOLUME_STEP);
		out.Ambience = Snap(state.Ambience, 0.0f, 1.0f, OPTION_VOLUME_STEP);
		out.Sfx = Snap(state.Sfx, 0.0f, 1.0f, OPTION_VOLUME_STEP);
		out.Ui = Snap(state.Ui, 0.0f, 1.0f, OPTION_VOLUME_STEP);
		out.CameraSpeed = Snap(state.CameraSpeed, OPTION_CAMERA_MIN, OPTION_CAMERA_MAX, OPTION_CAMERA_STEP);
		return out;
	}

	Settings OptionsToSettings(const OptionsState& state, const Settings& base)
	{
		const OptionsState clean = ClampOptions(state);
		Settings settings = base;
		settings.Audio.MasterVolume = clean.Master;
		settings.Audio.BusVolumes[k_BusMusic] = clean.Music;
		settings.Audio.BusVolumes[k_BusAmbience] = clean.Ambience;
		settings.Audio.BusVolumes[k_BusSfx] = clean.Sfx;
		settings.Audio.BusVolumes[k_BusUi] = clean.Ui;
		settings.Display.VSync = clean.VSync;
		settings.Display.Mode = !clean.Fullscreen ? WindowMode::Windowed : (clean.Exclusive ? WindowMode::ExclusiveFullscreen : WindowMode::BorderlessFullscreen);
		if (!settings.Game.IsObject())
			settings.Game = SaveData::MakeObject();
		settings.Game[k_KeyCameraSpeed] = clean.CameraSpeed;
		settings.Game[k_KeyHoldToPlace] = clean.HoldToPlace;
		settings.Game[k_KeyExclusive] = clean.Exclusive;
		return settings;
	}

	OptionsState OptionsFromSettings(const Settings& settings)
	{
		const OptionsState defaults;
		OptionsState state;
		state.Master = settings.Audio.MasterVolume;
		state.Music = BusVolume(settings, k_BusMusic, defaults.Music);
		state.Ambience = BusVolume(settings, k_BusAmbience, defaults.Ambience);
		state.Sfx = BusVolume(settings, k_BusSfx, defaults.Sfx);
		state.Ui = BusVolume(settings, k_BusUi, defaults.Ui);
		state.VSync = settings.Display.VSync;
		state.Fullscreen = settings.Display.Mode != WindowMode::Windowed;
		state.Exclusive = settings.Display.Mode == WindowMode::ExclusiveFullscreen
			|| (!state.Fullscreen && settings.Game.Get(k_KeyExclusive, false));
		state.CameraSpeed = settings.Game.Get(k_KeyCameraSpeed, defaults.CameraSpeed);
		state.HoldToPlace = settings.Game.Get(k_KeyHoldToPlace, defaults.HoldToPlace);
		return ClampOptions(state);
	}

	std::string VolumeText(float volume)
	{
		return std::format("{}%", static_cast<int>(std::lround(volume * 100.0f)));
	}

	std::string CameraSpeedText(float speed)
	{
		return std::format("{:.1f}x", speed);
	}

	std::string DisplayModeText(const OptionsState& state)
	{
		return state.Exclusive ? "Exclusive" : "Borderless";
	}

}
