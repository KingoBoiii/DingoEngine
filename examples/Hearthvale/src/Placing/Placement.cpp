#include "Placing/Placement.h"

#include <algorithm>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr bool IsEmptyRect(const TileRect& rect) { return rect.W <= 0 || rect.H <= 0; }

		bool IsCosmeticDecoration(const Occupant& occupant)
		{
			return occupant.Kind == OccupantKind::Decoration && occupant.DefId < DECOR_KIND_COUNT
				&& GetDecorDef(static_cast<DecorKind>(occupant.DefId)).Cosmetic;
		}

		struct Cost
		{
			ItemId Item;
			int BuildCost::* Member;
		};

		constexpr Cost k_Costs[] = {
			{ ItemId::Wood, &BuildCost::Wood },
			{ ItemId::Stone, &BuildCost::Stone },
			{ ItemId::Fibre, &BuildCost::Fibre },
		};

		void Charge(Inventory& inventory, const BuildCost& cost)
		{
			inventory.SpendCoins(cost.Coins);
			for (const Cost& entry : k_Costs)
				inventory.Remove(entry.Item, cost.*entry.Member);
		}

		void Give(Inventory& inventory, const BuildCost& cost)
		{
			inventory.AddCoins(cost.Coins);
			for (const Cost& entry : k_Costs)
				inventory.Add(entry.Item, cost.*entry.Member);
		}

		bool CanTake(const Inventory& inventory, const BuildCost& cost)
		{
			ItemStack stacks[std::size(k_Costs)];
			for (size_t i = 0; i < std::size(k_Costs); ++i)
				stacks[i] = ItemStack{ k_Costs[i].Item, cost.*k_Costs[i].Member };
			return inventory.CanAddAll(stacks);
		}

		bool IsPathable(Ground ground)
		{
			return ground == Ground::Grass || ground == Ground::Tilled || ground == Ground::Watered;
		}

		// Whether anything but cosmetic decoration stands on the tile.
		bool IsHeld(const TileGrid& grid, int x, int y)
		{
			const Tile& tile = grid.At(x, y);
			if (tile.Crop.Present())
				return true;
			if (tile.Occupant == k_NoOccupant)
				return false;
			const Occupant* occupant = grid.FindOccupant(tile.Occupant);
			return !occupant || !IsCosmeticDecoration(*occupant);
		}
	}

	const char* ToString(PlaceResult result)
	{
		switch (result)
		{
			case PlaceResult::Ok:           return "Ok";
			case PlaceResult::OutOfBounds:  return "Out of bounds";
			case PlaceResult::BadGround:    return "Can't build on this ground";
			case PlaceResult::Occupied:     return "Something is in the way";
			case PlaceResult::PlanLocked:   return "Buy the plan at the carpenter";
			case PlaceResult::CannotAfford: return "Not enough materials";
		}
		return "";
	}

	TileRect UnionOf(const TileRect& a, const TileRect& b)
	{
		if (IsEmptyRect(a))
			return b;
		if (IsEmptyRect(b))
			return a;
		const int x = (std::min)(a.X, b.X);
		const int y = (std::min)(a.Y, b.Y);
		return TileRect{ x, y, (std::max)(a.Right(), b.Right()) - x, (std::max)(a.Bottom(), b.Bottom()) - y };
	}

	TileRect FootprintOf(const PlaceSpec& spec)
	{
		const Footprint footprint = RotatedFootprint(GetBuildDef(spec.Id), spec.Rotation);
		return TileRect{ spec.X, spec.Y, footprint.W, footprint.H };
	}

	bool CanAfford(const Inventory& inventory, const BuildCost& cost)
	{
		if (inventory.GetCoins() < cost.Coins)
			return false;
		for (const Cost& entry : k_Costs)
		{
			if (inventory.Count(entry.Item) < cost.*entry.Member)
				return false;
		}
		return true;
	}

	BuildCost RefundFor(const BuildDef& def)
	{
		BuildCost refund;
		refund.Coins = def.Cost.Coins * def.RefundPermille / 1000;
		refund.Wood = def.Cost.Wood * def.RefundPermille / 1000;
		refund.Stone = def.Cost.Stone * def.RefundPermille / 1000;
		refund.Fibre = def.Cost.Fibre * def.RefundPermille / 1000;
		return refund;
	}

	PlaceResult CheckPlacement(const TileGrid& grid, const Inventory& inventory, const PlaceSpec& spec)
	{
		const BuildDef& def = GetBuildDef(spec.Id);
		const TileRect area = FootprintOf(spec);
		if (area.W <= 0 || area.H <= 0 || !grid.InBounds(area.X, area.Y) || !grid.InBounds(area.Right() - 1, area.Bottom() - 1))
			return PlaceResult::OutOfBounds;

		for (int y = area.Y; y < area.Bottom(); ++y)
		{
			for (int x = area.X; x < area.Right(); ++x)
			{
				const Ground ground = grid.GetGround(x, y);
				if (def.GroundOnly ? !(IsPathable(ground) || ground == Ground::Path) : !CanOccupy(ground))
					return PlaceResult::BadGround;
			}
		}

		for (int y = area.Y; y < area.Bottom(); ++y)
		{
			for (int x = area.X; x < area.Right(); ++x)
			{
				if (IsHeld(grid, x, y) || (def.GroundOnly && grid.GetGround(x, y) == Ground::Path))
					return PlaceResult::Occupied;
			}
		}

		if (!inventory.KnowsPlan(spec.Id))
			return PlaceResult::PlanLocked;
		return CanAfford(inventory, def.Cost) ? PlaceResult::Ok : PlaceResult::CannotAfford;
	}

	PlaceOutcome PlaceBuilding(TileGrid& grid, Inventory& inventory, const PlaceSpec& spec)
	{
		PlaceOutcome outcome;
		outcome.Result = CheckPlacement(grid, inventory, spec);
		if (outcome.Result != PlaceResult::Ok)
			return outcome;

		const BuildDef& def = GetBuildDef(spec.Id);
		const TileRect area = FootprintOf(spec);
		outcome.Dirty = area;

		std::vector<uint32_t> cosmetic;
		for (int y = area.Y; y < area.Bottom(); ++y)
		{
			for (int x = area.X; x < area.Right(); ++x)
			{
				const uint32_t id = grid.At(x, y).Occupant;
				if (id != k_NoOccupant && std::find(cosmetic.begin(), cosmetic.end(), id) == cosmetic.end())
					cosmetic.push_back(id);
			}
		}
		for (const uint32_t id : cosmetic)
		{
			if (const Occupant* occupant = grid.FindOccupant(id))
				outcome.Dirty = UnionOf(outcome.Dirty, TileRect{ occupant->X, occupant->Y, occupant->W, occupant->H });
			grid.RemoveOccupant(id);
			++outcome.ClearedDecor;
		}

		Charge(inventory, def.Cost);
		if (def.GroundOnly)
		{
			for (int y = area.Y; y < area.Bottom(); ++y)
			{
				for (int x = area.X; x < area.Right(); ++x)
				{
					Tile& tile = grid.At(x, y);
					tile.GroundState = Ground::Path;
					tile.Built = static_cast<uint8_t>(static_cast<int>(spec.Id) + 1);
				}
			}
		}
		else
		{
			outcome.OccupantId = grid.AddOccupant(OccupantKind::Building, static_cast<uint16_t>(spec.Id), area.X, area.Y, area.W, area.H, spec.Rotation, 0);
		}
		return outcome;
	}

	RemoveOutcome RemoveBuilding(TileGrid& grid, Inventory& inventory, int x, int y)
	{
		RemoveOutcome outcome;
		if (!grid.InBounds(x, y))
			return outcome;

		const Tile& tile = grid.At(x, y);
		if (tile.Occupant != k_NoOccupant)
		{
			const Occupant* occupant = grid.FindOccupant(tile.Occupant);
			if (!occupant || occupant->Kind != OccupantKind::Building)
				return outcome;
			const BuildId id = static_cast<BuildId>(occupant->DefId);
			outcome.Id = id;
			outcome.Dirty = TileRect{ occupant->X, occupant->Y, occupant->W, occupant->H };
			outcome.Refund = RefundFor(GetBuildDef(id));
			if (!CanTake(inventory, outcome.Refund))
			{
				outcome.Refund = BuildCost{};
				outcome.Dirty = TileRect{};
				outcome.InventoryFull = true;
				return outcome;
			}
			grid.RemoveOccupant(tile.Occupant);
			Give(inventory, outcome.Refund);
			outcome.Removed = true;
			return outcome;
		}

		if (tile.Built != 0 && tile.GroundState == Ground::Path && !tile.Crop.Present())
		{
			const BuildId id = static_cast<BuildId>(tile.Built - 1);
			outcome.Id = id;
			outcome.Dirty = TileRect{ x, y, 1, 1 };
			outcome.Refund = RefundFor(GetBuildDef(id));
			if (!CanTake(inventory, outcome.Refund))
			{
				outcome.Refund = BuildCost{};
				outcome.Dirty = TileRect{};
				outcome.InventoryFull = true;
				return outcome;
			}
			Tile& edit = grid.At(x, y);
			edit.GroundState = Ground::Grass;
			edit.Built = 0;
			Give(inventory, outcome.Refund);
			outcome.Removed = true;
		}
		return outcome;
	}

}
