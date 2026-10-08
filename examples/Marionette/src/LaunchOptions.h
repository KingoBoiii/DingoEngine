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
		int Bout = 0;             // 0 starts on the title; 1..3 skips it and picks the opponent; --drive, --freeze and --lineup default to 1
		bool Lineup = false;      // the four-fighter lineup instead of a bout
		bool Freeze = false;
		bool Overview = false;
		bool End = false;         // starts on the End scene as after a win, for captures
		bool Check = false;
		bool Autoplay = false;    // the player's brain is an AI of PlayerTier; AI vs AI
		bool DebugHitbox = false; // draws the hit and hurt spheres
		bool BreakHitbox = false; // moves every hitbox into the tail of its one-shot, where it never fires
		bool HotReload = false;
		bool Perf = false;        // with Autoplay: logs the mean frame, update and render times once, after a warm-up
		bool LiveEditDemo = false; // reads a copy of the assets and edits a hitbox range in it after LIVE_EDIT_DELAY; implies HotReload
		DriveMode Drive = DriveMode::None; // scripted player input, for captures and logs
		float Move = -1.0f;       // with Freeze: the player's Move parameter; negative = idle
		float Phase = -1.0f;      // with Freeze: the locomotion's phase, 0..1; negative = the idle pose of the lineup
		std::string PoseClip;     // with Freeze: the player holds this clip at PoseTime
		float PoseTime = 0.0f;
		float FixedDt = 0.0f;     // seconds; 0 = the measured delta; --freeze and --tournament default it to 1/60
		uint32_t Seed = 1;        // the AI's random seed; a tournament uses Seed to Seed + Tournament - 1
		int Tournament = 0;       // N > 0: tier PlayerTier plays bout Bout N times, reports and closes; implies Autoplay and a fixed delta
		int PlayerTier = 3;       // the autoplay player's AI tier
		int StepsPerFrame = 1;    // scene updates per rendered frame; only with a fixed delta
		bool NoPost = false;      // the v0.9 switches, each on its own, for A/B captures and --perf
		bool NoShadows = false;
		bool NoParticles = false;
	};

	// A run nobody plays by hand: an unfocused window must not pause it.
	bool IsScripted(const LaunchOptions& options);

	// Parses once; later calls return the first result, whatever they are given.
	const LaunchOptions& ParseLaunchOptions(const ApplicationCommandLineArgs& args);

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
