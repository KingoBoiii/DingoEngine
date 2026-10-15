#pragma once
#include "Defs/BoardDefs.h"
#include "Farm/Economy.h"
#include "Placing/Inventory.h"
#include "World/TileGrid.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Dingo
{

	struct RequestState
	{
		int Progress = 0;
		int Baseline = 0;   // Build requests: buildings of the kind standing when the board was armed
		bool Done = false;
	};

	struct BoardState
	{
		std::array<RequestState, BOARD_REQUESTS> Requests{};
	};

	// NotYet until the four boards are done, Pending from that day until the mayor's scene has played (the layer calls
	// AcknowledgeFestival), Held for good afterwards: the sandbox, with harder boards.
	enum class FestivalPhase : uint8_t { NotYet, Pending, Held };

	struct CompletedRequest
	{
		int Board = 0;
		int Index = 0;
		int Reward = 0;
	};

	struct BoardReport
	{
		std::vector<CompletedRequest> Completed;
		int RewardCoins = 0;
		bool FestivalTriggered = false;
	};

	// The community board (plan 2.1): one board per season, four requests each, filled by delivery. A board opens when its
	// season starts and stays open until done. Rewards are paid by Settle, once per request. After the festival every
	// request comes back scaled by (1 + tier) in count and reward. Plain data; the sim owns one.
	class CommunityBoard
	{
	public:
		CommunityBoard();

		static constexpr int BoardCount = SEASON_BOARD_COUNT;

		bool IsOpen(int board, int absoluteDay) const;
		const RequestState& GetRequest(int board, int index) const { return m_Boards[static_cast<size_t>(board)].Requests[static_cast<size_t>(index)]; }
		// What the request asks for at this tier.
		int TargetCount(int board, int index) const;
		int RewardFor(int board, int index) const;
		bool IsBoardDone(int board) const;
		bool AllDone() const;
		int DoneCount() const;

		void NoteShipped(ItemId item, int count, int absoluteDay);
		// Coins earned by shipping, for "Earn N coins from shipping" requests.
		void NoteEarned(int coins, int absoluteDay);
		// Returns whether a request took the gift. Does not touch any inventory.
		bool NoteGift(VillagerId villager, ItemId item, int absoluteDay);
		// Recounts Build requests from the grid.
		void RefreshBuilds(const TileGrid& grid, int absoluteDay);

		// Refreshes builds, completes what is full, and reports the rewards; the caller banks RewardCoins.
		BoardReport Settle(const TileGrid& grid, int absoluteDay);

		FestivalPhase GetFestival() const { return m_Festival; }
		bool IsSandbox() const { return m_Festival == FestivalPhase::Held; }
		int GetTier() const { return m_Tier; }
		// Pending -> Held; re-arms every request one tier harder. Returns false in any other phase.
		bool AcknowledgeFestival(const TileGrid& grid);

		uint64_t Hash() const;

	private:
		friend struct SaveCodec;

	private:
		static int CountBuilt(const TileGrid& grid, const BoardRequest& request);
		void Arm(const TileGrid& grid);

	private:
		std::array<BoardState, SEASON_BOARD_COUNT> m_Boards{};
		FestivalPhase m_Festival = FestivalPhase::NotYet;
		int m_Tier = 0;
	};

	// Takes one `item` out of the inventory for `villager` and offers it to the board. Fails with NotEnough when none is held.
	TradeResult GiveGift(Inventory& inventory, CommunityBoard& board, VillagerId villager, ItemId item, int absoluteDay);

}
