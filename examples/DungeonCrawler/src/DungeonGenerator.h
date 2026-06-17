#pragma once
#include <glm/glm.hpp>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

// Procedural dungeon generator for the Dungeon Crawler. Produces the same kind of
// '#'/'.' tile map the example used to hand-author (see the old s_RoomLayout), so
// the existing tile rendering, collision (GameContext::IsSolidWorld /
// MoveWithCollision) and follow-camera all work on the result unchanged.
//
// Algorithm: the classic roguelike "rooms + corridors" pass. The grid starts solid;
// we place non-overlapping rectangular rooms at random, carving each to floor, and
// connect every new room to the previous one with an L-shaped corridor. Chaining
// each room to its predecessor guarantees the whole dungeon is one connected
// component, so the player can always walk to every room (and thus every enemy and
// every dropped gem). The player spawns in the first room; enemies are scattered
// across the others.
namespace Dingo
{

	struct DungeonParams
	{
		// NOTE on size: the GameLayer draws one quad entity per cell, so Width*Height
		// quads are submitted to Renderer2D every frame. The renderer auto-batches
		// (Renderer2DCapabilities::MaxQuads is a per-batch size, default 2000; overflow
		// just starts another batch — nothing is dropped), so this is a gameplay/perf
		// choice, not an engine limit. 38*24 = 912 keeps the map readable on one screen.
		int Width = 38;             // map size in tiles (border included)
		int Height = 24;
		int MaxRooms = 10;          // upper bound on rooms actually placed
		int RoomMin = 5;            // room side length range (inclusive)
		int RoomMax = 8;
		int MaxEnemiesPerRoom = 2;  // per non-player room (each gets at least 1)
		int MaxEnemiesTotal = 12;   // hard cap so big dungeons don't overwhelm
		unsigned int Seed = 0;      // 0 => pick a fresh random seed
	};

	struct GeneratedDungeon
	{
		std::vector<std::string> Map;       // '#' = wall, '.' = floor
		int Width = 0;
		int Height = 0;
		glm::ivec2 PlayerSpawn{ 0, 0 };     // tile (col, row)
		std::vector<glm::ivec2> EnemySpawns; // tiles (col, row)
		unsigned int Seed = 0;              // the seed actually used (handy to display)
	};

