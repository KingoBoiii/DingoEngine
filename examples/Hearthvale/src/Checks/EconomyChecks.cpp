#include "Checks/Checks.h"
#include "Checks/EconomyRig.h"
#include "Farm/Crops.h"
#include "Farm/Economy.h"
#include "GameTuning.h"
#include "Placing/BuildController.h"
#include "Placing/Placement.h"
#include "Placing/Tools.h"

#include <array>
#include <format>
#include <string>

namespace Dingo
{

	namespace
	{
		constexpr int k_Capacity = INVENTORY_SLOTS * 99;

		ItemId HarvestOf(CropId crop) { return HarvestItem(crop); }

		void CheckInventoryStacks(CheckReport& report)
		{
			report.Check(INVENTORY_COLUMNS == 6 && INVENTORY_ROWS == 4 && BACKPACK_SLOTS == 24 && HOTBAR_SLOTS == 9 && INVENTORY_SLOTS == 33, "Economy: the pack is a 9 slot hotbar and a 6 x 4 backpack");

			Inventory pack;
			const int added = pack.Add(ItemId::Wood, 250);
			report.Check(added == 250 && pack.Count(ItemId::Wood) == 250 && pack.FreeSlots() == INVENTORY_SLOTS - 3, "Economy: 250 wood stacks into three slots");

			bool withinMax = true;
			for (int i = 0; i < INVENTORY_SLOTS; ++i)
			{
				const ItemStack& slot = pack.GetSlot(i);
				if (!slot.Empty() && slot.Count > GetItemDef(slot.Item).StackMax)
					withinMax = false;
			}
			report.Check(withinMax && pack.GetSlot(HOTBAR_SLOTS).Count == 99 && pack.GetSlot(HOTBAR_SLOTS + 1).Count == 99 && pack.GetSlot(HOTBAR_SLOTS + 2).Count == 52,
				"Economy: no stack passes its max, and a fresh item fills the backpack from its first slot");

			pack.Add(ItemId::Wood, 47);
			report.Check(pack.GetSlot(HOTBAR_SLOTS + 2).Count == 99 && pack.Count(ItemId::Wood) == 297 && pack.FreeSlots() == INVENTORY_SLOTS - 3, "Economy: more of an item tops its partial stack up before opening a new one");

			Inventory gear;
			gear.Add(ItemId::Hoe, 1);
			gear.Add(SeedItem(CropId::Parsnip), 5);
			gear.Add(ItemId::Stone, 1);
			report.Check(gear.FindSlot(ItemId::Hoe) >= 0 && gear.FindSlot(ItemId::Hoe) < HOTBAR_SLOTS && gear.FindSlot(SeedItem(CropId::Parsnip)) < HOTBAR_SLOTS
				&& gear.FindSlot(ItemId::Stone) >= HOTBAR_SLOTS,
				"Economy: tools and seeds go to the hotbar, everything else to the backpack");

			Inventory seeds;
			for (int crop = 0; crop < CROP_COUNT; ++crop)
				seeds.Add(SeedItem(static_cast<CropId>(crop)), 1);
			seeds.Add(ItemId::Egg, 1);
			bool hotbarFull = true;
			for (int i = 0; i < HOTBAR_SLOTS; ++i)
				hotbarFull = hotbarFull && !seeds.GetSlot(i).Empty();
			report.Check(hotbarFull && seeds.FindSlot(SeedItem(static_cast<CropId>(CROP_COUNT - 1))) >= HOTBAR_SLOTS && seeds.Count(SeedItem(static_cast<CropId>(CROP_COUNT - 1))) == 1,
				"Economy: seeds spill into the backpack when the hotbar is full");

			Inventory full;
			const int fitted = full.Add(ItemId::Wood, k_Capacity + 500);
			report.Check(fitted == k_Capacity && full.FreeSlots() == 0 && !full.CanAdd(ItemId::Wood, 1) && !full.CanAdd(ItemId::Stone, 1) && full.Add(ItemId::Stone, 5) == 0 && full.Count(ItemId::Stone) == 0,
				"Economy: a full pack takes nothing more and reports how much fitted");

			Inventory partial;
			partial.Add(ItemId::Wood, k_Capacity - 99 - 10);
			partial.Add(ItemId::Wood, 10);
			report.Check(partial.Room(ItemId::Wood) == 99 && partial.CanAdd(ItemId::Wood, 99) && !partial.CanAdd(ItemId::Wood, 100) && partial.Room(ItemId::Egg) == 99,
				"Economy: Room counts gaps in stacks plus whole empty slots");

			Inventory shared;
			shared.Add(ItemId::Wood, k_Capacity - 99);
			const std::array<ItemStack, 2> oneEach = { ItemStack{ ItemId::Stone, 1 }, ItemStack{ ItemId::Egg, 1 } };
			const std::array<ItemStack, 1> one = { ItemStack{ ItemId::Stone, 1 } };
			report.Check(!shared.CanAddAll(oneEach) && shared.CanAddAll(one) && shared.Count(ItemId::Stone) == 0, "Economy: CanAddAll counts two items that compete for the last slot");

			Inventory remove;
			remove.Add(ItemId::Fibre, 150);
			const uint64_t before = remove.Hash();
			report.Check(!remove.Remove(ItemId::Fibre, 151) && remove.Hash() == before && remove.Remove(ItemId::Fibre, 120) && remove.Count(ItemId::Fibre) == 30
				&& remove.FreeSlots() == INVENTORY_SLOTS - 1,
				"Economy: removing takes across stacks, frees emptied slots and refuses more than held");

			Inventory move;
			move.Add(ItemId::Egg, 150);
			const int a = move.FindSlot(ItemId::Egg);
			move.MoveSlot(a + 1, a);
			report.Check(move.GetSlot(a).Count == 99 && move.GetSlot(a + 1).Count == 51 && move.Count(ItemId::Egg) == 150, "Economy: moving a stack onto the same item merges up to the max");
			move.MoveSlot(a, 0);
			report.Check(move.GetSlot(0).Item == ItemId::Egg && move.GetSlot(0).Count == 99 && move.GetSlot(a).Empty(), "Economy: moving onto an empty slot relocates the stack");
		}

