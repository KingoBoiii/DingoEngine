#pragma once
#include "Placing/Inventory.h"
#include "Defs/BuildDefs.h"
#include "Defs/DecorDefs.h"
#include "World/TileGrid.h"
#include "World/ValleyLayout.h"

namespace Dingo
{

	enum class PlaceResult : uint8_t { Ok, OutOfBounds, BadGround, Occupied, PlanLocked, CannotAfford };

	const char* ToString(PlaceResult result);

	// `X`, `Y` is the footprint's minimum corner; the footprint is the def's size turned `Rotation` quarter turns.
	struct PlaceSpec
	{
		BuildId Id = BuildId::WoodFence;
		int X = 0;
		int Y = 0;
		int Rotation = 0;
	};

	struct PlaceOutcome
	{
		PlaceResult Result = PlaceResult::Ok;
		uint32_t OccupantId = k_NoOccupant;   // k_NoOccupant for a ground-only buildable
		int ClearedDecor = 0;
		TileRect Dirty;                       // every tile the edit touched; empty when it failed
	};

	struct RemoveOutcome
	{
		bool Removed = false;
		bool InventoryFull = false;           // the refund would not fit, so nothing was removed
		BuildId Id = BuildId::WoodFence;
		BuildCost Refund;
		TileRect Dirty;
	};

	TileRect FootprintOf(const PlaceSpec& spec);

	bool CanAfford(const Inventory& inventory, const BuildCost& cost);

	// The share of the cost given back on removal, per resource, rounded down.
	BuildCost RefundFor(const BuildDef& def);

	// The rules, with no side effects: bounds, ground, occupants (cosmetic decoration does not count), crops, the
	// building plan (a def with a PlanPrice must have been bought), and the cost. The first failing rule wins, in
	// that order.
	PlaceResult CheckPlacement(const TileGrid& grid, const Inventory& inventory, const PlaceSpec& spec);

	// Clears cosmetic decoration under the footprint for free, charges the cost, then adds the occupant (or sets the
	// ground to Path for a ground-only buildable). Changes and charges nothing unless CheckPlacement says Ok.
	PlaceOutcome PlaceBuilding(TileGrid& grid, Inventory& inventory, const PlaceSpec& spec);

	// Removes the player's building covering the tile, or the player's own path on it, and refunds its share. Generated
	// paths, decoration, landmarks and anything with a crop are left alone. A refund that does not fit the inventory
	// removes nothing and sets InventoryFull.
	RemoveOutcome RemoveBuilding(TileGrid& grid, Inventory& inventory, int x, int y);

	TileRect UnionOf(const TileRect& a, const TileRect& b);

}
