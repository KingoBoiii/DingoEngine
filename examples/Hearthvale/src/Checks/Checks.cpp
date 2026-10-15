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
	}

}
