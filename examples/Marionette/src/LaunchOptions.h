#pragma once
#include <DingoEngine.h>

#include <cstdint>

namespace Dingo
{

	struct LaunchOptions
	{
		int Bout = 0;             // 0 starts on the title, 1..3 skips it; M4: bouts 2 and 3 pick their opponent
		bool Lineup = false;      // M2: forces the lineup once the arena holds a bout
		bool Freeze = false;
		bool Overview = false;
		bool Check = false;
		bool Autoplay = false;    // M3: AI vs AI
		bool DebugHitbox = false; // M3: draws hit and hurt spheres
		bool HotReload = false;
		float FixedDt = 0.0f;     // seconds; 0 = the measured delta
		uint32_t Seed = 1;        // M3: the AI's random seed
	};

	// Parses once; later calls return the first result, whatever they are given.
	const LaunchOptions& ParseLaunchOptions(const ApplicationCommandLineArgs& args);

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