		void CheckHotbar(CheckReport& report)
		{
			Inventory pack;
			pack.GiveStarterLoadout();
			report.Check(pack.GetCoins() == STARTER_COINS && pack.GetSelected().Item == ItemId::Axe && pack.GetSelectedSlot() == pack.FindSlot(ItemId::Axe), "Economy: the starting loadout gives the coins and selects the axe");
			bool loadout = true;
			for (const StarterStack& stack : STARTER_LOADOUT)
				loadout = loadout && pack.Count(stack.Item) == stack.Count;
			bool toolsInHotbar = true;
			for (const ItemId tool : BUILD_TOOLS)
				toolsInHotbar = toolsInHotbar && pack.FindSlot(tool) >= 0 && pack.FindSlot(tool) < HOTBAR_SLOTS;
			report.Check(loadout && toolsInHotbar && pack.Count(ItemId::Wood) == 0, "Economy: the loadout is exactly the tuning table, tools on the hotbar");

			const int start = pack.GetSelectedSlot();
			pack.CycleSelection(1);
			const int forward = pack.GetSelectedSlot();
			pack.CycleSelection(-1);
			report.Check(forward != start && pack.GetSelectedSlot() == start && !pack.GetSelected().Empty(), "Economy: cycling steps over to the next stocked slot and back");

			Inventory ring;
			ring.Add(ItemId::Hoe, 1);
			ring.Add(ItemId::Axe, 1);
			ring.SelectSlot(1);
			ring.CycleSelection(1);
			const int wrapped = ring.GetSelectedSlot();
			ring.SelectSlot(HOTBAR_SLOTS + 4);
			report.Check(wrapped == 0 && ring.GetSelectedSlot() == 0, "Economy: cycling skips empty slots and wraps; selecting outside the hotbar is ignored");

			Inventory none;
			none.CycleSelection(1);
			report.Check(none.GetSelectedSlot() == 0 && none.GetSelected().Empty(), "Economy: cycling an empty hotbar stays put");
		}

