#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// Shared, game-wide state. The GameLayer owns one instance and passes a pointer
	// to the scripts that need it. Scripts read/write the score, lives, etc.; the
	// GameLayer reads it to update the HUD and trigger the game-over transition.
	struct GameContext
	{
		int Score = 0;
		int Lives = 3;
		int Wave = 1;
		bool GameOver = false;

		// Playfield half-extents in world units (set once from the window aspect).
		float HalfWidth = 0.0f;
		float HalfHeight = 0.0f;

		// The player entity, so invader bombs can test for a hit.
		Entity Player;
	};

}
