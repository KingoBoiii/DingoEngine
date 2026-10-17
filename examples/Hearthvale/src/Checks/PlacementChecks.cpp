#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Placing/Placement.h"
#include "Placing/Targeting.h"
#include "Placing/Tools.h"
#include "Checks/Checks.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"
#include "World/Valley.h"
#include "World/WorldCells.h"

#include <cmath>
#include <format>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr int k_Size = 32;

		struct Fixture
		{
			TileGrid Grid{ k_Size, k_Size };
			Inventory Stock;

			Fixture()
			{
				Stock.AddCoins(5000);
				Stock.LearnAllPlans();
				for (const ItemId tool : BUILD_TOOLS)
					Stock.Add(tool, 1);
				for (const ItemId item : { ItemId::Wood, ItemId::Stone, ItemId::Fibre })
					Stock.Add(item, 297);
			}
		};

		struct Totals
		{
			int Coins, Wood, Stone, Fibre;

			bool operator==(const Totals&) const = default;
		};

		Totals TotalsOf(const Inventory& stock)
		{
			return Totals{ stock.GetCoins(), stock.Count(ItemId::Wood), stock.Count(ItemId::Stone), stock.Count(ItemId::Fibre) };
		}

		uint32_t AddDecor(TileGrid& grid, DecorKind kind, int x, int y)
		{
			return grid.AddOccupant(OccupantKind::Decoration, static_cast<uint16_t>(kind), x, y, 1, 1, 0, 0);
		}

		int QueuedCells(const TileRect& area)
		{
			DirtyCells cells(k_Size, k_Size, CELL_TILES);
			cells.MarkArea(area.X, area.Y, area.W, area.H);
			return static_cast<int>(cells.Drain().size());
		}

		void CheckFreeTile(CheckReport& report)
		{
			Fixture f;
			const BuildDef& def = GetBuildDef(BuildId::Scarecrow);
			const Totals before = TotalsOf(f.Stock);
			const PlaceOutcome outcome = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Scarecrow, 5, 5, 0 });
			const Totals after = TotalsOf(f.Stock);
			report.Check(outcome.Result == PlaceResult::Ok && outcome.OccupantId != k_NoOccupant && f.Grid.At(5, 5).Occupant == outcome.OccupantId,
				"Placement: a free tile takes the building");
			report.Check(before.Coins - after.Coins == def.Cost.Coins && before.Wood - after.Wood == def.Cost.Wood
				&& before.Stone - after.Stone == def.Cost.Stone && before.Fibre - after.Fibre == def.Cost.Fibre,
				"Placement: placing charges exactly the def's cost");
			const Occupant* occupant = f.Grid.FindOccupant(outcome.OccupantId);
			report.Check(occupant && occupant->Kind == OccupantKind::Building && occupant->DefId == static_cast<uint16_t>(BuildId::Scarecrow)
				&& occupant->X == 5 && occupant->Y == 5 && occupant->W == 1 && occupant->H == 1,
				"Placement: the occupant records the def and the footprint");
			report.Check(outcome.Dirty.X == 5 && outcome.Dirty.Y == 5 && outcome.Dirty.W == 1 && outcome.Dirty.H == 1, "Placement: the edit reports the tile it touched");

			bool everyBuildable = true;
			for (int build = 0; build < BUILD_COUNT; ++build)
			{
				Fixture g;
				const BuildDef& each = GetBuildDef(static_cast<BuildId>(build));
				const Totals start = TotalsOf(g.Stock);
				const PlaceOutcome placed = PlaceBuilding(g.Grid, g.Stock, PlaceSpec{ each.Id, 8, 8, 0 });
				const Totals end = TotalsOf(g.Stock);
				everyBuildable = everyBuildable && placed.Result == PlaceResult::Ok && start.Coins - end.Coins == each.Cost.Coins
					&& start.Wood - end.Wood == each.Cost.Wood && start.Stone - end.Stone == each.Cost.Stone && start.Fibre - end.Fibre == each.Cost.Fibre;
			}
			report.Check(everyBuildable, "Placement: every one of the 20 buildables places on open grass and charges its own cost");
		}

		void CheckRefusals(CheckReport& report)
		{
			Fixture f;
			f.Grid.SetGround(3, 3, Ground::Water);
			f.Grid.SetGround(4, 3, Ground::Blocked);
			PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::WoodFence, 6, 6, 0 });
			AddDecor(f.Grid, DecorKind::Tree, 8, 8);
			f.Grid.At(10, 10).Crop.Def = 1;

			const uint64_t hash = f.Grid.Hash();
			const Totals totals = TotalsOf(f.Stock);

			struct Case
			{
				const char* Name;
				PlaceSpec Spec;
				PlaceResult Expected;
			};
			const Case cases[] = {
				{ "water", { BuildId::Scarecrow, 3, 3, 0 }, PlaceResult::BadGround },
				{ "blocked ground", { BuildId::Scarecrow, 4, 3, 0 }, PlaceResult::BadGround },
				{ "a building", { BuildId::Scarecrow, 6, 6, 0 }, PlaceResult::Occupied },
				{ "a tree", { BuildId::Scarecrow, 8, 8, 0 }, PlaceResult::Occupied },
				{ "a crop", { BuildId::Scarecrow, 10, 10, 0 }, PlaceResult::Occupied },
				{ "the west edge", { BuildId::Scarecrow, -1, 5, 0 }, PlaceResult::OutOfBounds },
				{ "the north edge", { BuildId::Scarecrow, 5, -1, 0 }, PlaceResult::OutOfBounds },
				{ "the east edge", { BuildId::Scarecrow, k_Size, 5, 0 }, PlaceResult::OutOfBounds },
				{ "the south edge", { BuildId::Scarecrow, 5, k_Size, 0 }, PlaceResult::OutOfBounds },
				{ "a footprint over the edge", { BuildId::Barn, k_Size - 2, 5, 0 }, PlaceResult::OutOfBounds },
				{ "a path on water", { BuildId::DirtPath, 3, 3, 0 }, PlaceResult::BadGround },
				{ "a path under a tree", { BuildId::DirtPath, 8, 8, 0 }, PlaceResult::Occupied },
			};
			for (const Case& c : cases)
			{
				const PlaceOutcome outcome = PlaceBuilding(f.Grid, f.Stock, c.Spec);
				report.Check(outcome.Result == c.Expected && outcome.OccupantId == k_NoOccupant && outcome.Dirty.W == 0
					&& f.Grid.Hash() == hash && TotalsOf(f.Stock) == totals,
					std::format("Placement: {} refuses, changes nothing and charges nothing", c.Name));
			}

			Inventory poor;
			poor.AddCoins(49);
			poor.Add(ItemId::Wood, 99);
			poor.Add(ItemId::Fibre, 99);
			TileGrid empty(k_Size, k_Size);
			const uint64_t emptyHash = empty.Hash();
			const PlaceOutcome broke = PlaceBuilding(empty, poor, PlaceSpec{ BuildId::Scarecrow, 5, 5, 0 });
			report.Check(broke.Result == PlaceResult::CannotAfford && empty.Hash() == emptyHash && poor.GetCoins() == 49 && poor.Count(ItemId::Wood) == 99,
				"Placement: one coin short refuses and charges nothing");
			Inventory noWood;
			noWood.AddCoins(5000);
			noWood.Add(ItemId::Stone, 99);
			noWood.Add(ItemId::Fibre, 99);
			report.Check(PlaceBuilding(empty, noWood, PlaceSpec{ BuildId::Chest, 5, 5, 0 }).Result == PlaceResult::CannotAfford && empty.Hash() == emptyHash,
				"Placement: a missing resource refuses even with coins to spare");

			report.Check(CheckPlacement(f.Grid, f.Stock, PlaceSpec{ BuildId::Scarecrow, 20, 20, 0 }) == PlaceResult::Ok && f.Grid.Hash() == hash && TotalsOf(f.Stock) == totals,
				"Placement: CheckPlacement answers without changing anything");
		}

		void CheckRemoval(CheckReport& report)
		{
			bool shares = true;
			for (int build = 0; build < BUILD_COUNT; ++build)
			{
				Fixture f;
				const BuildDef& def = GetBuildDef(static_cast<BuildId>(build));
				const uint64_t blank = f.Grid.Hash();
				const Totals start = TotalsOf(f.Stock);
				const PlaceOutcome placed = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ def.Id, 12, 12, 0 });
				const RemoveOutcome removed = RemoveBuilding(f.Grid, f.Stock, 12, 12);
				const Totals end = TotalsOf(f.Stock);
				const int permille = def.RefundPermille;
				shares = shares && placed.Result == PlaceResult::Ok && removed.Removed && removed.Id == def.Id
					&& start.Coins - end.Coins == def.Cost.Coins - def.Cost.Coins * permille / 1000
					&& start.Wood - end.Wood == def.Cost.Wood - def.Cost.Wood * permille / 1000
					&& start.Stone - end.Stone == def.Cost.Stone - def.Cost.Stone * permille / 1000
					&& start.Fibre - end.Fibre == def.Cost.Fibre - def.Cost.Fibre * permille / 1000
					&& f.Grid.Hash() == blank;
			}
			report.Check(shares, "Placement: removing refunds the def's share of every resource, rounded down, and restores the grid");

			Fixture f;
			PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Scarecrow, 2, 2, 0 });
			const Totals built = TotalsOf(f.Stock);
			const RemoveOutcome removed = RemoveBuilding(f.Grid, f.Stock, 2, 2);
			const Totals after = TotalsOf(f.Stock);
			report.Check(removed.Removed && removed.Refund.Coins == 25 && removed.Refund.Wood == 2 && removed.Refund.Stone == 0 && removed.Refund.Fibre == 2
				&& after.Coins - built.Coins == 25 && after.Wood - built.Wood == 2 && after.Fibre - built.Fibre == 2,
				"Placement: a scarecrow (50 coins, 4 wood, 4 fibre) gives back 25, 2 and 2 at the default half share");
			report.Check(GetBuildDef(BuildId::Scarecrow).RefundPermille == BUILD_REFUND_PERMILLE, "Placement: buildables take the tuned refund share");

			PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::WoodFence, 3, 3, 0 });
			AddDecor(f.Grid, DecorKind::Tree, 9, 9);
			const uint64_t hash = f.Grid.Hash();
			const Totals totals = TotalsOf(f.Stock);
			const bool onTree = RemoveBuilding(f.Grid, f.Stock, 9, 9).Removed;
			const bool onEmpty = RemoveBuilding(f.Grid, f.Stock, 20, 20).Removed;
			const bool outside = RemoveBuilding(f.Grid, f.Stock, -3, 4).Removed;
			report.Check(!onTree && !onEmpty && !outside && f.Grid.Hash() == hash && TotalsOf(f.Stock) == totals, "Placement: removing decoration, an empty tile or a tile outside the map does nothing");

			TileGrid valley(VALLEY_SIZE, VALLEY_SIZE);
			GenerateValley(valley, 1);
			const uint64_t valleyHash = valley.Hash();
			Inventory stock;
			bool landmarkSafe = true;
			for (const LandmarkSpec& spec : k_Landmarks)
				landmarkSafe = landmarkSafe && !RemoveBuilding(valley, stock, spec.Footprint.X, spec.Footprint.Y).Removed;
			report.Check(landmarkSafe && valley.Hash() == valleyHash && stock.GetCoins() == 0, "Placement: landmarks cannot be removed");
		}

		void CheckFootprints(CheckReport& report)
		{
			bool allTiles = true;
			bool allBlockers = true;
			for (const BuildId id : { BuildId::Bench, BuildId::Coop, BuildId::Barn, BuildId::Greenhouse })
			{
				const BuildDef& def = GetBuildDef(id);
				for (int j = 0; j < def.Height; ++j)
				{
					for (int i = 0; i < def.Width; ++i)
					{
						Fixture water;
						water.Grid.SetGround(10 + i, 10 + j, Ground::Water);
						allTiles = allTiles && PlaceBuilding(water.Grid, water.Stock, PlaceSpec{ id, 10, 10, 0 }).Result == PlaceResult::BadGround;

						Fixture blocked;
						AddDecor(blocked.Grid, DecorKind::Rock, 10 + i, 10 + j);
						const uint64_t hash = blocked.Grid.Hash();
						allBlockers = allBlockers && PlaceBuilding(blocked.Grid, blocked.Stock, PlaceSpec{ id, 10, 10, 0 }).Result == PlaceResult::Occupied
							&& blocked.Grid.Hash() == hash;
					}
				}
				Fixture clear;
				const PlaceOutcome placed = PlaceBuilding(clear.Grid, clear.Stock, PlaceSpec{ id, 10, 10, 0 });
				bool covered = placed.Result == PlaceResult::Ok;
				for (int j = 0; j < def.Height; ++j)
					for (int i = 0; i < def.Width; ++i)
						covered = covered && clear.Grid.At(10 + i, 10 + j).Occupant == placed.OccupantId;
				covered = covered && clear.Grid.At(10 + def.Width, 10).Occupant == k_NoOccupant && clear.Grid.At(10, 10 + def.Height).Occupant == k_NoOccupant;
				allTiles = allTiles && covered;
			}
			report.Check(allTiles, "Placement: a multi-tile building refuses water under any one of its tiles, and a free one covers exactly its footprint (bench 2x1, coop 3x2, barn 4x3, greenhouse 4x4)");
			report.Check(allBlockers, "Placement: a multi-tile building refuses an obstacle on any one of its tiles and leaves the grid untouched");

			Fixture f;
			const PlaceOutcome big = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Greenhouse, 4, 4, 0 });
			const PlaceOutcome overlap = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Scarecrow, 7, 7, 0 });
			report.Check(big.Result == PlaceResult::Ok && overlap.Result == PlaceResult::Occupied, "Placement: the far corner of a placed 4x4 is occupied");
			report.Check(RemoveBuilding(f.Grid, f.Stock, 7, 7).Removed && f.Grid.CountOccupants(OccupantKind::Building) == 0 && f.Grid.At(4, 4).Occupant == k_NoOccupant,
				"Placement: removing from any tile of a building frees every tile of it");
		}

		void CheckRotation(CheckReport& report)
		{
			const BuildDef& coop = GetBuildDef(BuildId::Coop);
			const Footprint r0 = RotatedFootprint(coop, 0);
			const Footprint r1 = RotatedFootprint(coop, 1);
			const Footprint r2 = RotatedFootprint(coop, 2);
			const Footprint r3 = RotatedFootprint(coop, 3);
			report.Check(r0.W == 3 && r0.H == 2 && r1.W == 2 && r1.H == 3 && r2.W == 3 && r2.H == 2 && r3.W == 2 && r3.H == 3, "Placement: rotation swaps width and height on odd quarter turns");

			Fixture f;
			AddDecor(f.Grid, DecorKind::Rock, 10, 12);
			report.Check(CheckPlacement(f.Grid, f.Stock, PlaceSpec{ BuildId::Coop, 10, 10, 0 }) == PlaceResult::Ok
				&& CheckPlacement(f.Grid, f.Stock, PlaceSpec{ BuildId::Coop, 10, 10, 1 }) == PlaceResult::Occupied,
				"Placement: a coop fits at 3x2 and not at 2x3 when a rock stands one tile below it");

			const PlaceOutcome turned = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Coop, 20, 20, 1 });
			const Occupant* occupant = f.Grid.FindOccupant(turned.OccupantId);
			report.Check(occupant && occupant->W == 2 && occupant->H == 3 && occupant->Rotation == 1 && f.Grid.At(21, 22).Occupant == turned.OccupantId
				&& f.Grid.At(22, 20).Occupant == k_NoOccupant,
				"Placement: a turned coop is stored 2x3 with its rotation and occupies the turned tiles");
			report.Check(turned.Dirty.W == 2 && turned.Dirty.H == 3, "Placement: the reported edit follows the turned footprint");

			Fixture edge;
			report.Check(CheckPlacement(edge.Grid, edge.Stock, PlaceSpec{ BuildId::Coop, 0, k_Size - 2, 0 }) == PlaceResult::Ok
				&& CheckPlacement(edge.Grid, edge.Stock, PlaceSpec{ BuildId::Coop, 0, k_Size - 2, 1 }) == PlaceResult::OutOfBounds,
				"Placement: turning a coop at the south edge pushes it out of bounds");
		}

		void CheckGroundOnly(CheckReport& report)
		{
			Fixture f;
			const uint64_t blank = f.Grid.Hash();
			const PlaceOutcome dirt = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::DirtPath, 6, 6, 0 });
			report.Check(dirt.Result == PlaceResult::Ok && dirt.OccupantId == k_NoOccupant && f.Grid.GetGround(6, 6) == Ground::Path && f.Grid.At(6, 6).Occupant == k_NoOccupant
				&& f.Grid.Hash() != blank,
				"Placement: a path sets the ground and adds no occupant");
			report.Check(PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::StonePath, 6, 6, 0 }).Result == PlaceResult::Occupied, "Placement: a path does not go over a path");

			const Totals before = TotalsOf(f.Stock);
			const RemoveOutcome removed = RemoveBuilding(f.Grid, f.Stock, 6, 6);
			report.Check(removed.Removed && removed.Id == BuildId::DirtPath && f.Grid.GetGround(6, 6) == Ground::Grass && f.Grid.Hash() == blank
				&& TotalsOf(f.Stock).Fibre - before.Fibre == GetBuildDef(BuildId::DirtPath).Cost.Fibre * BUILD_REFUND_PERMILLE / 1000,
				"Placement: removing a path restores grass and refunds by the path that was laid");

			PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::StonePath, 7, 7, 0 });
			const Totals stoneBefore = TotalsOf(f.Stock);
			const RemoveOutcome stone = RemoveBuilding(f.Grid, f.Stock, 7, 7);
			report.Check(stone.Removed && stone.Id == BuildId::StonePath && TotalsOf(f.Stock).Stone - stoneBefore.Stone == GetBuildDef(BuildId::StonePath).Cost.Stone * BUILD_REFUND_PERMILLE / 1000
				&& TotalsOf(f.Stock).Fibre == stoneBefore.Fibre,
				"Placement: a stone path is remembered as stone and refunds stone, not fibre");

			f.Grid.SetGround(9, 9, Ground::Path);
			const uint64_t hash = f.Grid.Hash();
			report.Check(!RemoveBuilding(f.Grid, f.Stock, 9, 9).Removed && f.Grid.Hash() == hash, "Placement: a path the player did not lay cannot be removed");

			f.Grid.SetGround(11, 11, Ground::Tilled);
			report.Check(PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::DirtPath, 11, 11, 0 }).Result == PlaceResult::Ok && f.Grid.GetGround(11, 11) == Ground::Path,
				"Placement: a path goes over empty tilled soil");
			f.Grid.SetGround(12, 11, Ground::Tilled);
			f.Grid.At(12, 11).Crop.Def = 2;
			report.Check(PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::DirtPath, 12, 11, 0 }).Result == PlaceResult::Occupied, "Placement: a path does not go over a crop");

			PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::DirtPath, 14, 14, 0 });
			PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Bench, 14, 14, 0 });
			const RemoveOutcome bench = RemoveBuilding(f.Grid, f.Stock, 14, 14);
			report.Check(bench.Removed && bench.Id == BuildId::Bench && f.Grid.GetGround(14, 14) == Ground::Path && f.Grid.At(14, 14).Built != 0,
				"Placement: a building on a laid path comes off first and leaves the path");
		}

		void CheckClearing(CheckReport& report)
		{
			Fixture f;
			AddDecor(f.Grid, DecorKind::Grass, 5, 5);
			AddDecor(f.Grid, DecorKind::Flower, 6, 5);
			const PlaceOutcome over = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Bench, 5, 5, 0 });
			report.Check(over.Result == PlaceResult::Ok && over.ClearedDecor == 2 && f.Grid.CountOccupants(OccupantKind::Decoration) == 0 && f.Grid.CountOccupants(OccupantKind::Building) == 1,
				"Placement: building over grass and a flower clears both for free");

			Fixture g;
			AddDecor(g.Grid, DecorKind::Grass, 5, 5);
			const Totals before = TotalsOf(g.Stock);
			PlaceBuilding(g.Grid, g.Stock, PlaceSpec{ BuildId::WoodFence, 5, 5, 0 });
			report.Check(before.Wood - TotalsOf(g.Stock).Wood == GetBuildDef(BuildId::WoodFence).Cost.Wood && TotalsOf(g.Stock).Fibre == before.Fibre,
				"Placement: clearing cosmetic decoration yields and costs nothing");

			Fixture h;
			AddDecor(h.Grid, DecorKind::Grass, 5, 5);
			AddDecor(h.Grid, DecorKind::Bush, 6, 5);
			const uint64_t hash = h.Grid.Hash();
			report.Check(PlaceBuilding(h.Grid, h.Stock, PlaceSpec{ BuildId::Bench, 5, 5, 0 }).Result == PlaceResult::Occupied && h.Grid.Hash() == hash,
				"Placement: a bush blocks, and the grass beside it is not cleared by a refused placement");

			Fixture p;
			AddDecor(p.Grid, DecorKind::Flower, 3, 3);
			report.Check(PlaceBuilding(p.Grid, p.Stock, PlaceSpec{ BuildId::DirtPath, 3, 3, 0 }).Result == PlaceResult::Ok && p.Grid.CountOccupants(OccupantKind::Decoration) == 0,
				"Placement: a path clears a flower under it");

			struct ToolCase
			{
				const char* Name;
				ItemId Tool;
				DecorKind Kind;
				ItemId Yield;
				int Count;
			};
			const ToolCase tools[] = {
				{ "axe on a tree", ItemId::Axe, DecorKind::Tree, ItemId::Wood, 3 },
				{ "axe on a pine", ItemId::Axe, DecorKind::Pine, ItemId::Wood, 4 },
				{ "axe on a stump", ItemId::Axe, DecorKind::Stump, ItemId::Wood, 1 },
				{ "pickaxe on a rock", ItemId::Pickaxe, DecorKind::Rock, ItemId::Stone, 2 },
				{ "scythe on a weed", ItemId::Scythe, DecorKind::Weed, ItemId::Fibre, 1 },
				{ "scythe on a bush", ItemId::Scythe, DecorKind::Bush, ItemId::Fibre, 2 },
			};
			for (const ToolCase& c : tools)
			{
				Fixture t;
				AddDecor(t.Grid, c.Kind, 4, 4);
				const int held = t.Stock.Count(c.Yield);
				const ToolOutcome outcome = UseClearingTool(t.Grid, t.Stock, c.Tool, 4, 4);
				report.Check(outcome.Result == ToolResult::Cleared && t.Stock.Count(c.Yield) == held + c.Count && t.Grid.At(4, 4).Occupant == k_NoOccupant
					&& t.Grid.CountOccupants(OccupantKind::Decoration) == 0 && outcome.Dirty.X == 4 && outcome.Dirty.W == 1,
					std::format("Tools: {} clears it for {} {}", c.Name, c.Count, GetItemDef(c.Yield).Name));
			}

			Fixture t;
			AddDecor(t.Grid, DecorKind::Rock, 4, 4);
			AddDecor(t.Grid, DecorKind::Tree, 5, 4);
			AddDecor(t.Grid, DecorKind::Grass, 6, 4);
			const uint64_t toolHash = t.Grid.Hash();
			const Totals toolTotals = TotalsOf(t.Stock);
			const ToolOutcome wrong = UseClearingTool(t.Grid, t.Stock, ItemId::Axe, 4, 4);
			const ToolOutcome wrongTree = UseClearingTool(t.Grid, t.Stock, ItemId::Scythe, 5, 4);
			const ToolOutcome hoe = UseClearingTool(t.Grid, t.Stock, ItemId::Hoe, 4, 4);
			const ToolOutcome nothing = UseClearingTool(t.Grid, t.Stock, ItemId::Axe, 20, 20);
			const ToolOutcome outside = UseClearingTool(t.Grid, t.Stock, ItemId::Axe, -1, 20);
			const ToolOutcome grassAxe = UseClearingTool(t.Grid, t.Stock, ItemId::Axe, 6, 4);
			report.Check(wrong.Result == ToolResult::WrongTool && wrong.Needed == ItemId::Pickaxe && wrongTree.Result == ToolResult::WrongTool && wrongTree.Needed == ItemId::Axe
				&& hoe.Result == ToolResult::NotMine && nothing.Result == ToolResult::Nothing && outside.Result == ToolResult::Nothing && grassAxe.Result == ToolResult::WrongTool
				&& t.Grid.Hash() == toolHash && TotalsOf(t.Stock) == toolTotals,
				"Tools: the wrong tool, the hoe, an empty tile and a tile outside the map change nothing and say why");
			const ToolOutcome grass = UseClearingTool(t.Grid, t.Stock, ItemId::Scythe, 6, 4);
			report.Check(grass.Result == ToolResult::Cleared && grass.YieldCount == 0 && TotalsOf(t.Stock) == toolTotals && t.Grid.At(6, 4).Occupant == k_NoOccupant,
				"Tools: the scythe clears grass for nothing");

			Inventory stock;
			stock.Add(ItemId::Wood, 3);
			report.Check(!stock.Remove(ItemId::Wood, 4) && stock.Count(ItemId::Wood) == 3 && stock.Remove(ItemId::Wood, 3) && stock.Count(ItemId::Wood) == 0
				&& !stock.SpendCoins(1) && stock.GetCoins() == 0,
				"Inventory: removing more than held takes nothing");

			TileGrid valley(VALLEY_SIZE, VALLEY_SIZE);
			GenerateValley(valley, 1);
			int tx = -1;
			int ty = -1;
			DecorKind kind = DecorKind::Tree;
			for (const Occupant& occupant : valley.GetOccupants())
			{
				if (occupant.Kind == OccupantKind::Decoration && (occupant.DefId == static_cast<uint16_t>(DecorKind::Tree) || occupant.DefId == static_cast<uint16_t>(DecorKind::Pine)))
				{
					tx = occupant.X;
					ty = occupant.Y;
					kind = static_cast<DecorKind>(occupant.DefId);
					break;
				}
			}
			Inventory bag;
			const int decorBefore = valley.CountOccupants(OccupantKind::Decoration);
			const ToolOutcome felled = UseClearingTool(valley, bag, ItemId::Axe, tx, ty);
			report.Check(tx >= 0 && felled.Result == ToolResult::Cleared && valley.CountOccupants(OccupantKind::Decoration) == decorBefore - 1 && bag.Count(ItemId::Wood) == GetDecorDef(kind).YieldCount,
				"Tools: a generated valley's tree falls to the axe for its def's wood");
		}

		void CheckRebakeCount(CheckReport& report)
		{
			Fixture f;
			const PlaceOutcome fence = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::WoodFence, 5, 5, 0 });
			const RemoveOutcome fenceOut = RemoveBuilding(f.Grid, f.Stock, 5, 5);
			report.Check(QueuedCells(fence.Dirty) == 1 && QueuedCells(fenceOut.Dirty) == 1, "Placement: placing and removing a fence each queue exactly one cell");

			const PlaceOutcome seam = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Barn, 14, 14, 0 });
			const RemoveOutcome seamOut = RemoveBuilding(f.Grid, f.Stock, 15, 15);
			report.Check(QueuedCells(seam.Dirty) == 4 && QueuedCells(seamOut.Dirty) == 4, "Placement: a barn across a cell corner queues the four cells it touches, once each, and removal queues the same");

			const PlaceOutcome inside = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Barn, 2, 2, 0 });
			report.Check(QueuedCells(inside.Dirty) == 1, "Placement: a barn inside one cell queues one cell");

			AddDecor(f.Grid, DecorKind::Grass, 15, 20);
			const PlaceOutcome clearing = PlaceBuilding(f.Grid, f.Stock, PlaceSpec{ BuildId::Scarecrow, 15, 20, 0 });
			report.Check(clearing.ClearedDecor == 1 && QueuedCells(clearing.Dirty) == 1, "Placement: clearing the grass under a scarecrow adds no second cell");

			const ToolOutcome none = UseClearingTool(f.Grid, f.Stock, ItemId::Axe, 25, 25);
			report.Check(none.Dirty.W == 0, "Tools: a swing that clears nothing reports no dirty tiles");
		}

		void CheckTargeting(CheckReport& report)
		{
			const glm::vec2 feet(10.4f, 12.7f);
			report.Check(Targeting::TileOf(feet).X == 10 && Targeting::TileOf(feet).Y == 12 && Targeting::TileOf(glm::vec2(-0.2f, -0.2f)).X == -1,
				"Targeting: a world point maps to its tile, negatives floor");

			const float quarter = 0.5f * PlayerMath::PI;
			const TileCoord south = Targeting::FacingTile(feet, 0.0f);
			const TileCoord east = Targeting::FacingTile(feet, quarter);
			const TileCoord north = Targeting::FacingTile(feet, PlayerMath::PI);
			const TileCoord west = Targeting::FacingTile(feet, -quarter);
			report.Check(south == TileCoord{ 10, 13 } && east == TileCoord{ 11, 12 } && north == TileCoord{ 10, 11 } && west == TileCoord{ 9, 12 },
				"Targeting: the tile in front is the next tile along yaw 0 (+z), a quarter turn (+x), a half turn and three quarters");
			report.Check(Targeting::FacingTile(feet, 0.3f) == south && Targeting::FacingTile(feet, quarter + 0.4f) == east && Targeting::FacingTile(feet, quarter - 0.4f) == east
				&& Targeting::FacingTile(feet, 3.0f) == north,
				"Targeting: a diagonal facing snaps to its dominant axis");

			const AimInput facing{ feet, 0.0f, false, glm::vec2(0.0f) };
			const Aim byFacing = Targeting::Resolve(facing, 3);
			report.Check(!byFacing.FromMouse && byFacing.Target == south && byFacing.Facing == glm::ivec2(0, 1), "Targeting: with no mouse the target is the tile in front");

			AimInput hover{ feet, 0.0f, true, glm::vec2(12.2f, 14.9f) };
			Aim byMouse = Targeting::Resolve(hover, 3);
			report.Check(byMouse.FromMouse && byMouse.Target == TileCoord{ 12, 14 }, "Targeting: a hovered tile within 3 tiles is the target");
			const glm::vec2 toTile = glm::vec2(12.5f, 14.5f) - feet;
			report.Check(std::abs(byMouse.FaceYaw - PlayerMath::YawOf(toTile)) < 1e-5f && byMouse.Facing == glm::ivec2(1, 0),
				"Targeting: the farmer turns toward the hovered tile");

			hover.MouseGround = glm::vec2(13.5f, 12.5f);
			report.Check(Targeting::Resolve(hover, 3).FromMouse, "Targeting: exactly 3 tiles away is still in reach");
			hover.MouseGround = glm::vec2(14.5f, 12.5f);
			const Aim far = Targeting::Resolve(hover, 3);
			report.Check(!far.FromMouse && far.Target == south, "Targeting: a hovered tile 4 tiles away falls back to the tile in front");

			bool property = true;
			const glm::ivec2 facings[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
			for (int id = 0; id < BUILD_COUNT; ++id)
			{
				for (int rotation = 0; rotation < 4; ++rotation)
				{
					const Footprint fp = RotatedFootprint(GetBuildDef(static_cast<BuildId>(id)), rotation);
					for (const glm::ivec2& step : facings)
					{
						const TileCoord farmer{ 20, 20 };
						const TileCoord target{ farmer.X + step.x, farmer.Y + step.y };
						const TileCoord origin = Targeting::OriginInFront(target, step, fp.W, fp.H);
						const bool hasTarget = target.X >= origin.X && target.X < origin.X + fp.W && target.Y >= origin.Y && target.Y < origin.Y + fp.H;
						const bool hasFarmer = farmer.X >= origin.X && farmer.X < origin.X + fp.W && farmer.Y >= origin.Y && farmer.Y < origin.Y + fp.H;
						property = property && hasTarget && !hasFarmer;
					}
				}
			}
			report.Check(property, "Targeting: every buildable at every rotation and facing covers the target tile and never the farmer's own");

			const TileCoord centred = Targeting::OriginCentred(TileCoord{ 20, 20 }, 4, 3);
			report.Check(centred == TileCoord{ 18, 19 } && Targeting::OriginCentred(TileCoord{ 20, 20 }, 1, 1) == TileCoord{ 20, 20 },
				"Targeting: a mouse target sits at the centre of the footprint");
		}

		void CheckController(CheckReport& report)
		{
			Fixture f;
			int dirtyCalls = 0;
			TileRect lastDirty;
			BuildController controller(f.Grid, f.Stock, [&](const TileRect& area) { ++dirtyCalls; lastDirty = area; });

			AddDecor(f.Grid, DecorKind::Tree, 10, 11);
			AddDecor(f.Grid, DecorKind::Rock, 11, 10);
			f.Stock.SelectSlot(f.Stock.FindSlot(ItemId::Axe));
			const int wood0 = f.Stock.Count(ItemId::Wood);
			const int stone0 = f.Stock.Count(ItemId::Stone);
			AimInput aim;
			aim.Feet = glm::vec2(10.5f, 10.5f);
			aim.Yaw = 0.0f;

			PlayerIntent idle;
			controller.Step(idle, aim);
			report.Check(controller.GetMode() == BuildMode::Tools && controller.GetTool() == ItemId::Axe && controller.GetAim().Target == TileCoord{ 10, 11 } && dirtyCalls == 0,
				"Controller: it starts in tool mode with the axe, aiming at the tile in front, and an idle step edits nothing");

			PlayerIntent swing;
			swing.Use = true;
			controller.Step(swing, aim);
			report.Check(f.Stock.Count(ItemId::Wood) == wood0 + 3 && dirtyCalls == 1 && lastDirty.X == 10 && lastDirty.Y == 11 && f.Grid.CountOccupants(OccupantKind::Decoration) == 1,
				"Controller: a Use pulse swings the axe at the tile in front, yields wood and reports one edit");

			PlayerIntent east;
			aim.Yaw = 0.5f * PlayerMath::PI;
			controller.Step(east, aim);
			controller.Step(swing, aim);
			report.Check(f.Stock.Count(ItemId::Stone) == stone0 && dirtyCalls == 1 && !controller.GetMessage().empty(),
				"Controller: the axe on a rock changes nothing and says which tool it needs");

			PlayerIntent pick;
			pick.HotbarSlot = 3;
			controller.Step(pick, aim);
			controller.Step(swing, aim);
			report.Check(controller.GetTool() == ItemId::Pickaxe && f.Stock.Count(ItemId::Stone) == stone0 + 2 && dirtyCalls == 2, "Controller: hotbar slot 4 picks the pickaxe, which breaks the rock");

			PlayerIntent next;
			next.HotbarNext = true;
			controller.Step(next, aim);
			const ItemId afterNext = controller.GetTool();
			PlayerIntent previous;
			previous.HotbarPrevious = true;
			controller.Step(previous, aim);
			controller.Step(previous, aim);
			controller.Step(previous, aim);
			controller.Step(previous, aim);
			controller.Step(previous, aim);
			report.Check(afterNext == ItemId::Scythe && controller.GetTool() == ItemId::Scythe, "Controller: next and previous cycle the tools and wrap");

			PlayerIntent toggle;
			toggle.ToggleBuild = true;
			controller.Step(toggle, aim);
			report.Check(controller.GetMode() == BuildMode::Build, "Controller: the build toggle enters build mode");

			PlayerIntent choose;
			choose.HotbarSlot = static_cast<int>(BuildId::Coop);
			controller.Step(choose, aim);
			report.Check(controller.GetBuildable() == BuildId::Coop && controller.GetRotation() == 0, "Controller: in build mode a slot picks that buildable");

			aim.Feet = glm::vec2(20.5f, 20.5f);
			aim.Yaw = 0.5f * PlayerMath::PI;
			controller.Step(idle, aim);
			const PlaceSpec spec = controller.GetSpec();
			report.Check(spec.X == 21 && spec.Y == 19 && controller.GetPreview() == PlaceResult::Ok,
				"Controller: the coop previews three wide starting at the tile in front, valid");

			PlayerIntent rotate;
			rotate.RotatePlacement = true;
			controller.Step(rotate, aim);
			const PlaceSpec turned = controller.GetSpec();
			report.Check(controller.GetRotation() == 1 && FootprintOf(turned).W == 2 && FootprintOf(turned).H == 3 && turned.Y == 19, "Controller: rotate turns the footprint and keeps it centred across the facing");
			for (int i = 0; i < 3; ++i)
				controller.Step(rotate, aim);
			report.Check(controller.GetRotation() == 0, "Controller: four rotations come back round");

			const Totals before = TotalsOf(f.Stock);
			const int callsBefore = dirtyCalls;
			controller.Step(swing, aim);
			const Totals after = TotalsOf(f.Stock);
			report.Check(dirtyCalls == callsBefore + 1 && before.Coins - after.Coins == 400 && before.Wood - after.Wood == 30 && before.Stone - after.Stone == 10
				&& f.Grid.CountOccupants(OccupantKind::Building) == 1 && lastDirty.W == 3 && lastDirty.H == 2,
				"Controller: Use in build mode places the previewed coop, charges it and reports the footprint");

			controller.Step(swing, aim);
			report.Check(controller.GetPreview() == PlaceResult::Occupied && dirtyCalls == callsBefore + 1 && f.Grid.CountOccupants(OccupantKind::Building) == 1 && TotalsOf(f.Stock) == after,
				"Controller: placing again on the same spot is refused, charged nothing and not reported");

			aim.Feet = glm::vec2(20.5f, 19.5f);
			aim.Yaw = 0.5f * PlayerMath::PI;
			PlayerIntent remove;
			remove.Interact = true;
			controller.Step(remove, aim);
			report.Check(f.Grid.CountOccupants(OccupantKind::Building) == 0 && TotalsOf(f.Stock).Coins == after.Coins + 200 && dirtyCalls == callsBefore + 2,
				"Controller: Interact in build mode removes the building at the target and refunds half");

			controller.SetMode(BuildMode::Tools);
			controller.SelectTool(ItemId::Hoe);
			controller.Step(swing, aim);
			report.Check(controller.GetTool() == ItemId::Hoe && dirtyCalls == callsBefore + 3, "Controller: the hoe is selectable and tills the tile in front");

			Fixture g;
			BuildController mouse(g.Grid, g.Stock, [](const TileRect&) {});
			AimInput mouseAim;
			mouseAim.Feet = glm::vec2(20.5f, 20.5f);
			mouseAim.HasMouse = true;
			mouseAim.MouseGround = glm::vec2(22.5f, 21.5f);
			mouse.SetMode(BuildMode::Build);
			mouse.SelectBuildable(BuildId::Barn);
			mouse.Step(idle, mouseAim);
			report.Check(mouse.GetAim().FromMouse && mouse.GetAim().Target == TileCoord{ 22, 21 } && mouse.GetSpec().X == 20 && mouse.GetSpec().Y == 20,
				"Controller: with the mouse the barn is centred on the hovered tile");
		}
	}

	void RunPlacementChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Placement");
		CheckFreeTile(report);
		CheckRefusals(report);
		CheckRemoval(report);
		CheckFootprints(report);
		CheckRotation(report);
		CheckGroundOnly(report);
		CheckClearing(report);
		CheckRebakeCount(report);
		CheckTargeting(report);
		CheckController(report);
	}

}