		void CheckPrices(CheckReport& report)
		{
			bool seedsPriced = true;
			bool harvestsPriced = true;
			bool backBelow = true;
			for (const CropDef& crop : GetCropDefs())
			{
				seedsPriced = seedsPriced && GetItemDef(SeedItem(crop.Id)).BuyPrice > 0;
				harvestsPriced = harvestsPriced && GetItemDef(HarvestOf(crop.Id)).SellPrice > 0;
				const int back = ShopSellBackPrice(HarvestOf(crop.Id));
				backBelow = backBelow && back > 0 && back <= GetItemDef(HarvestOf(crop.Id)).SellPrice;
			}
			report.Check(seedsPriced && harvestsPriced, "Economy: every seed has a buy price and every harvest a ship price above zero");
			report.Check(backBelow && ShopSellBackPrice(ItemId::Hoe) == 0 && ShopSellBackPrice(ItemId::Count) == 0, "Economy: the shop pays at most the ship price for goods, and nothing for tools");

			bool products = true;
			for (const ItemId item : { ItemId::Egg, ItemId::Milk, ItemId::Honey, ItemId::Wood, ItemId::Stone, ItemId::Fibre })
				products = products && GetItemDef(item).SellPrice > 0;
			report.Check(products, "Economy: eggs, milk, honey and the resources can be shipped");

			bool stockPositive = true;
			bool seedsStocked = true;
			const std::vector<ShopOffer> shop = ShopStock(Merchant::Shopkeeper);
			for (const ShopOffer& offer : shop)
				stockPositive = stockPositive && offer.Price > 0 && offer.Kind == OfferKind::Item && offer.Price == GetItemDef(offer.Item).BuyPrice;
			for (const CropDef& crop : GetCropDefs())
			{
				bool found = false;
				for (const ShopOffer& offer : shop)
					found = found || offer.Item == SeedItem(crop.Id);
				seedsStocked = seedsStocked && found;
			}
			report.Check(!shop.empty() && stockPositive && seedsStocked, "Economy: the shopkeeper sells every seed, every offer at its table price above zero");

			bool plansPriced = true;
			int planCount = 0;
			const std::vector<ShopOffer> carpenter = ShopStock(Merchant::Carpenter);
			for (const BuildDef& def : GetBuildDefs())
			{
				bool offered = false;
				for (const ShopOffer& offer : carpenter)
					offered = offered || (offer.Build == def.Id && offer.Price == def.PlanPrice);
				plansPriced = plansPriced && def.PlanPrice >= 0 && offered == (def.PlanPrice > 0);
				planCount += def.PlanPrice > 0 ? 1 : 0;
			}
			report.Check(plansPriced && planCount > 0 && static_cast<int>(carpenter.size()) == planCount, "Economy: the carpenter sells exactly the blueprint-gated buildables, each above zero");

			bool costsPositive = true;
			for (const BuildDef& def : GetBuildDefs())
				costsPositive = costsPositive && def.Cost.Coins + def.Cost.Wood + def.Cost.Stone + def.Cost.Fibre > 0;
			bool rewards = true;
			for (int s = 0; s < SEASON_BOARD_COUNT; ++s)
			{
				for (const BoardRequest& request : GetBoardDef(static_cast<Season>(s)).Requests)
					rewards = rewards && request.RewardCoins > 0 && request.Count > 0;
			}
			report.Check(costsPositive && rewards, "Economy: every building costs something and every board request pays a positive reward");
		}

