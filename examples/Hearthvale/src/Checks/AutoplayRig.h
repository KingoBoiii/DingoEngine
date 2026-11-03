#pragma once
#include "Checks/Autoplay.h"
#include "Farm/Animals.h"
#include "Farm/EconomySim.h"
#include "Farm/FarmSim.h"
#include "Farm/GameState.h"
#include "Farm/SleepFlow.h"
#include "Life/VillagerSim.h"
#include "Placing/BuildController.h"
#include "ValleyDirector.h"

#include <string>
#include <vector>

namespace Dingo
{

	// The sim the layer wires, minus the window: a director with the farm, economy, villagers, animals and sleep
	// hooks, a build controller and the autoplay farmer in the tools stage. `--days` in the layer and the
	// determinism and autoplay checks all play it the same way.
	struct AutoplayRig
	{
		ValleyDirector Director;
		GameState State;
		FarmSim Farm;
		EconomySim Economy;
		VillagerSim Villagers;
		AnimalSim Animals;
		SleepFlow Sleep;
		BuildController Build;
		AutoplayFarmer Bot;
		std::vector<std::string> DayLines;

		explicit AutoplayRig(uint32_t seed = 1)
			: Director(MakeConfig(seed)), Farm(Director, State.Pack), Economy(Director, State), Villagers(Director),
			  Animals(Director, State.Animals), Sleep(Director),
			  Build(Director.GetGrid(), State.Pack, [this](const TileRect&) { Villagers.InvalidatePlans(); }),
			  Bot(Director, State, Build, Sleep, &Villagers)
		{
			Build.SetCalendar(&Director.GetCalendar());
			Build.SetSleepHandler([this] { Sleep.RequestSleep(); });
			State.Pack.GiveStarterLoadout();
			Director.OnStage(SimStage::ToolsAndPlacement, [this](ValleyDirector&, const StepInfo&) { Bot.Step(); });
			Director.OnDayRoll(DayRollStep::Autosave, [this](ValleyDirector&, DayRollReason) { DayLines.push_back(FormatDayLine(Director, State)); });
		}

		void PlayDays(int days)
		{
			const int target = Director.GetCalendar().GetAbsoluteDay() + days;
			while (Director.GetCalendar().GetAbsoluteDay() < target)
				Director.Step();
		}

		static DirectorConfig MakeConfig(uint32_t seed)
		{
			DirectorConfig config;
			config.Seed = seed;
			return config;
		}
	};

}
