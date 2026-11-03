#pragma once
#include "Checks/CheckReport.h"

#include <cstdint>
#include <functional>

namespace Dingo
{

	class ValleyDirector;
	class ValleyWorld;
	struct GameState;

	// The Replace suite: swaps the live game for a new valley, as New game and Load do, and reads that every
	// occupant of the new grid has an entity. Needs a window and a scene; never run headless.
	class ReplaceCheckRun
	{
	public:
		// Call once a frame before the world syncs. `replaced` is the layer's reaction to a new game, called after
		// each swap. Returns true once the suite has logged its summary.
		bool Tick(CheckReport& report, ValleyDirector& director, GameState& state, ValleyWorld& world, const std::function<void()>& replaced);

	private:
		enum class Step : uint8_t { Start, Other, Back, Done };

		bool Verify(CheckReport& report, const ValleyDirector& director, const ValleyWorld& world, const char* label) const;
		bool Finish(CheckReport& report);

	private:
		Step m_Step = Step::Start;
		int m_PassedAtStart = 0;
		int m_FailedAtStart = 0;
		uint32_t m_OriginalSeed = 1;
	};

}
