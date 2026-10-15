#pragma once
#include "Defs/VillagerDefs.h"
#include "GameTuning.h"

#include <array>
#include <cstdint>

namespace Dingo
{

	enum class GiftOutcome : uint8_t { Loved, Fine, DailyLimit, NotGiftable };

	struct VillagerBond
	{
		int Points = 0;
		int LastTalkDay = -1;
		int LastGiftDay = -1;
		int GiftsOnLastGiftDay = 0;
	};

	// What each villager feels about the farmer (plan 2.2). Plain data like the rest of GameState; the day is passed in,
	// so the daily limits need no day-roll hook.
	class Friendship
	{
	public:
		int GetPoints(VillagerId villager) const { return Bond(villager).Points; }
		int GetHearts(VillagerId villager) const;
		const VillagerBond& Bond(VillagerId villager) const { return m_Bonds[static_cast<size_t>(villager)]; }

		bool HasTalkedToday(VillagerId villager, int absoluteDay) const { return Bond(villager).LastTalkDay == absoluteDay; }
		int GiftsToday(VillagerId villager, int absoluteDay) const;

		// Points gained: the first talk of a day pays, later ones do not.
		int Talk(VillagerId villager, int absoluteDay);

		// Whether the gift would be taken and how it lands; changes nothing.
		GiftOutcome Evaluate(VillagerId villager, ItemId item, int absoluteDay) const;
		// Applies an accepted gift and returns the points gained; a refused one gains 0.
		int Give(VillagerId villager, ItemId item, int absoluteDay);

		static bool Likes(VillagerId villager, ItemId item);

		uint64_t Hash() const;

	private:
		friend struct SaveCodec;

	private:
		VillagerBond& Bond(VillagerId villager) { return m_Bonds[static_cast<size_t>(villager)]; }
		void AddPoints(VillagerId villager, int points);

	private:
		std::array<VillagerBond, VILLAGER_COUNT> m_Bonds{};
	};

}
