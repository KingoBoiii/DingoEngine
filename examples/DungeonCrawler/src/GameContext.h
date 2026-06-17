#pragma once
#include <DingoEngine.h>

#include "GameTuning.h"

#include <glm/glm.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace Dingo
{

	// Shared, game-wide state for the Dungeon Crawler. The GameLayer owns one
	// instance and passes a pointer to the scripts. It also owns the tile map and
	// the tile-collision helpers the player and enemies query while moving.
	struct GameContext
	{
		enum class GameState { Playing, Dead, Cleared };

		// Tile map ('#' = wall, '.' = floor). Owned here for collision queries.
		std::vector<std::string> Map;
		int MapWidth = 0;
		int MapHeight = 0;

		GameState State = GameState::Playing;
		Entity Player;
		float PlayerHealth = PLAYER_MAX_HEALTH;
		int LootCollected = 0;

		glm::vec2 TileToWorld(int col, int row) const
		{
			return glm::vec2(col * TILE_SIZE, -row * TILE_SIZE);
		}

		bool IsSolidTile(int col, int row) const
		{
			if (col < 0 || col >= MapWidth || row < 0 || row >= MapHeight)
				return true;
			return Map[row][col] == '#';
		}

		bool IsSolidWorld(float x, float y) const
		{
			const int col = (int)std::floor(x / TILE_SIZE + 0.5f);
			const int row = (int)std::floor(-y / TILE_SIZE + 0.5f);
			return IsSolidTile(col, row);
		}

		// Axis-separated AABB-vs-tile sweep. Valid while half-size < one tile.
		glm::vec2 MoveWithCollision(const glm::vec2& pos, const glm::vec2& delta, float half) const
		{
			const float eps = 0.001f;
			glm::vec2 result = pos;

			if (delta.x != 0.0f)
			{
				const float nx = result.x + delta.x;
				const float edgeX = nx + (delta.x > 0.0f ? half : -half);
				if (!IsSolidWorld(edgeX, result.y - half + eps) &&
				    !IsSolidWorld(edgeX, result.y + half - eps))
				{
					result.x = nx;
				}
			}

			if (delta.y != 0.0f)
			{
				const float ny = result.y + delta.y;
				const float edgeY = ny + (delta.y > 0.0f ? half : -half);
				if (!IsSolidWorld(result.x - half + eps, edgeY) &&
				    !IsSolidWorld(result.x + half - eps, edgeY))
				{
					result.y = ny;
				}
			}

			return result;
		}
	};

}
