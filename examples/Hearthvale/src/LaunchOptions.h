#pragma once
#include <DingoEngine.h>

#include <cstdint>
#include <string>

namespace Dingo
{

	enum class LaunchSeason : uint8_t { Spring, Summer, Fall, Winter };
	enum class LaunchWeather : uint8_t { Auto, Clear, Rain, Snow };

	const char* ToString(LaunchSeason season);
	const char* ToString(LaunchWeather weather);

	struct LaunchOptions
	{
		bool Check = false;
		bool Autoplay = false;
		int Days = 0;               // with Autoplay: plays this many days and closes; implies FixedDt
		float FixedDt = 0.0f;       // seconds; 0 = the measured delta
		int StepsPerFrame = 1;      // sim steps per OnUpdate; only with a fixed delta
		uint32_t Seed = 1;          // valley and weather
		bool HasSeed = false;       // --seed given: a new game from the slots screen uses it instead of a random one
		int Day = 0;                // 1-based day of the season to start on; 0 = a fresh game's
		bool HasSeason = false;
		LaunchSeason Season = LaunchSeason::Spring;
		float TimeHours = -1.0f;    // start time of day in hours; negative = the morning
		LaunchWeather Weather = LaunchWeather::Auto;
		std::string Load;           // slot to load; empty = a new game
		bool LayoutDemo = false;    // --layout=demo
		bool FarmerModel = false;   // --farmer=model draws the KayKit character; the default is the placeholder body
		bool Perf = false;
		bool Freeze = false;
		bool NoPost = false;
		bool NoShadows = false;
		bool NoParticles = false;
		bool NoFog = false;
		bool NoInstancing = false;
		bool NoStatic = false;
		bool NoCull = false;
	};

	bool IsScripted(const LaunchOptions& options);

	// Parses once; later calls return the first result, whatever they are given.
	const LaunchOptions& ParseLaunchOptions(const ApplicationCommandLineArgs& args);

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