	inline GeneratedDungeon GenerateDungeon(DungeonParams params = {})
	{
		// --- Sanitize params so the distributions below are always valid. ---
		params.Width = std::clamp(params.Width, 12, 60);
		params.Height = std::clamp(params.Height, 12, 40);
		// Keep the map to a sane on-screen size. Renderer2D auto-batches now, so this is
		// a gameplay/readability bound (and keeps the draw-call count modest) rather than
		// the hard engine cap it once worked around.
		constexpr int kMaxCells = 950;
		if (params.Width * params.Height > kMaxCells)
			params.Height = kMaxCells / params.Width;
		params.MaxEnemiesPerRoom = std::max(params.MaxEnemiesPerRoom, 1); // dist needs a >= 1
		params.RoomMin = std::max(params.RoomMin, 3);
		// A room (plus its 1-tile wall border) must fit inside the map interior.
		params.RoomMax = std::clamp(params.RoomMax, params.RoomMin,
		                            std::min(params.Width - 2, params.Height - 2));

		const int W = params.Width;
		const int H = params.Height;

		GeneratedDungeon out;
		out.Width = W;
		out.Height = H;
		out.Seed = (params.Seed != 0) ? params.Seed : std::random_device{}();

		std::mt19937 rng(out.Seed);

		// Start fully solid; rooms and corridors are carved out of the rock.
		out.Map.assign(H, std::string(W, '#'));

		struct Rect
		{
			int x, y, w, h;
			int CenterX() const { return x + w / 2; }
			int CenterY() const { return y + h / 2; }
			// True if this rect, grown by a 1-tile margin, overlaps `o` — used to keep
			// at least one wall between rooms.
			bool TouchesWithMargin(const Rect& o) const
			{
				return x - 1 < o.x + o.w && o.x < x + w + 1 &&
				       y - 1 < o.y + o.h && o.y < y + h + 1;
			}
		};

		auto carveRoom = [&](const Rect& r)
		{
			for (int yy = r.y; yy < r.y + r.h; ++yy)
				for (int xx = r.x; xx < r.x + r.w; ++xx)
					out.Map[yy][xx] = '.';
		};
		auto carveH = [&](int x0, int x1, int y)
		{
			for (int xx = std::min(x0, x1); xx <= std::max(x0, x1); ++xx)
				out.Map[y][xx] = '.';
		};
		auto carveV = [&](int y0, int y1, int x)
		{
			for (int yy = std::min(y0, y1); yy <= std::max(y0, y1); ++yy)
				out.Map[yy][x] = '.';
		};

		std::vector<Rect> rooms;
		std::uniform_int_distribution<int> sizeDist(params.RoomMin, params.RoomMax);

		const int attempts = params.MaxRooms * 20; // give placement room to breathe
		for (int i = 0; i < attempts && (int)rooms.size() < params.MaxRooms; ++i)
		{
			const int rw = sizeDist(rng);
			const int rh = sizeDist(rng);
			// Keep a 1-tile wall border around the whole map: x in [1, W-1-rw].
			if (W - 1 - rw < 1 || H - 1 - rh < 1)
				continue;
			const int rx = std::uniform_int_distribution<int>(1, W - 1 - rw)(rng);
			const int ry = std::uniform_int_distribution<int>(1, H - 1 - rh)(rng);
			const Rect cand{ rx, ry, rw, rh };

			bool overlaps = false;
			for (const Rect& r : rooms)
			{
				if (cand.TouchesWithMargin(r)) { overlaps = true; break; }
			}
			if (overlaps)
				continue;

			carveRoom(cand);

			// Connect to the previously placed room with an L-shaped corridor. Randomize
			// which leg comes first so corridors aren't all elbowed the same way.
			if (!rooms.empty())
			{
				const Rect& prev = rooms.back();
				if (rng() & 1u)
				{
					carveH(prev.CenterX(), cand.CenterX(), prev.CenterY());
					carveV(prev.CenterY(), cand.CenterY(), cand.CenterX());
				}
				else
				{
					carveV(prev.CenterY(), cand.CenterY(), prev.CenterX());
					carveH(prev.CenterX(), cand.CenterX(), cand.CenterY());
				}
			}

			rooms.push_back(cand);
		}

		// Fallback: if placement somehow failed, carve a single central room so the
		// game always has a playable floor.
		if (rooms.empty())
		{
			const int rw = std::min(8, W - 2);
			const int rh = std::min(8, H - 2);
			const Rect r{ (W - rw) / 2, (H - rh) / 2, rw, rh };
			carveRoom(r);
			rooms.push_back(r);
		}

		// Belt-and-suspenders: rooms/corridors are kept off the edge, but re-seal the
		// border so the dungeon is guaranteed closed.
		for (int c = 0; c < W; ++c) { out.Map[0][c] = '#'; out.Map[H - 1][c] = '#'; }
		for (int r = 0; r < H; ++r) { out.Map[r][0] = '#'; out.Map[r][W - 1] = '#'; }

		// Player starts in the first room.
		out.PlayerSpawn = { rooms[0].CenterX(), rooms[0].CenterY() };

		// Scatter enemies across the remaining rooms (each non-player room gets at
		// least one, so no room clears for free), up to the global cap.
		for (size_t i = 1; i < rooms.size() && (int)out.EnemySpawns.size() < params.MaxEnemiesTotal; ++i)
		{
			const Rect& room = rooms[i];
			const int count = std::uniform_int_distribution<int>(1, params.MaxEnemiesPerRoom)(rng);
			std::uniform_int_distribution<int> exDist(room.x, room.x + room.w - 1);
			std::uniform_int_distribution<int> eyDist(room.y, room.y + room.h - 1);
			for (int k = 0; k < count && (int)out.EnemySpawns.size() < params.MaxEnemiesTotal; ++k)
				out.EnemySpawns.push_back({ exDist(rng), eyDist(rng) });
		}

		// Single-room dungeon (only the fallback / a 1-room roll): drop one enemy a few
		// tiles from the player so the room still has to be fought for, rather than
		// clearing the instant it loads.
		if (out.EnemySpawns.empty())
		{
			const Rect& room = rooms[0];
			out.EnemySpawns.push_back({ room.x, room.CenterY() }); // left edge of the room interior
		}

		return out;
	}

}
