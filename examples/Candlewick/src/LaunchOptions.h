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
		bool AllLit = false;     // every brazier but the altar starts lit
		float FixedDt = 0.0f;    // seconds; 0 = the measured delta; --freeze defaults it to 1/60
		bool Perf = false;       // logs the mean frame, update, render and GPU pass times once, after a warm-up
		bool NoPost = false;
		bool NoShadows = false;
		bool NoParticles = false;
		bool HideCheck = false;  // logs what hides the player, once, after HIDE_CHECK_FRAME frames in the keep
		std::optional<glm::ivec2> Spawn; // --spawn=<col>,<row>: the player's start tile
	};

	// Parsed on first use, so it must not be called before the Application exists.
	const LaunchOptions& GetLaunchOptions();

}
