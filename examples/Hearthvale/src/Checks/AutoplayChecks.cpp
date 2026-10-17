#include "Checks/Checks.h"
#include "Checks/AutoplayRig.h"
#include "Defs/BuildDefs.h"
#include "Sim/PerfWindow.h"
#include "World/ValleyLayout.h"

#include <format>

namespace Dingo
{

	namespace
	{
		int CountBuilt(const TileGrid& grid, BuildId id)
		{
			int count = 0;
			for (const Occupant& occupant : grid.GetOccupants())
			{
				if (occupant.Kind == OccupantKind::Building && occupant.DefId == static_cast<uint16_t>(id))
					++count;
			}
			return count;
		}
	}

	void RunAutoplayChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Autoplay");

		AutoplayRig rig(1);
		const int startCoins = rig.State.Pack.GetCoins();
		rig.PlayDays(28);
		const AutoplayStats& stats = rig.Bot.GetStats();

		DE_INFO("[Autoplay] 28 days: coins {} -> {}, {}", startCoins, rig.State.Pack.GetCoins(), FormatAutoplayStats(stats));

		report.Check(rig.DayLines.size() == 28, "Autoplay: 28 days log 28 [Day] lines");
		report.Check(rig.Director.GetCalendar().GetAbsoluteDay() == 28, "Autoplay: the bot reaches day 28 without a stall");
		report.Check(stats.Nights >= 27, "Autoplay: the bot goes to bed every night");
		report.Check(stats.Tilled > 0 && stats.Planted > 0 && stats.Watered > 0, "Autoplay: the bot tills, plants and waters");
		report.Check(stats.Harvested > 0, "Autoplay: the bot harvests");
		report.Check(stats.Shipped > 0, "Autoplay: the bot ships the harvest");
		report.Check(stats.SeedsBought > 0, "Autoplay: the bot buys seeds at the shop");
		report.Check(rig.State.Pack.GetCoins() > startCoins, "Autoplay: the bot earns more than it spends over 28 days");
		report.Check(CountBuilt(rig.Director.GetGrid(), BuildId::WoodFence) >= 10, "Autoplay: the bot builds a fence run of ten");
		report.Check(CountBuilt(rig.Director.GetGrid(), BuildId::LampPost) >= 1, "Autoplay: the bot builds a lamp");
		report.Check(stats.Gifts >= 1 && rig.State.Board.DoneCount() >= 1, "Autoplay: the bot delivers a gift the board asks for");
		report.Check(stats.Fences >= 10 && stats.Lamps >= 1, "Autoplay: the placements went through the build controller");

		const glm::vec2 feet = rig.Bot.GetFeet();
		const int tileX = static_cast<int>(std::floor(feet.x / TILE_SIZE));
		const int tileY = static_cast<int>(std::floor(feet.y / TILE_SIZE));
		report.Check(rig.Director.GetGrid().InBounds(tileX, tileY), "Autoplay: the bot ends on the map");

		PerfWindow window(3, 4);
		bool skipsWarmup = true;
		for (int frame = 0; frame < 3; ++frame)
		{
			PerfSample slow{};
			slow[static_cast<size_t>(PerfMetric::Frame)] = 100.0f;
			window.Add(slow);
			skipsWarmup = skipsWarmup && window.GetCounted() == 0;
		}
		for (int frame = 1; frame <= 4; ++frame)
		{
			PerfSample sample{};
			sample[static_cast<size_t>(PerfMetric::Frame)] = static_cast<float>(frame);
			window.Add(sample);
		}
		report.Check(skipsWarmup && window.IsDone() && window.GetCounted() == 4, "Perf: the window skips its warm-up frames and closes after the counted ones");
		report.Check(window.Mean(PerfMetric::Frame) == 2.5f && window.Max(PerfMetric::Frame) == 4.0f, "Perf: the window reports the mean and the max of the counted frames only");
	}

}
