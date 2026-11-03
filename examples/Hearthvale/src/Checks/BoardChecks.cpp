#include "Checks/Checks.h"
#include "Checks/BoardFill.h"
#include "Checks/EconomyRig.h"
#include "Farm/Board.h"
#include "GameTuning.h"
#include "World/Valley.h"
#include "World/ValleyLayout.h"

#include <format>
#include <vector>

namespace Dingo
{

	namespace
	{
		using namespace BoardFill;

		int TotalRewards(const CommunityBoard& board)
		{
			int total = 0;
			for (int b = 0; b < CommunityBoard::BoardCount; ++b)
			{
				for (int i = 0; i < BOARD_REQUESTS; ++i)
					total += board.RewardFor(b, i);
			}
			return total;
		}

		void CheckDefsAndOpening(CheckReport& report)
		{
			bool shape = true;
			for (int s = 0; s < SEASON_BOARD_COUNT; ++s)
				shape = shape && GetBoardDef(static_cast<Season>(s)).BoardSeason == static_cast<Season>(s);
			CommunityBoard board;
			report.Check(shape && CommunityBoard::BoardCount == 4 && BOARD_REQUESTS == 4 && board.DoneCount() == 0 && !board.AllDone() && board.GetFestival() == FestivalPhase::NotYet && !board.IsSandbox(),
				"Board: four boards of four requests, one per season, nothing done at the start");
			report.Check(board.IsOpen(0, 0) && !board.IsOpen(1, 0) && !board.IsOpen(1, DAYS_PER_SEASON - 1) && board.IsOpen(1, DAYS_PER_SEASON) && board.IsOpen(3, DAYS_PER_YEAR - 1),
				"Board: a board opens when its season starts");
			report.Check(board.TargetCount(0, 0) == 20 && board.RewardFor(0, 0) == 300, "Board: targets and rewards read the defs at the first tier");
		}

		void CheckShipRequests(CheckReport& report)
		{
			EconomyRig rig;
			Inventory& pack = rig.State.Pack;
			const ItemId parsnip = HarvestItem(CropId::Parsnip);

			pack.Add(parsnip, 12);
			rig.State.Bin.Ship(pack, parsnip, 12);
			report.Check(rig.State.Board.GetRequest(0, 0).Progress == 0, "Board: goods waiting in the bin do not count yet");
			rig.Sleep();
			report.Check(rig.State.Board.GetRequest(0, 0).Progress == 12 && !rig.State.Board.GetRequest(0, 0).Done && rig.Economy.GetLastRoll().Board.Completed.empty(),
				"Board: shipping counts at the day roll and a partial delivery completes nothing");

			const int coinsBefore = pack.GetCoins();
			pack.Add(parsnip, 12);
			rig.State.Bin.Ship(pack, parsnip, 12);
			rig.Sleep();
			const BoardReport& done = rig.Economy.GetLastRoll().Board;
			report.Check(rig.State.Board.GetRequest(0, 0).Done && rig.State.Board.GetRequest(0, 0).Progress == 20 && done.Completed.size() >= 1 && done.Completed[0].Board == 0 && done.Completed[0].Index == 0
				&& done.Completed[0].Reward == 300 && pack.GetCoins() == coinsBefore + rig.Economy.GetLastRoll().Shipped.Coins + done.RewardCoins,
				"Board: the delivery that fills a request completes it and the reward is banked with the payout");

			const int afterReward = pack.GetCoins();
			pack.Add(parsnip, 30);
			rig.State.Bin.Ship(pack, parsnip, 30);
			rig.Sleep();
			bool repaid = false;
			for (const CompletedRequest& completed : rig.Economy.GetLastRoll().Board.Completed)
				repaid = repaid || (completed.Board == 0 && completed.Index == 0);
			report.Check(!repaid && pack.GetCoins() == afterReward + rig.Economy.GetLastRoll().Shipped.Coins + rig.Economy.GetLastRoll().Board.RewardCoins && rig.State.Board.GetRequest(0, 0).Progress == 20,
				"Board: a completed request pays once and takes no more deliveries");

			EconomyRig early;
			const ItemId tomato = HarvestItem(CropId::Tomato);
			early.State.Pack.Add(tomato, 15);
			early.State.Bin.Ship(early.State.Pack, tomato, 15);
			early.Sleep();
			report.Check(early.State.Board.GetRequest(1, 0).Progress == 0 && !early.State.Board.GetRequest(1, 0).Done, "Board: summer's request takes nothing shipped in spring");

			EconomyRig summer(DAYS_PER_SEASON);
			summer.State.Pack.Add(tomato, 15);
			summer.State.Bin.Ship(summer.State.Pack, tomato, 15);
			summer.Sleep();
			report.Check(summer.State.Board.GetRequest(1, 0).Done && summer.Economy.GetLastRoll().Board.RewardCoins >= 350, "Board: in summer the same delivery fills the summer request");

			EconomyRig earn;
			earn.State.Pack.Add(ItemId::Honey, 14);
			earn.State.Bin.Ship(earn.State.Pack, ItemId::Honey, 14);
			earn.Sleep();
			const RequestState& partial = earn.State.Board.GetRequest(0, 3);
			report.Check(partial.Progress == 14 * GetItemDef(ItemId::Honey).SellPrice && !partial.Done, "Board: earning counts the coins the bin paid");
			earn.State.Pack.Add(ItemId::Honey, 1);
			earn.State.Bin.Ship(earn.State.Pack, ItemId::Honey, 1);
			earn.Sleep();
			report.Check(earn.State.Board.GetRequest(0, 3).Done && earn.State.Board.GetRequest(0, 3).Progress == 1500, "Board: earning 1500 coins from shipping fills its request, capped at the target");

			EconomyRig shopSale;
			shopSale.State.Pack.Add(ItemId::Honey, 20);
			ShopSellItem(shopSale.State.Pack, ItemId::Honey, 20);
			shopSale.Sleep();
			report.Check(shopSale.State.Board.GetRequest(0, 3).Progress == 0, "Board: selling at the shop does not count as earning from shipping");
		}

