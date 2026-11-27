#pragma once

#include <DingoEngine.h>

namespace Dingo
{

	struct LaunchOptions
	{
		bool Check = false;
		int Environment = 0;
		float TimeHours = 15.0f;
		bool NoRaymarch = false;
		bool Freeze = false;
		int View = 1;
	};

	LaunchOptions ParseLaunchOptions(const ApplicationCommandLineArgs& args);
	const LaunchOptions& GetLaunchOptions();

}
