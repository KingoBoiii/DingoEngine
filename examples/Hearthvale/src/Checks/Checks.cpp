#include "Checks/Checks.h"

namespace Dingo
{

	void RunSimChecks(CheckReport& report)
	{
		RunDefChecks(report);
		RunGenerationChecks(report);
		RunCalendarChecks(report);
		RunWorldChecks(report);
		RunSkyChecks(report);
		RunPlayerChecks(report);
		RunPlacementChecks(report);
		RunGrowthChecks(report);
		RunEconomyChecks(report);
		RunBoardChecks(report);
		RunUiLayoutChecks(report);
		RunMenuChecks(report);
		RunLifeChecks(report);
	}

}