		void CheckBuildRequests(CheckReport& report)
		{
			EconomyRig rig;
			TileGrid& grid = rig.Director.GetGrid();
			bool all = true;
			for (int i = 0; i < 9; ++i)
				all = PlaceAnywhere(grid, BuildId::WoodFence, k_Plot) && all;
			rig.Sleep();
			report.Check(all && rig.State.Board.GetRequest(0, 1).Progress == 9 && !rig.State.Board.GetRequest(0, 1).Done, "Board: standing fences count toward the build request");

			const int coins = rig.State.Pack.GetCoins();
			PlaceAnywhere(grid, BuildId::WoodFence, k_Plot);
			rig.Sleep();
			report.Check(rig.State.Board.GetRequest(0, 1).Done && rig.State.Pack.GetCoins() == coins + 200 && rig.Economy.GetLastRoll().Board.RewardCoins == 200, "Board: the tenth fence completes the request and pays its reward");

			for (const Occupant& occupant : std::vector<Occupant>(grid.GetOccupants()))
			{
				if (occupant.DefId == static_cast<uint16_t>(BuildId::WoodFence))
					grid.RemoveOccupant(occupant.Id);
			}
			rig.Sleep();
			report.Check(rig.State.Board.GetRequest(0, 1).Done && rig.State.Board.GetRequest(0, 1).Progress == 10, "Board: a request done stays done when the buildings come down");

			EconomyRig square(DAYS_PER_SEASON);
			TileGrid& sq = square.Director.GetGrid();
			for (int i = 0; i < 3; ++i)
				PlaceAnywhere(sq, BuildId::LampPost, k_Plot);
			square.Sleep();
			report.Check(square.State.Board.GetRequest(1, 1).Progress == 0, "Board: lamp posts outside the square do not count toward a square request");
			bool inSquare = true;
			for (int i = 0; i < 3; ++i)
				inSquare = PlaceAnywhere(sq, BuildId::LampPost, VILLAGE_SQUARE) && inSquare;
			square.Sleep();
			report.Check(inSquare && square.State.Board.GetRequest(1, 1).Done && square.Economy.GetLastRoll().Board.RewardCoins >= 300, "Board: three lamp posts in the square complete the request");
		}

		void CheckGiftRequests(CheckReport& report)
		{
			EconomyRig rig;
			Inventory& pack = rig.State.Pack;
			CommunityBoard& board = rig.State.Board;
			const ItemId strawberry = HarvestItem(CropId::Strawberry);

			report.Check(GiveGift(pack, board, VillagerId::Child, strawberry, rig.Day()) == TradeResult::NotEnough && board.GetRequest(0, 2).Progress == 0, "Board: a gift needs the item in hand");
			pack.Add(strawberry, 2);
			report.Check(GiveGift(pack, board, VillagerId::Mayor, strawberry, rig.Day()) == TradeResult::Ok && pack.Count(strawberry) == 1 && board.GetRequest(0, 2).Progress == 0,
				"Board: a gift to someone else is handed over but fills nothing");
			report.Check(GiveGift(pack, board, VillagerId::Child, strawberry, rig.Day()) == TradeResult::Ok && pack.Count(strawberry) == 0 && board.GetRequest(0, 2).Progress == 1,
				"Board: the right gift to the right villager fills the request");
			const int coins = pack.GetCoins();
			rig.Sleep();
			report.Check(board.GetRequest(0, 2).Done && pack.GetCoins() == coins + 150, "Board: the gift request pays at the day roll");

			pack.Add(strawberry, 1);
			GiveGift(pack, board, VillagerId::Child, strawberry, rig.Day());
			report.Check(board.GetRequest(0, 2).Progress == 1 && !board.NoteGift(VillagerId::Child, strawberry, rig.Day()), "Board: a given request takes no second gift");
		}

