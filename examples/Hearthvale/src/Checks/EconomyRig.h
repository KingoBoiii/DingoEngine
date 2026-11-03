#pragma once
#include "Farm/Calendar.h"
#include "Farm/EconomySim.h"
#include "Farm/FarmSim.h"
#include "Farm/GameState.h"
#include "ValleyDirector.h"

namespace Dingo
{

	// A director with the farm and economy hooks on and an empty pack: the headless rig of the economy and board checks.
	struct EconomyRig
	{
		ValleyDirector Director;
		GameState State;
		FarmSim Farm;
		EconomySim Economy;

		explicit EconomyRig(int startDay = 0, uint32_t seed = 1)
			: Director(MakeConfig(startDay, seed)), Farm(Director, State.Pack), Economy(Director, State)
		{}

		static DirectorConfig MakeConfig(int startDay, uint32_t seed)
		{
			DirectorConfig config;
			config.Seed = seed;
			config.StartDay = startDay;
			return config;
		}

		int Day() const { return Director.GetCalendar().GetAbsoluteDay(); }

		void Sleep()
		{
			Director.RequestSleep();
			Director.Step();
		}

		void PassOut()
		{
			Director.GetCalendar().Reset(Day(), DAY_TICKS - 1);
			Director.Step();
		}
	};

}
