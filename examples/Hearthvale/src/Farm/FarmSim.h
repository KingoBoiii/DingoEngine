#pragma once
#include "Farm/Crops.h"
#include "ValleyDirector.h"

namespace Dingo
{

	// What the latest day roll did on the farm; for logs, the HUD and the checks.
	struct FarmRollReport
	{
		GrowReport Crops;
		int Dried = 0;
		int Sprinklered = 0;
		int Rained = 0;
		int CoinsLost = 0;
		DayRollReason Reason = DayRollReason::Sleep;
	};

	// The farm's part of the day roll, registered on the director's hooks: at the Crops step a pass-out
	// costs coins, then crops grow or wither and the soil dries; at Sprinklers the dawn water goes out for the new day;
	// at Weather a rainy new day waters the outdoor soil. Plain C++: no renderer, runs headless.
	class FarmSim
	{
	public:
		FarmSim(ValleyDirector& director, Inventory& inventory);
		FarmSim(const FarmSim&) = delete;
		FarmSim& operator=(const FarmSim&) = delete;

		const FarmRollReport& GetLastRoll() const { return m_Last; }

	private:
		ValleyDirector& m_Director;
		Inventory& m_Inventory;
		FarmRollReport m_Last;
	};

}