		void CheckFestival(CheckReport& report)
		{
			TileGrid grid(VALLEY_SIZE, VALLEY_SIZE);
			CommunityBoard threeOnly;
			bool placed = true;
			for (int b = 0; b < 3; ++b)
				placed = Fulfil(threeOnly, grid, b, DAYS_PER_YEAR - 1) && placed;
			const BoardReport three = threeOnly.Settle(grid, DAYS_PER_YEAR - 1);
			report.Check(placed && three.Completed.size() == 12 && threeOnly.DoneCount() == 12 && !threeOnly.AllDone() && !three.FestivalTriggered && threeOnly.GetFestival() == FestivalPhase::NotYet,
				"Board: three boards done do not hold the festival");

			EconomyRig rig(DAYS_PER_YEAR - 1);
			CommunityBoard& board = rig.State.Board;
			TileGrid& valley = rig.Director.GetGrid();
			placed = true;
			for (int b = 0; b < 3; ++b)
				placed = Fulfil(board, valley, b, rig.Day()) && placed;
			rig.Sleep();
			report.Check(placed && board.DoneCount() == 12 && board.GetFestival() == FestivalPhase::NotYet && !rig.Economy.GetLastRoll().Board.FestivalTriggered, "Board: the day roll settles three boards and no festival");

			Fulfil(board, valley, 3, rig.Day());
			const int coins = rig.State.Pack.GetCoins();
			rig.Sleep();
			const BoardReport& last = rig.Economy.GetLastRoll().Board;
			report.Check(board.AllDone() && last.FestivalTriggered && board.GetFestival() == FestivalPhase::Pending && !board.IsSandbox() && last.Completed.size() == 4 && rig.State.Pack.GetCoins() == coins + last.RewardCoins,
				"Board: the fourth board done triggers the festival at the day roll and pays its rewards");

			rig.Sleep();
			report.Check(board.GetFestival() == FestivalPhase::Pending && !rig.Economy.GetLastRoll().Board.FestivalTriggered && rig.Economy.GetLastRoll().Board.RewardCoins == 0, "Board: the festival is triggered once and nothing is paid twice");

			report.Check(board.AcknowledgeFestival(valley) && board.GetFestival() == FestivalPhase::Held && board.IsSandbox() && board.GetTier() == 1 && board.DoneCount() == 0,
				"Board: after the festival the game goes on as a sandbox with every request armed again");
			report.Check(!board.AcknowledgeFestival(valley) && board.GetTier() == 1, "Board: the festival is acknowledged once");

			report.Check(board.TargetCount(0, 0) == 40 && board.RewardFor(0, 0) == 600 && board.TargetCount(0, 1) == 20 && board.GetRequest(0, 1).Progress == 0 && board.GetRequest(0, 1).Baseline == 10,
				"Board: sandbox requests ask double and pay double, and existing buildings do not count");

			const int fences = board.TargetCount(0, 1);
			bool more = true;
			for (int n = 0; n < fences; ++n)
				more = PlaceAnywhere(valley, BuildId::WoodFence, k_Plot) && more;
			rig.State.Pack.Add(HarvestItem(CropId::Parsnip), 40);
			rig.State.Bin.Ship(rig.State.Pack, HarvestItem(CropId::Parsnip), 40);
			const int before = rig.State.Pack.GetCoins();
			rig.Sleep();
			const BoardReport& sandbox = rig.Economy.GetLastRoll().Board;
			bool rewarded600 = false;
			bool fence400 = false;
			for (const CompletedRequest& completed : sandbox.Completed)
			{
				rewarded600 = rewarded600 || (completed.Board == 0 && completed.Index == 0 && completed.Reward == 600);
				fence400 = fence400 || (completed.Board == 0 && completed.Index == 1 && completed.Reward == 400);
			}
			report.Check(more && rewarded600 && fence400 && !sandbox.FestivalTriggered && board.GetFestival() == FestivalPhase::Held
				&& rig.State.Pack.GetCoins() == before + rig.Economy.GetLastRoll().Shipped.Coins + sandbox.RewardCoins,
				"Board: sandbox requests fill and pay at the new scale without a second festival");
		}

		void CheckBoardState(CheckReport& report)
		{
			EconomyRig a;
			EconomyRig b;
			report.Check(a.State.Board.Hash() == b.State.Board.Hash(), "Board: fresh boards hash alike");
			a.State.Board.NoteShipped(HarvestItem(CropId::Parsnip), 1, 0);
			const uint64_t progressed = a.State.Board.Hash();
			report.Check(progressed != b.State.Board.Hash(), "Board: progress changes the hash");

			EconomyRig c(DAYS_PER_YEAR - 1);
			TileGrid& grid = c.Director.GetGrid();
			for (int s = 0; s < CommunityBoard::BoardCount; ++s)
				Fulfil(c.State.Board, grid, s, c.Day());
			c.Sleep();
			const uint64_t pending = c.State.Board.Hash();
			c.State.Board.AcknowledgeFestival(grid);
			report.Check(pending != c.State.Board.Hash() && pending != CommunityBoard().Hash(), "Board: the festival flag and the tier change the hash");
		}
	}

	void RunBoardChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Board");
		CheckDefsAndOpening(report);
		CheckShipRequests(report);
		CheckBuildRequests(report);
		CheckGiftRequests(report);
		CheckFestival(report);
		CheckBoardState(report);
	}

}
