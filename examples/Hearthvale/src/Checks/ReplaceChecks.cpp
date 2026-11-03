#include "Checks/ReplaceChecks.h"
#include "Farm/GameState.h"
#include "Save/SaveGame.h"
#include "ValleyDirector.h"
#include "World/ValleyWorld.h"

#include <format>

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_OtherSeed = 12345;
	}

	bool ReplaceCheckRun::Verify(CheckReport& report, const ValleyDirector& director, const ValleyWorld& world, const char* label) const
	{
		const std::vector<Occupant>& occupants = director.GetGrid().GetOccupants();
		size_t missing = 0;
		for (const Occupant& occupant : occupants)
			missing += world.HasOccupantVisual(occupant.Id) ? 0 : 1;
		const bool ok = !occupants.empty() && missing == 0 && world.GetOccupantVisualCount() == occupants.size();
		report.Check(ok, std::format("Replace: after {} every occupant has an entity and none is left over ({} occupants, {} missing, {} entities)",
			label, occupants.size(), missing, world.GetOccupantVisualCount()));
		return ok;
	}

	bool ReplaceCheckRun::Finish(CheckReport& report)
	{
		const int passed = report.GetPassed() - m_PassedAtStart;
		const int failed = report.GetFailed() - m_FailedAtStart;
		report.AddSuite("Replace", passed, failed);
		DE_INFO("Replace checks: {} passed, {} failed", passed, failed);
		m_Step = Step::Done;
		return true;
	}

	bool ReplaceCheckRun::Tick(CheckReport& report, ValleyDirector& director, GameState& state, ValleyWorld& world, const std::function<void()>& replaced)
	{
		switch (m_Step)
		{
			case Step::Start:
			{
				m_PassedAtStart = report.GetPassed();
				m_FailedAtStart = report.GetFailed();
				m_OriginalSeed = director.GetSeed();
				Verify(report, director, world, "the launch valley");
				ApplyLoadedGame(MakeNewGame(m_OriginalSeed == k_OtherSeed ? k_OtherSeed + 1 : k_OtherSeed), director, state);
				replaced();
				m_Step = Step::Other;
				return false;
			}

			case Step::Other:
			{
				Verify(report, director, world, "a new game with another seed");
				ApplyLoadedGame(MakeNewGame(m_OriginalSeed), director, state);
				replaced();
				m_Step = Step::Back;
				return false;
			}

			case Step::Back:
			{
				Verify(report, director, world, "going back to the first seed");
				return Finish(report);
			}

			case Step::Done:
				return true;
		}
		return true;
	}

}
