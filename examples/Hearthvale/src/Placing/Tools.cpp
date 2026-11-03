#include "Placing/Tools.h"

namespace Dingo
{

	ToolOutcome UseClearingTool(TileGrid& grid, Inventory& inventory, ItemId tool, int x, int y)
	{
		ToolOutcome outcome;
		if (!IsClearingTool(tool))
		{
			outcome.Result = ToolResult::NotMine;
			return outcome;
		}
		if (!grid.InBounds(x, y))
			return outcome;

		const Occupant* occupant = grid.FindOccupant(grid.At(x, y).Occupant);
		if (!occupant || occupant->Kind != OccupantKind::Decoration || occupant->DefId >= DECOR_KIND_COUNT)
			return outcome;

		const DecorDef& def = GetDecorDef(static_cast<DecorKind>(occupant->DefId));
		outcome.Kind = def.Kind;
		outcome.Needed = def.Cosmetic ? ItemId::Scythe : def.Tool;
		const bool right = def.Tool == tool || (def.Cosmetic && tool == ItemId::Scythe);
		if (!right)
		{
			outcome.Result = ToolResult::WrongTool;
			return outcome;
		}

		const bool yields = def.Yield != ItemId::Count && def.YieldCount > 0;
		if (yields && !inventory.CanAdd(def.Yield, def.YieldCount))
		{
			outcome.Yield = def.Yield;
			outcome.YieldCount = def.YieldCount;
			outcome.Result = ToolResult::InventoryFull;
			return outcome;
		}

		outcome.Dirty = TileRect{ occupant->X, occupant->Y, occupant->W, occupant->H };
		const uint32_t id = occupant->Id;
		if (yields)
		{
			outcome.Yield = def.Yield;
			outcome.YieldCount = def.YieldCount;
			inventory.Add(def.Yield, def.YieldCount);
		}
		grid.RemoveOccupant(id);
		outcome.Result = ToolResult::Cleared;
		return outcome;
	}

}
