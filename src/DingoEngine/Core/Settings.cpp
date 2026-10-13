#include "depch.h"
#include "DingoEngine/Core/Settings.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include <cmath>

namespace Dingo
{

	namespace
	{
		constexpr int64_t k_FormatVersion = 1;
		constexpr int32_t k_MaxWindowSide = 16384;

		std::optional<WindowMode> ParseWindowMode(std::string_view text)
		{
			for (WindowMode mode : { WindowMode::Windowed, WindowMode::BorderlessFullscreen, WindowMode::ExclusiveFullscreen })
			{
				if (text == WindowModeToString(mode))
					return mode;
			}
			return std::nullopt;
		}

		float ReadVolume(const SaveData& value, float fallback)
		{
			const double volume = value.AsDouble(fallback);
			return std::isfinite(volume) ? static_cast<float>(std::clamp(volume, 0.0, 4.0)) : fallback;
		}
	}

	Settings Settings::Capture()
	{
		return Capture(Settings());
	}

	Settings Settings::Capture(const Settings& base)
	{
		Settings settings = base;
		if (!Application::HasInstance())
			return settings;

		Application& application = Application::Get();
		const Window& window = application.GetWindow();
		settings.Display.Mode = window.GetWindowMode();
		settings.Display.Width = window.GetWindowedWidth();
		settings.Display.Height = window.GetWindowedHeight();
		if (window.GetWindowMode() == WindowMode::ExclusiveFullscreen)
			settings.Display.FullscreenMode = window.GetFullscreenDisplayMode();
		settings.Display.VSync = window.IsVSync();

		const AudioEngine& audio = application.GetAudioEngine();
		settings.Audio.MasterVolume = audio.GetMasterVolume();
		for (auto& [name, volume] : settings.Audio.BusVolumes)
		{
			const AudioBusId bus = audio.FindBus(name);
			if (bus != k_InvalidBus)
				volume = audio.GetBusVolume(bus);
		}

		settings.Input.GamepadDeadzone = Dingo::Input::GetGamepadDeadzone();
		return settings;
	}

	void Settings::Apply() const
	{
		if (!Application::HasInstance())
		{
			DE_CORE_WARN("Settings::Apply: no application to apply them to.");
			return;
		}

		Application& application = Application::Get();
		Window& window = application.GetWindow();
		if (window.GetWindowedWidth() != Display.Width || window.GetWindowedHeight() != Display.Height)
			window.SetSize(Display.Width, Display.Height);

		const bool exclusiveChanged = Display.Mode == WindowMode::ExclusiveFullscreen && Display.FullscreenMode.IsValid()
			&& window.GetFullscreenDisplayMode() != Display.FullscreenMode;
		if (window.GetWindowMode() != Display.Mode || exclusiveChanged)
			window.SetWindowMode(Display.Mode, Display.FullscreenMode);
		window.SetVSync(Display.VSync);

		AudioEngine& audio = application.GetAudioEngine();
		audio.SetMasterVolume(Audio.MasterVolume);
		for (const auto& [name, volume] : Audio.BusVolumes)
		{
			const AudioBusId bus = audio.FindBus(name);
			if (bus != k_InvalidBus)
				audio.SetBusVolume(bus, volume);
		}

		Dingo::Input::SetGamepadDeadzone(Input.GamepadDeadzone);
	}

	void Settings::ApplyTo(WindowParams& params) const
	{
		params.Mode = Display.Mode;
		params.Width = Display.Width;
		params.Height = Display.Height;
		params.FullscreenMode = Display.FullscreenMode;
		params.VSync = Display.VSync;
	}

	SaveData Settings::ToSaveData() const
	{
		SaveData data;
		data["version"] = k_FormatVersion;

		SaveData& display = data["display"];
		display["mode"] = WindowModeToString(Display.Mode);
		display["width"] = Display.Width;
		display["height"] = Display.Height;
		display["fullscreenMode"] = glm::ivec3(Display.FullscreenMode.Width, Display.FullscreenMode.Height, Display.FullscreenMode.RefreshRate);
		display["vsync"] = Display.VSync;

		SaveData& audio = data["audio"];
		audio["master"] = Audio.MasterVolume;
		SaveData& buses = audio["buses"];
		buses = SaveData::MakeObject();
		for (const auto& [name, volume] : Audio.BusVolumes)
			buses[name] = volume;

		data["input"]["gamepadDeadzone"] = Input.GamepadDeadzone;
		data["game"] = Game.IsObject() ? Game : SaveData::MakeObject();
		return data;
	}

	Settings Settings::FromSaveData(const SaveData& data)
	{
		return FromSaveData(data, Settings());
	}

	Settings Settings::FromSaveData(const SaveData& data, const Settings& defaults)
	{
		Settings settings = defaults;

		const SaveData& display = data["display"];
		if (const std::optional<WindowMode> mode = ParseWindowMode(display["mode"].AsString()))
			settings.Display.Mode = *mode;
		const int32_t width = display.Get("width", int32_t(0));
		const int32_t height = display.Get("height", int32_t(0));
		if (width > 0 && height > 0)
		{
			settings.Display.Width = (std::min)(width, k_MaxWindowSide);
			settings.Display.Height = (std::min)(height, k_MaxWindowSide);
		}
		if (const SaveData* fullscreen = display.Find("fullscreenMode"))
		{
			const glm::ivec3 mode = fullscreen->AsIVec3(glm::ivec3(-1));
			if (mode.x >= 0 && mode.y >= 0 && mode.z >= 0)
				settings.Display.FullscreenMode = DisplayMode{ mode.x, mode.y, mode.z };
		}
		settings.Display.VSync = display.Get("vsync", settings.Display.VSync);

		const SaveData& audio = data["audio"];
		settings.Audio.MasterVolume = ReadVolume(audio["master"], settings.Audio.MasterVolume);
		const SaveData& buses = audio["buses"];
		for (size_t i = 0; i < buses.Size() && buses.IsObject(); i++)
		{
			const std::string name(buses.GetKey(i));
			const auto existing = settings.Audio.BusVolumes.find(name);
			const float fallback = existing != settings.Audio.BusVolumes.end() ? existing->second : 1.0f;
			if (buses.At(i).IsNumber())
				settings.Audio.BusVolumes[name] = ReadVolume(buses.At(i), fallback);
		}

		const double deadzone = data["input"]["gamepadDeadzone"].AsDouble(settings.Input.GamepadDeadzone);
		if (std::isfinite(deadzone))
			settings.Input.GamepadDeadzone = static_cast<float>(std::clamp(deadzone, 0.0, 0.95));

		if (const SaveData* game = data.Find("game"); game && game->IsObject())
			settings.Game = *game;
		return settings;
	}

	bool Settings::Save(std::string_view slot) const
	{
		return SaveSlots::Write(slot, ToSaveData());
	}

	Settings Settings::Load(std::string_view slot, bool* found)
	{
		const Settings current = Capture();
		if (found)
			*found = false;
		if (!SaveSlots::Exists(slot))
			return current;

		const std::optional<SaveData> data = SaveSlots::Read(slot);
		if (!data)
			return current;
		if (found)
			*found = true;
		return FromSaveData(*data, current);
	}

}
