#pragma once
#include "Defs/VillagerDefs.h"
#include "Farm/Friendship.h"
#include "Sim/SimTypes.h"

#include <span>

namespace Dingo
{

	inline constexpr int LINE_ANY = -1;

	// One thing a villager can say. Season and Weather are LINE_ANY or the enum value; the heart range gates it by how
	// well the farmer knows them.
	struct VillagerLine
	{
		const char* Text;
		int Season;
		int Weather;
		int MinHearts;
		int MaxHearts;
	};

	struct LineContext
	{
		Dingo::Season Season = Dingo::Season::Spring;
		Dingo::Weather Weather = Dingo::Weather::Clear;
		int Hearts = 0;
		int Day = 0;
		// Which talk of the day this is, so a second chat is not a repeat of the first.
		int Talk = 0;
	};

	std::span<const VillagerLine> GetVillagerLines(VillagerId villager);
	bool LineMatches(const VillagerLine& line, const LineContext& context);
	int CountMatchingLines(VillagerId villager, const LineContext& context);

	// A deterministic pick among the lines that match; never empty.
	const char* PickVillagerLine(VillagerId villager, const LineContext& context);

	const char* GiftReaction(VillagerId villager, GiftOutcome outcome);

}
