#pragma once
#include "Defs/ItemDefs.h"
#include "World/ValleyLayout.h"

#include <array>
#include <cstdint>
#include <span>

namespace Dingo
{

	enum class VillagerId : uint8_t { Shopkeeper, Carpenter, Mayor, Child, Count };

	inline constexpr int VILLAGER_COUNT = static_cast<int>(VillagerId::Count);
	inline constexpr int VILLAGER_LIKED_MAX = 4;

	struct VillagerDef
	{
		VillagerId Id;
		const char* Name;
		const char* Role;
		Landmark Home;
		std::array<ItemId, VILLAGER_LIKED_MAX> Liked;   // ItemId::Count pads unused slots
	};

	std::span<const VillagerDef> GetVillagerDefs();
	const VillagerDef& GetVillagerDef(VillagerId id);

}
