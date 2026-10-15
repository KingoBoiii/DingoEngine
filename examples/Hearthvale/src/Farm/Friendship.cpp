#include "Farm/Friendship.h"
#include "Sim/Fnv.h"

#include <algorithm>

namespace Dingo
{

	int Friendship::GetHearts(VillagerId villager) const
	{
		return (std::min)(Bond(villager).Points / FRIENDSHIP_POINTS_PER_HEART, FRIENDSHIP_HEARTS_MAX);
	}

	int Friendship::GiftsToday(VillagerId villager, int absoluteDay) const
	{
		const VillagerBond& bond = Bond(villager);
		return bond.LastGiftDay == absoluteDay ? bond.GiftsOnLastGiftDay : 0;
	}

	void Friendship::AddPoints(VillagerId villager, int points)
	{
		VillagerBond& bond = Bond(villager);
		bond.Points = (std::min)(bond.Points + points, FRIENDSHIP_POINTS_PER_HEART * FRIENDSHIP_HEARTS_MAX);
	}

	int Friendship::Talk(VillagerId villager, int absoluteDay)
	{
		if (HasTalkedToday(villager, absoluteDay))
			return 0;
		const int before = Bond(villager).Points;
		AddPoints(villager, FRIENDSHIP_TALK_POINTS);
		Bond(villager).LastTalkDay = absoluteDay;
		return Bond(villager).Points - before;
	}

	bool Friendship::Likes(VillagerId villager, ItemId item)
	{
		for (ItemId liked : GetVillagerDef(villager).Liked)
		{
			if (liked != ItemId::Count && liked == item)
				return true;
		}
		return false;
	}

	GiftOutcome Friendship::Evaluate(VillagerId villager, ItemId item, int absoluteDay) const
	{
		if (item >= ItemId::Count || GetItemDef(item).Kind == ItemKind::Tool)
			return GiftOutcome::NotGiftable;
		if (GiftsToday(villager, absoluteDay) >= FRIENDSHIP_GIFTS_PER_DAY)
			return GiftOutcome::DailyLimit;
		return Likes(villager, item) ? GiftOutcome::Loved : GiftOutcome::Fine;
	}

	int Friendship::Give(VillagerId villager, ItemId item, int absoluteDay)
	{
		const GiftOutcome outcome = Evaluate(villager, item, absoluteDay);
		if (outcome != GiftOutcome::Loved && outcome != GiftOutcome::Fine)
			return 0;

		VillagerBond& bond = Bond(villager);
		bond.GiftsOnLastGiftDay = GiftsToday(villager, absoluteDay) + 1;
		bond.LastGiftDay = absoluteDay;

		const int before = bond.Points;
		AddPoints(villager, outcome == GiftOutcome::Loved ? FRIENDSHIP_GIFT_LIKED : FRIENDSHIP_GIFT_FINE);
		return bond.Points - before;
	}

	uint64_t Friendship::Hash() const
	{
		Fnv1a hash;
		for (const VillagerBond& bond : m_Bonds)
		{
			hash.I32(bond.Points);
			hash.I32(bond.LastTalkDay);
			hash.I32(bond.LastGiftDay);
			hash.I32(bond.GiftsOnLastGiftDay);
		}
		return hash.Value();
	}

}