		void CheckShop(CheckReport& report)
		{
			const ItemId seed = SeedItem(CropId::Parsnip);
			const int price = GetItemDef(seed).BuyPrice;

			Inventory pack;
			pack.AddCoins(price - 1);
			uint64_t before = pack.Hash();
			report.Check(ShopBuyItem(pack, seed, 1) == TradeResult::CannotAfford && pack.Hash() == before, "Economy: the shop refuses a seed one coin short and changes nothing");

			pack.AddCoins(1);
			report.Check(ShopBuyItem(pack, seed, 1) == TradeResult::Ok && pack.GetCoins() == 0 && pack.Count(seed) == 1, "Economy: with the exact price a seed is bought");

			pack.AddCoins(price * 2);
			before = pack.Hash();
			report.Check(ShopBuyItem(pack, seed, 3) == TradeResult::CannotAfford && pack.Hash() == before, "Economy: an order of three that only two cover buys none");
			report.Check(ShopBuyItem(pack, seed, 0) == TradeResult::BadCount && ShopBuyItem(pack, ItemId::Hoe, 1) == TradeResult::NotForSale
				&& ShopBuyItem(pack, HarvestOf(CropId::Parsnip), 1) == TradeResult::NotForSale && pack.Hash() == before,
				"Economy: the shop refuses a zero count and what it does not stock");

			Inventory crowded;
			crowded.AddCoins(1000);
			crowded.Add(ItemId::Egg, k_Capacity);
			before = crowded.Hash();
			report.Check(ShopBuyItem(crowded, seed, 1) == TradeResult::InventoryFull && crowded.Hash() == before, "Economy: a full pack cannot take a purchase and keeps its coins");

			Inventory seller;
			seller.Add(HarvestOf(CropId::Parsnip), 4);
			const int back = ShopSellBackPrice(HarvestOf(CropId::Parsnip));
			report.Check(ShopSellItem(seller, HarvestOf(CropId::Parsnip), 5) == TradeResult::NotEnough && seller.GetCoins() == 0 && seller.Count(HarvestOf(CropId::Parsnip)) == 4,
				"Economy: selling more than held sells nothing");
			report.Check(ShopSellItem(seller, HarvestOf(CropId::Parsnip), 4) == TradeResult::Ok && seller.GetCoins() == back * 4 && seller.Count(HarvestOf(CropId::Parsnip)) == 0,
				"Economy: selling to the shop pays at once, below the shipping price");
			seller.Add(ItemId::Hoe, 1);
			report.Check(ShopSellItem(seller, ItemId::Hoe, 1) == TradeResult::NotSellable && ShopSellItem(seller, ItemId::Wood, 0) == TradeResult::BadCount && seller.Count(ItemId::Hoe) == 1,
				"Economy: tools are not bought back");

			BuildId gated = BuildId::Count;
			for (const BuildDef& def : GetBuildDefs())
			{
				if (def.PlanPrice > 0 && gated == BuildId::Count)
					gated = def.Id;
			}
			const int planPrice = GetBuildDef(gated).PlanPrice;
			Inventory buyer;
			buyer.AddCoins(planPrice - 1);
			before = buyer.Hash();
			const bool lockedBefore = !buyer.KnowsPlan(gated);
			report.Check(lockedBefore && ShopBuyPlan(buyer, gated) == TradeResult::CannotAfford && buyer.Hash() == before && !buyer.KnowsPlan(gated), "Economy: a plan one coin short is refused");
			buyer.AddCoins(1);
			report.Check(ShopBuyPlan(buyer, gated) == TradeResult::Ok && buyer.KnowsPlan(gated) && buyer.GetCoins() == 0, "Economy: buying a plan spends its price and learns it");
			buyer.AddCoins(planPrice);
			report.Check(ShopBuyPlan(buyer, gated) == TradeResult::AlreadyKnown && buyer.GetCoins() == planPrice, "Economy: a plan is bought once");
			report.Check(ShopBuyPlan(buyer, BuildId::WoodFence) == TradeResult::NotForSale, "Economy: a buildable known from the start has no plan to buy");
		}

