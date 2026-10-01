#pragma once

#include <glm/glm.hpp>

#include <optional>

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
		bool NoRangeClamp = false;
		bool AllLit = false;     // every brazier but the altar starts lit
		std::optional<glm::ivec2> Spawn; // --spawn=<col>,<row>: the player's start tile
	};

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
