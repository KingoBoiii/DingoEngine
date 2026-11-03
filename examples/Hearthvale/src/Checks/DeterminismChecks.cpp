#include "Checks/Checks.h"
#include "Checks/AutoplayRig.h"

namespace Dingo
{

	void RunDeterminismChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Determinism");

		constexpr int days = 7;
		AutoplayRig first(1);
		AutoplayRig second(1);
		first.PlayDays(days);
		second.PlayDays(days);

		report.Check(first.DayLines.size() == static_cast<size_t>(days), "Determinism: seven days log seven [Day] lines");
		report.Check(first.DayLines == second.DayLines, "Determinism: two runs of --days=7 --seed=1 log the same [Day] lines");
		report.Check(HashGameState(first.Director, first.State).All == HashGameState(second.Director, second.State).All,
			"Determinism: the final state hashes match");
		report.Check(first.Bot.GetStats().Planted == second.Bot.GetStats().Planted && first.Bot.GetStats().Shipped == second.Bot.GetStats().Shipped,
			"Determinism: the two farmers did the same work");

		AutoplayRig other(2);
		other.PlayDays(days);
		report.Check(other.DayLines != first.DayLines, "Determinism: another seed plays a different week");
	}

}
