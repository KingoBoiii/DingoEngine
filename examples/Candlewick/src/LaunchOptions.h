#pragma once

namespace Dingo
{

	struct LaunchOptions
	{
		int Room = 0;            // 1..4 starts in that room of the keep; 0 starts on the title
		bool Freeze = false;
		bool Overview = false;
		int Oil = -1;            // -1 = not given
		bool NoLightLod = false;
		bool DebugCone = false;
	};

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
