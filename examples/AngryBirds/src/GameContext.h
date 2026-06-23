#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// Shared, game-wide state. The GameLayer owns one instance and passes a pointer
	// to the pig scripts (which decrement the pig count and add score on a pop). The
	// GameLayer reads it to drive the HUD and the win/lose transitions.
	struct GameContext
	{
		int Score = 0;
		int Level = 1;
		int BirdsLeft = 0;
		int PigsLeft = 0;
		bool Won = false;   // set by the layer when the last level is cleared

		// Playfield half-extents in world units (set once from the window aspect).
		float HalfWidth = 0.0f;
		float HalfHeight = 0.0f;
	};

}
