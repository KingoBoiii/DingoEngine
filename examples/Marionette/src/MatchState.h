#pragma once
#include "BoutFlow.h"

#include <cstdint>
#include <vector>

namespace Dingo
{

	struct BoutRecord
	{
		uint32_t Seed = 0;
		BoutWinner Winner = BoutWinner::None;
		float Seconds = 0.0f;
		float PlayerHealth = 0.0f;
		float OpponentHealth = 0.0f;
	};

	// Outlives the arena scene, which is rebuilt for every bout: the layer owns it, the arena reads and fills it.
	struct MatchState
	{
		int Bout = 1;
		int Retries = 0;
		float Seconds = 0.0f;
		bool Victory = false;
		// Set by the arena when another bout follows; the layer rebuilds the arena.
		bool Restart = false;
		// Set once a tournament has reported, so nothing runs on after the app is told to close.
		bool Done = false;
		std::vector<BoutRecord> Records;

		void ResetRun(int bout)
		{
			Bout = bout;
			Retries = 0;
			Seconds = 0.0f;
			Victory = false;
			Restart = false;
		}
	};

}
