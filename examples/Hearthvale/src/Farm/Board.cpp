#include "Farm/Board.h"
#include "Farm/Calendar.h"
#include "Farm/Economy.h"
#include "Sim/Fnv.h"
#include "World/ValleyLayout.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		const BoardRequest& Def(int board, int index)
		{
			return GetBoardDef(static_cast<Season>(board)).Requests[static_cast<size_t>(index)];
		}
	}

	CommunityBoard::CommunityBoard() = default;

	bool CommunityBoard::IsOpen(int board, int absoluteDay) const
	{
		return m_Tier > 0 || absoluteDay >= board * DAYS_PER_SEASON;
	}

	int CommunityBoard::TargetCount(int board, int index) const
	{
		return Def(board, index).Count * (1 + m_Tier);
	}

	int CommunityBoard::RewardFor(int board, int index) const
	{
		return Def(board, index).RewardCoins * (1 + m_Tier);
	}

	bool CommunityBoard::IsBoardDone(int board) const
	{
		for (const RequestState& request : m_Boards[static_cast<size_t>(board)].Requests)
		{
			if (!request.Done)
				return false;
		}
		return true;
	}

	bool CommunityBoard::AllDone() const
	{
		for (int b = 0; b < BoardCount; ++b)
		{
			if (!IsBoardDone(b))
				return false;
		}
		return true;
	}

	int CommunityBoard::DoneCount() const
	{
		int done = 0;
		for (const BoardState& board : m_Boards)
		{
			for (const RequestState& request : board.Requests)
				done += request.Done ? 1 : 0;
		}
		return done;
	}

	void CommunityBoard::NoteShipped(ItemId item, int count, int absoluteDay)
	{
		if (count <= 0)
			return;
		for (int b = 0; b < BoardCount; ++b)
		{
			if (!IsOpen(b, absoluteDay))
				continue;
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& def = Def(b, i);
				RequestState& state = m_Boards[static_cast<size_t>(b)].Requests[static_cast<size_t>(i)];
				if (def.Kind == RequestKind::Ship && def.Item == item && !state.Done)
					state.Progress = (std::min)(state.Progress + count, TargetCount(b, i));
			}
		}
	}

	void CommunityBoard::NoteEarned(int coins, int absoluteDay)
	{
		if (coins <= 0)
			return;
		for (int b = 0; b < BoardCount; ++b)
		{
			if (!IsOpen(b, absoluteDay))
				continue;
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				RequestState& state = m_Boards[static_cast<size_t>(b)].Requests[static_cast<size_t>(i)];
				if (Def(b, i).Kind == RequestKind::EarnCoins && !state.Done)
					state.Progress = (std::min)(state.Progress + coins, TargetCount(b, i));
			}
		}
	}

	bool CommunityBoard::NoteGift(VillagerId villager, ItemId item, int absoluteDay)
	{
		bool taken = false;
		for (int b = 0; b < BoardCount; ++b)
		{
			if (!IsOpen(b, absoluteDay))
				continue;
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& def = Def(b, i);
				RequestState& state = m_Boards[static_cast<size_t>(b)].Requests[static_cast<size_t>(i)];
				if (def.Kind == RequestKind::Gift && def.Villager == villager && def.Item == item && !state.Done)
				{
					state.Progress = (std::min)(state.Progress + 1, TargetCount(b, i));
					taken = true;
				}
			}
		}
		return taken;
	}

	int CommunityBoard::CountBuilt(const TileGrid& grid, const BoardRequest& request)
	{
		int count = 0;
		for (const Occupant& occupant : grid.GetOccupants())
		{
			if (occupant.Kind != OccupantKind::Building || occupant.DefId != static_cast<uint16_t>(request.Build))
				continue;
			if (!request.InSquare || VILLAGE_SQUARE.Contains(occupant.X, occupant.Y))
				++count;
		}
		const uint8_t built = static_cast<uint8_t>(static_cast<int>(request.Build) + 1);
		for (int y = 0; y < grid.GetHeight(); ++y)
		{
			for (int x = 0; x < grid.GetWidth(); ++x)
			{
				if (grid.At(x, y).Built == built && (!request.InSquare || VILLAGE_SQUARE.Contains(x, y)))
					++count;
			}
		}
		return count;
	}

	void CommunityBoard::RefreshBuilds(const TileGrid& grid, int absoluteDay)
	{
		for (int b = 0; b < BoardCount; ++b)
		{
			if (!IsOpen(b, absoluteDay))
				continue;
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& def = Def(b, i);
				RequestState& state = m_Boards[static_cast<size_t>(b)].Requests[static_cast<size_t>(i)];
				if (def.Kind != RequestKind::Build || state.Done)
					continue;
				const int built = CountBuilt(grid, def) - state.Baseline;
				state.Progress = (std::clamp)(built, 0, TargetCount(b, i));
			}
		}
	}

	BoardReport CommunityBoard::Settle(const TileGrid& grid, int absoluteDay)
	{
		RefreshBuilds(grid, absoluteDay);
		BoardReport report;
		for (int b = 0; b < BoardCount; ++b)
		{
			if (!IsOpen(b, absoluteDay))
				continue;
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				RequestState& state = m_Boards[static_cast<size_t>(b)].Requests[static_cast<size_t>(i)];
				if (state.Done || state.Progress < TargetCount(b, i))
					continue;
				state.Done = true;
				const int reward = RewardFor(b, i);
				report.Completed.push_back(CompletedRequest{ b, i, reward });
				report.RewardCoins += reward;
			}
		}
		if (m_Festival == FestivalPhase::NotYet && AllDone())
		{
			m_Festival = FestivalPhase::Pending;
			report.FestivalTriggered = true;
		}
		return report;
	}

	void CommunityBoard::Arm(const TileGrid& grid)
	{
		for (int b = 0; b < BoardCount; ++b)
		{
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& def = Def(b, i);
				RequestState& state = m_Boards[static_cast<size_t>(b)].Requests[static_cast<size_t>(i)];
				state = RequestState{};
				if (def.Kind == RequestKind::Build)
					state.Baseline = CountBuilt(grid, def);
			}
		}
	}

	bool CommunityBoard::AcknowledgeFestival(const TileGrid& grid)
	{
		if (m_Festival != FestivalPhase::Pending)
			return false;
		m_Festival = FestivalPhase::Held;
		++m_Tier;
		Arm(grid);
		return true;
	}

	uint64_t CommunityBoard::Hash() const
	{
		Fnv1a hash;
		hash.Byte(static_cast<uint8_t>(m_Festival));
		hash.I32(m_Tier);
		for (const BoardState& board : m_Boards)
		{
			for (const RequestState& request : board.Requests)
			{
				hash.I32(request.Progress);
				hash.I32(request.Baseline);
				hash.Byte(request.Done ? 1 : 0);
			}
		}
		return hash.Value();
	}

	TradeResult GiveGift(Inventory& inventory, CommunityBoard& board, VillagerId villager, ItemId item, int absoluteDay)
	{
		if (!inventory.Remove(item, 1))
			return TradeResult::NotEnough;
		board.NoteGift(villager, item, absoluteDay);
		return TradeResult::Ok;
	}

}