		void CheckPlanGate(CheckReport& report)
		{
			BuildId gated = BuildId::Count;
			for (const BuildDef& def : GetBuildDefs())
			{
				if (def.PlanPrice > 0 && !def.GroundOnly && gated == BuildId::Count)
					gated = def.Id;
			}
			const BuildDef& def = GetBuildDef(gated);

			TileGrid grid(32, 32);
			Inventory pack;
			pack.AddCoins(100000 + def.PlanPrice);
			for (const ItemId item : { ItemId::Wood, ItemId::Stone, ItemId::Fibre })
				pack.Add(item, 297);
			const uint64_t gridHash = grid.Hash();
			const uint64_t packHash = pack.Hash();
			const PlaceOutcome locked = PlaceBuilding(grid, pack, PlaceSpec{ gated, 4, 4, 0 });
			report.Check(locked.Result == PlaceResult::PlanLocked && grid.Hash() == gridHash && pack.Hash() == packHash, "Economy: a gated building is refused until its plan is bought, and nothing is charged");

			bool freeOnes = true;
			for (const BuildDef& other : GetBuildDefs())
				freeOnes = freeOnes && (other.PlanPrice > 0 || pack.KnowsPlan(other.Id));
			report.Check(freeOnes && pack.KnowsPlan(BuildId::WoodFence), "Economy: buildables with no plan price are known from the start");

			ShopBuyPlan(pack, gated);
			const PlaceOutcome open = PlaceBuilding(grid, pack, PlaceSpec{ gated, 4, 4, 0 });
			report.Check(open.Result == PlaceResult::Ok && grid.Hash() != gridHash, "Economy: after the plan is bought the building places");
		}

		void CheckFullPickups(CheckReport& report)
		{
			TileGrid grid(32, 32);
			Inventory pack;
			pack.LearnAllPlans();
			grid.At(5, 5).GroundState = Ground::Tilled;
			grid.At(5, 5).Crop.Def = static_cast<uint8_t>(CropId::Parsnip);
			grid.At(5, 5).Crop.DaysWatered = static_cast<uint8_t>(GetCropDef(CropId::Parsnip).GrowthDays);
			pack.Add(ItemId::Egg, k_Capacity);
			uint64_t gridHash = grid.Hash();
			uint64_t packHash = pack.Hash();
			const FarmOutcome refused = HarvestTile(grid, pack, 5, 5);
			report.Check(refused.Result == FarmResult::InventoryFull && grid.Hash() == gridHash && pack.Hash() == packHash && grid.At(5, 5).Crop.Present(),
				"Economy: a full pack refuses a harvest and the crop stays in the soil");

			pack.Remove(ItemId::Egg, 99);
			const FarmOutcome taken = HarvestTile(grid, pack, 5, 5);
			report.Check(taken.Result == FarmResult::Ok && pack.Count(HarvestOf(CropId::Parsnip)) >= 1 && !grid.At(5, 5).Crop.Present(), "Economy: once there is room the same crop is harvested");

			grid.AddOccupant(OccupantKind::Decoration, static_cast<uint16_t>(DecorKind::Tree), 9, 9, 1, 1, 0, 0);
			gridHash = grid.Hash();
			packHash = pack.Hash();
			const ToolOutcome noRoom = UseClearingTool(grid, pack, ItemId::Axe, 9, 9);
			report.Check(pack.FreeSlots() == 0 && noRoom.Result == ToolResult::InventoryFull && grid.Hash() == gridHash && pack.Hash() == packHash && grid.CountOccupants(OccupantKind::Decoration) == 1,
				"Economy: a full pack refuses a felled tree's wood and the tree stays standing");

			Inventory builder;
			builder.LearnAllPlans();
			builder.AddCoins(10000);
			builder.Add(ItemId::Wood, 99);
			builder.Add(ItemId::Stone, 99);
			builder.Add(ItemId::Fibre, 99);
			const PlaceOutcome placed = PlaceBuilding(grid, builder, PlaceSpec{ BuildId::Chest, 15, 15, 0 });
			builder.Add(ItemId::Wood, k_Capacity);
			gridHash = grid.Hash();
			packHash = builder.Hash();
			const RemoveOutcome removal = RemoveBuilding(grid, builder, 15, 15);
			report.Check(placed.Result == PlaceResult::Ok && builder.FreeSlots() == 0 && builder.Room(ItemId::Wood) == 0 && !removal.Removed && removal.InventoryFull
				&& grid.Hash() == gridHash && builder.Hash() == packHash,
				"Economy: a refund that does not fit leaves the building standing and the pack untouched");
		}

