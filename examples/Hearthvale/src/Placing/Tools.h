#pragma once
#include "Placing/Inventory.h"
#include "Defs/DecorDefs.h"
#include "World/TileGrid.h"
#include "World/ValleyLayout.h"

namespace Dingo
{

	enum class ToolResult : uint8_t
	{
		Nothing,        // no decoration on the tile
		NotMine,        // the hoe and the watering can belong to P6
		WrongTool,      // `Needed` clears it
		Cleared,
	};

	struct ToolOutcome
	{
		ToolResult Result = ToolResult::Nothing;
		DecorKind Kind = DecorKind::Tree;
		ItemId Needed = ItemId::Count;
		ItemId Yield = ItemId::Count;
		int YieldCount = 0;
		TileRect Dirty;
	};

	constexpr bool IsClearingTool(ItemId tool)
	{
		return tool == ItemId::Axe || tool == ItemId::Pickaxe || tool == ItemId::Scythe;
	}

	// One swing at the decoration on the tile: the right tool removes it and puts its yield in the inventory. The
	// scythe also clears cosmetic grass and flowers for nothing. Anything with another tool changes nothing.
	ToolOutcome UseClearingTool(TileGrid& grid, Inventory& inventory, ItemId tool, int x, int y);

}
