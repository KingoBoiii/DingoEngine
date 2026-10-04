#pragma once
#include <DingoEngine.h>

#include <cstdint>
#include <string>

namespace Dingo
{

	enum class DriveMode : uint8_t
	{
		None,
		Ramp,
		Circle,
		Strafe,
		Wall,
		Duel
	};

	const char* ToString(DriveMode mode);

	struct LaunchOptions
	{
		int Bout = 0;             // 0 starts on the title; 1..3 skips it and picks the opponent; --drive and --freeze default to 1
		bool Lineup = false;      // the four-fighter lineup instead of a bout
		bool Freeze = false;
		bool Overview = false;
		bool Check = false;
		bool Autoplay = false;    // M4: AI vs AI
		bool DebugHitbox = false; // draws the hit and hurt spheres
		bool BreakHitbox = false; // moves every hitbox into the tail of its one-shot, where it never fires
		bool HotReload = false;
		DriveMode Drive = DriveMode::None; // scripted player input, for captures and logs
		float Move = -1.0f;       // with Freeze: the player's Move parameter; negative = idle
		float Phase = -1.0f;      // with Freeze: the locomotion's phase, 0..1; negative = the idle pose of the lineup
		std::string PoseClip;     // with Freeze: the player holds this clip at PoseTime
		float PoseTime = 0.0f;
		float FixedDt = 0.0f;     // seconds; 0 = the measured delta
		uint32_t Seed = 1;        // M4: the AI's random seed
	};

	// Parses once; later calls return the first result, whatever they are given.
	const LaunchOptions& ParseLaunchOptions(const ApplicationCommandLineArgs& args);

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
