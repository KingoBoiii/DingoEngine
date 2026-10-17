#include "Defs/DecorDefs.h"

#include <array>

namespace Dingo
{

	namespace
	{
		constexpr ItemId k_None = ItemId::Count;

		constexpr std::array<DecorDef, DECOR_KIND_COUNT> k_Decor = { {
			{ DecorKind::Tree,   "Tree",   false, ItemId::Axe,     ItemId::Wood,  3 },
			{ DecorKind::Pine,   "Pine",   false, ItemId::Axe,     ItemId::Wood,  4 },
			{ DecorKind::Bush,   "Bush",   false, ItemId::Scythe,  ItemId::Fibre, 2 },
			{ DecorKind::Rock,   "Rock",   false, ItemId::Pickaxe, ItemId::Stone, 2 },
			{ DecorKind::Stump,  "Stump",  false, ItemId::Axe,     ItemId::Wood,  1 },
			{ DecorKind::Weed,   "Weed",   false, ItemId::Scythe,  ItemId::Fibre, 1 },
			{ DecorKind::Grass,  "Grass",  true,  k_None,          k_None,        0 },
			{ DecorKind::Flower, "Flower", true,  k_None,          k_None,        0 },
		} };
	}

	std::span<const DecorDef> GetDecorDefs()
	{
		return k_Decor;
	}

	const DecorDef& GetDecorDef(DecorKind kind)
	{
		return k_Decor[static_cast<size_t>(kind)];
	}

}
