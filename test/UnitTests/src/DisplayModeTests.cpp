#include <doctest/doctest.h>

#include <DingoEngine/Core/Settings.h>
#include <DingoEngine/Windowing/DisplayMode.h>

using namespace Dingo;

TEST_SUITE("DisplayMode")
{
	TEST_CASE("modes sort largest first without duplicates or empty sizes")
	{
		const std::vector<DisplayMode> sorted = SortDisplayModes({
			{ 1280, 720, 60 }, { 1920, 1080, 60 }, { 1920, 1080, 144 }, { 1280, 720, 60 }, { 0, 0, 60 }, { 1440, 1080, 60 }, { 1920, 1080, 60 } });
		REQUIRE(sorted.size() == 4);
		CHECK(sorted[0] == DisplayMode{ 1920, 1080, 144 });
		CHECK(sorted[1] == DisplayMode{ 1920, 1080, 60 });
		CHECK(sorted[2] == DisplayMode{ 1440, 1080, 60 });
		CHECK(sorted[3] == DisplayMode{ 1280, 720, 60 });
	}

	TEST_CASE("the nearest listed mode is chosen")
	{
		const std::vector<DisplayMode> modes = SortDisplayModes({
			{ 2560, 1440, 60 }, { 1920, 1080, 60 }, { 1920, 1080, 144 }, { 1680, 1050, 60 }, { 1280, 1024, 75 }, { 1280, 720, 60 } });

		CHECK(ChooseDisplayMode(modes, { 1920, 1080, 60 }) == DisplayMode{ 1920, 1080, 60 });
		CHECK(ChooseDisplayMode(modes, { 1920, 1080, 0 }) == DisplayMode{ 1920, 1080, 144 });
		CHECK(ChooseDisplayMode(modes, { 1920, 1080, 120 }) == DisplayMode{ 1920, 1080, 144 });
		CHECK(ChooseDisplayMode(modes, { 1920, 1080, 75 }) == DisplayMode{ 1920, 1080, 60 });
		CHECK(ChooseDisplayMode(modes, { 1700, 1050, 60 }) == DisplayMode{ 1680, 1050, 60 });
		CHECK(ChooseDisplayMode(modes, { 1280, 960, 60 }) == DisplayMode{ 1280, 1024, 75 });
		CHECK(ChooseDisplayMode(modes, { 3840, 2160, 60 }) == DisplayMode{ 2560, 1440, 60 });
		CHECK(ChooseDisplayMode(modes, {}) == DisplayMode{});
		CHECK(ChooseDisplayMode({}, { 800, 600, 60 }) == DisplayMode{ 800, 600, 60 });
	}

	TEST_CASE("window modes have names")
	{
		CHECK(std::string_view(WindowModeToString(WindowMode::ExclusiveFullscreen)) == "ExclusiveFullscreen");
	}
}

TEST_SUITE("Settings")
{
	TEST_CASE("settings round-trip through save data")
	{
		Settings settings;
		settings.Display.Mode = WindowMode::ExclusiveFullscreen;
		settings.Display.Width = 1280;
		settings.Display.Height = 720;
		settings.Display.FullscreenMode = { 1920, 1080, 144 };
		settings.Display.VSync = false;
		settings.Audio.MasterVolume = 0.5f;
		settings.Audio.BusVolumes["Music"] = 0.25f;
		settings.Audio.BusVolumes["Sfx"] = 0.75f;
		settings.Input.GamepadDeadzone = 0.2f;
		settings.Game["sensitivity"] = 1.5;
		settings.Game["language"] = "da";

		const std::optional<SaveData> data = SaveData::FromJson(settings.ToSaveData().ToJson());
		REQUIRE(data.has_value());
		const Settings read = Settings::FromSaveData(*data);
		CHECK(read.Display.Mode == WindowMode::ExclusiveFullscreen);
		CHECK(read.Display.Width == 1280);
		CHECK(read.Display.Height == 720);
		CHECK(read.Display.FullscreenMode == DisplayMode{ 1920, 1080, 144 });
		CHECK_FALSE(read.Display.VSync);
		CHECK(read.Audio.MasterVolume == 0.5f);
		CHECK(read.Audio.BusVolumes == settings.Audio.BusVolumes);
		CHECK(read.Input.GamepadDeadzone == 0.2f);
		CHECK(read.Game == settings.Game);
	}

	TEST_CASE("missing and bad entries keep the defaults, out-of-range ones are clamped")
	{
		Settings defaults;
		defaults.Display.Width = 1024;
		defaults.Audio.BusVolumes["Music"] = 0.3f;

		const std::optional<SaveData> data = SaveData::FromJson(R"({
			"display": { "mode": "Sideways", "width": -5, "height": 600, "fullscreenMode": "big", "vsync": 1 },
			"audio": { "master": 99, "buses": { "Music": "loud", "Voice": 0.5 } },
			"input": { "gamepadDeadzone": 3 },
			"game": [1, 2]
		})");
		REQUIRE(data.has_value());
		const Settings read = Settings::FromSaveData(*data, defaults);
		CHECK(read.Display.Mode == WindowMode::Windowed);
		CHECK(read.Display.Width == 1024);
		CHECK(read.Display.Height == 900);
		CHECK_FALSE(read.Display.FullscreenMode.IsValid());
		CHECK(read.Display.VSync);
		CHECK(read.Audio.MasterVolume == 4.0f);
		CHECK(read.Audio.BusVolumes.at("Music") == 0.3f);
		CHECK(read.Audio.BusVolumes.at("Voice") == 0.5f);
		CHECK(read.Input.GamepadDeadzone == 0.95f);
		CHECK(read.Game.IsObject());
		CHECK(read.Game.IsEmpty());

		CHECK(Settings::FromSaveData(SaveData()).Display.Width == 1600);
	}
}
