#pragma once
#include <DingoEngine.h>

#include <cstdint>

namespace Dingo
{

	enum class DriveMode : uint8_t
	{
		None,
		Ramp,
		Circle,
		Strafe,
		Wall
	};

	const char* ToString(DriveMode mode);

	struct LaunchOptions
	{
		int Bout = 0;             // 0 starts on the title; 1..3 skips it and picks the opponent
		bool Lineup = false;      // the four-fighter lineup instead of a bout
		bool Freeze = false;
		bool Overview = false;
		bool Check = false;
		bool Autoplay = false;    // M3: AI vs AI
		bool DebugHitbox = false; // M3: draws hit and hurt spheres
		bool HotReload = false;
		DriveMode Drive = DriveMode::None; // scripted player input, for captures and logs
		float Move = -1.0f;       // with Freeze: the player's Move parameter; negative = idle
		float Phase = -1.0f;      // with Freeze: the locomotion's phase, 0..1; negative = the idle pose of the lineup
		float FixedDt = 0.0f;     // seconds; 0 = the measured delta
		uint32_t Seed = 1;        // M3: the AI's random seed
	};

	// Parses once; later calls return the first result, whatever they are given.
	const LaunchOptions& ParseLaunchOptions(const ApplicationCommandLineArgs& args);

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
