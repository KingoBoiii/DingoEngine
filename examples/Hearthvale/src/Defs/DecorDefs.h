#pragma once
#include "Defs/ItemDefs.h"

#include <cstdint>
#include <span>

namespace Dingo
{

	enum class DecorKind : uint8_t { Tree, Pine, Bush, Rock, Stump, Weed, Grass, Flower, Count };

	inline constexpr int DECOR_KIND_COUNT = static_cast<int>(DecorKind::Count);
	inline constexpr int DECOR_VARIANTS = 3;

	struct DecorDef
	{
		DecorKind Kind;
		const char* Name;
		bool Cosmetic;      // building or tilling over it clears it for free
		ItemId Tool;        // the tool that clears it; ItemId::Count = none
		ItemId Yield;
		int YieldCount;
	};

	std::span<const DecorDef> GetDecorDefs();
	const DecorDef& GetDecorDef(DecorKind kind);

}
