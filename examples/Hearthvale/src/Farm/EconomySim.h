#pragma once
#include "Farm/GameState.h"
#include "ValleyDirector.h"

namespace Dingo
{

	// What the latest day roll paid and the board did.
	struct EconomyRollReport
	{
		ShippingPayout Shipped;
		BoardReport Board;
	};

	// The economy's part of the day roll, on the director's hooks: at Shipping the bin pays out, and what
	// it paid counts toward the board's shipping and earning requests; at Board the board is settled for the new day and
	// its rewards are banked. The pass-out coin loss is FarmSim's, at the earlier Crops step, so the payout is never taxed.
	// Plain C++: no renderer, runs headless.
	class EconomySim
	{
	public:
		EconomySim(ValleyDirector& director, GameState& state);
		EconomySim(const EconomySim&) = delete;
		EconomySim& operator=(const EconomySim&) = delete;

		const EconomyRollReport& GetLastRoll() const { return m_Last; }

	private:
		GameState& m_State;
		EconomyRollReport m_Last;
	};

}
