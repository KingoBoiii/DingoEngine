#pragma once
#include "Farm/Board.h"
#include "GameTuning.h"
#include "World/TileGrid.h"
#include "World/ValleyLayout.h"

namespace Dingo
{

	// Fills board requests without the day roll, for the suites that need a board in a given state.
	namespace BoardFill
	{
		inline constexpr TileRect k_Plot{ 24, 42, 12, 12 };

		inline bool PlaceAnywhere(TileGrid& grid, BuildId id, const TileRect& area)
		{
			const BuildDef& def = GetBuildDef(id);
			for (int y = area.Y; y < area.Bottom(); ++y)
			{
				for (int x = area.X; x < area.Right(); ++x)
				{
					if (grid.IsAreaFree(x, y, def.Width, def.Height))
						return grid.AddOccupant(OccupantKind::Building, static_cast<uint16_t>(id), x, y, def.Width, def.Height, 0, 0) != k_NoOccupant;
				}
			}
			return false;
		}

		// Does what each request of `board` asks without touching the day roll.
		inline bool Fulfil(CommunityBoard& board, TileGrid& grid, int b, int day)
		{
			bool placed = true;
			for (int i = 0; i < BOARD_REQUESTS; ++i)
			{
				const BoardRequest& request = GetBoardDef(static_cast<Season>(b)).Requests[static_cast<size_t>(i)];
				const int target = board.TargetCount(b, i);
				switch (request.Kind)
				{
					case RequestKind::Ship:
						board.NoteShipped(request.Item, target, day);
						break;
					case RequestKind::EarnCoins:
						board.NoteEarned(target, day);
						break;
					case RequestKind::Gift:
						board.NoteGift(request.Villager, request.Item, day);
						break;
					case RequestKind::Build:
						for (int n = 0; n < target; ++n)
							placed = PlaceAnywhere(grid, request.Build, request.InSquare ? VILLAGE_SQUARE : k_Plot) && placed;
						break;
				}
			}
			return placed;
		}

	}

}