		void CheckShippingPayout(CheckReport& report)
		{
			EconomyRig rig;
			Inventory& pack = rig.State.Pack;
			pack.AddCoins(100);
			pack.Add(HarvestOf(CropId::Parsnip), 10);
			const int value = 10 * GetItemDef(HarvestOf(CropId::Parsnip)).SellPrice;

			report.Check(rig.State.Bin.Ship(pack, HarvestOf(CropId::Parsnip), 11) == TradeResult::NotEnough && rig.State.Bin.IsEmpty(), "Economy: shipping more than held ships nothing");
			report.Check(rig.State.Bin.Ship(pack, ItemId::Hoe, 1) == TradeResult::NotSellable && rig.State.Bin.Ship(pack, HarvestOf(CropId::Parsnip), 0) == TradeResult::BadCount, "Economy: tools and zero counts cannot be shipped");
			report.Check(rig.State.Bin.Ship(pack, HarvestOf(CropId::Parsnip), 10) == TradeResult::Ok && pack.Count(HarvestOf(CropId::Parsnip)) == 0
				&& rig.State.Bin.Held(HarvestOf(CropId::Parsnip)) == 10 && rig.State.Bin.PendingValue() == value,
				"Economy: shipped goods leave the pack and wait in the bin");

			for (int i = 0; i < 120; ++i)
				rig.Director.Step();
			report.Check(pack.GetCoins() == 100 && rig.State.Bin.PendingValue() == value && rig.Director.GetDayRollCount() == 0, "Economy: time passing in the day pays nothing");

			report.Check(rig.State.Bin.Retrieve(pack, HarvestOf(CropId::Parsnip), 4) == TradeResult::Ok && pack.Count(HarvestOf(CropId::Parsnip)) == 4 && pack.GetCoins() == 100
				&& rig.State.Bin.Held(HarvestOf(CropId::Parsnip)) == 6, "Economy: goods can be taken back out of the bin before the roll, without payment");
			rig.State.Bin.Ship(pack, HarvestOf(CropId::Parsnip), 4);

			rig.Sleep();
			report.Check(rig.Director.GetDayRollCount() == 1 && pack.GetCoins() == 100 + value && rig.State.Bin.IsEmpty() && rig.Economy.GetLastRoll().Shipped.Coins == value
				&& rig.Economy.GetLastRoll().Shipped.Items[static_cast<size_t>(HarvestOf(CropId::Parsnip))] == 10,
				"Economy: the bin pays at the day roll, then is empty");

			rig.Sleep();
			report.Check(pack.GetCoins() == 100 + value && rig.Economy.GetLastRoll().Shipped.Coins == 0, "Economy: an empty bin pays nothing the next night");

			EconomyRig late;
			late.State.Pack.AddCoins(1000);
			late.State.Pack.Add(ItemId::Honey, 3);
			late.State.Bin.Ship(late.State.Pack, ItemId::Honey, 3);
			const int honey = 3 * GetItemDef(ItemId::Honey).SellPrice;
			late.PassOut();
			const int lost = 1000 * PASSOUT_COIN_LOSS_PERMILLE / 1000;
			report.Check(late.Farm.GetLastRoll().Reason == DayRollReason::PassOut && late.Farm.GetLastRoll().CoinsLost == lost && late.State.Pack.GetCoins() == 1000 - lost + honey,
				"Economy: passing out takes its share of the coins in hand first, then the bin pays in full");

			Inventory crowded;
			crowded.Add(ItemId::Egg, 1);
			ShippingBin bin;
			bin.Ship(crowded, ItemId::Egg, 1);
			crowded.Add(ItemId::Wood, k_Capacity);
			report.Check(crowded.FreeSlots() == 0 && bin.Retrieve(crowded, ItemId::Egg, 1) == TradeResult::InventoryFull && bin.Held(ItemId::Egg) == 1, "Economy: goods cannot leave the bin into a full pack");
			const ShippingPayout paid = bin.Settle(crowded);
			report.Check(paid.Coins == GetItemDef(ItemId::Egg).SellPrice && crowded.GetCoins() == paid.Coins && bin.IsEmpty(), "Economy: coins fit a full pack, so the payout is never refused");
		}

		void CheckStateHash(CheckReport& report)
		{
			auto build = [](EconomyRig& rig)
			{
				rig.State.Pack.GiveStarterLoadout();
				rig.State.Pack.Add(HarvestOf(CropId::Parsnip), 12);
				rig.State.Bin.Ship(rig.State.Pack, HarvestOf(CropId::Parsnip), 7);
			};

			EconomyRig a;
			EconomyRig b;
			build(a);
			build(b);
			const GameStateDigest da = HashGameState(a.Director, a.State);
			const GameStateDigest db = HashGameState(b.Director, b.State);
			report.Check(da.All == db.All && da.Pack == db.Pack && da.Grid == db.Grid && da.Time == db.Time, "Economy: two identical games hash alike");

			for (int i = 0; i < 4; ++i)
			{
				a.Sleep();
				b.Sleep();
			}
			report.Check(HashGameState(a.Director, a.State).All == HashGameState(b.Director, b.State).All && HashGameState(a.Director, a.State).All != da.All, "Economy: four identical days keep them alike and move the hash");

			EconomyRig base;
			build(base);
			const GameStateDigest ref = HashGameState(base.Director, base.State);

			EconomyRig coins;
			build(coins);
			coins.State.Pack.AddCoins(1);
			EconomyRig stack;
			build(stack);
			stack.State.Pack.Add(ItemId::Egg, 1);
			EconomyRig plan;
			build(plan);
			plan.State.Pack.LearnAllPlans();
			EconomyRig select;
			build(select);
			select.State.Pack.CycleSelection(1);
			EconomyRig bin;
			build(bin);
			bin.State.Bin.Ship(bin.State.Pack, HarvestOf(CropId::Parsnip), 1);
			EconomyRig board;
			build(board);
			board.State.Board.NoteShipped(HarvestOf(CropId::Parsnip), 1, 0);
			EconomyRig festival;
			build(festival);
			festival.State.Board.NoteEarned(1, 0);

			const GameStateDigest dCoins = HashGameState(coins.Director, coins.State);
			const GameStateDigest dStack = HashGameState(stack.Director, stack.State);
			const GameStateDigest dPlan = HashGameState(plan.Director, plan.State);
			const GameStateDigest dSelect = HashGameState(select.Director, select.State);
			const GameStateDigest dBin = HashGameState(bin.Director, bin.State);
			const GameStateDigest dBoard = HashGameState(board.Director, board.State);
			report.Check(dCoins.Pack != ref.Pack && dStack.Pack != ref.Pack && dPlan.Pack != ref.Pack && dSelect.Pack != ref.Pack && dCoins.All != ref.All && dStack.All != ref.All,
				"Economy: the pack hash sees coins, an item, a learned plan and the selected slot");
			report.Check(dBin.Bin != ref.Bin && dBin.All != ref.All && dBin.Grid == ref.Grid && dBoard.Board != ref.Board && dBoard.All != ref.All && dBoard.Pack == ref.Pack,
				"Economy: the bin and the board each move their own hash and the total, and nothing else");
		}
	}

	void RunEconomyChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Economy");
		CheckInventoryStacks(report);
		CheckHotbar(report);
		CheckPrices(report);
		CheckShop(report);
		CheckPlanGate(report);
		CheckFullPickups(report);
		CheckShippingPayout(report);
		CheckStateHash(report);
	}

}
