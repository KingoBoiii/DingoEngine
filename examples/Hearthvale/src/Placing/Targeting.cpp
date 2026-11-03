#include "Placing/Targeting.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"

#include <cmath>
#include <cstdlib>

namespace Dingo::Targeting
{

	TileCoord TileOf(const glm::vec2& worldXZ)
	{
		return TileCoord{ static_cast<int>(std::floor(worldXZ.x / TILE_SIZE)), static_cast<int>(std::floor(worldXZ.y / TILE_SIZE)) };
	}

	glm::ivec2 FacingStep(float yaw)
	{
		const float x = std::sin(yaw);
		const float z = std::cos(yaw);
		if (std::abs(x) > std::abs(z))
			return glm::ivec2(x > 0.0f ? 1 : -1, 0);
		return glm::ivec2(0, z >= 0.0f ? 1 : -1);
	}

	TileCoord FacingTile(const glm::vec2& feet, float yaw)
	{
		const TileCoord own = TileOf(feet);
		const glm::ivec2 step = FacingStep(yaw);
		return TileCoord{ own.X + step.x, own.Y + step.y };
	}

	bool InReach(const TileCoord& farmer, const TileCoord& tile, int reachTiles)
	{
		return std::abs(tile.X - farmer.X) <= reachTiles && std::abs(tile.Y - farmer.Y) <= reachTiles;
	}

	float YawToward(const glm::vec2& feet, const TileCoord& tile, float fallback)
	{
		const glm::vec2 centre((static_cast<float>(tile.X) + 0.5f) * TILE_SIZE, (static_cast<float>(tile.Y) + 0.5f) * TILE_SIZE);
		const glm::vec2 delta = centre - feet;
		if (glm::dot(delta, delta) < 1e-6f)
			return fallback;
		return PlayerMath::YawOf(delta);
	}

	Aim Resolve(const AimInput& input, int reachTiles)
	{
		Aim aim;
		aim.Facing = FacingStep(input.Yaw);
		aim.Target = FacingTile(input.Feet, input.Yaw);
		aim.FaceYaw = input.Yaw;
		if (input.HasMouse)
		{
			const TileCoord hovered = TileOf(input.MouseGround);
			if (InReach(TileOf(input.Feet), hovered, reachTiles))
			{
				aim.Target = hovered;
				aim.FromMouse = true;
				aim.Facing = FacingStep(YawToward(input.Feet, hovered, input.Yaw));
				aim.FaceYaw = YawToward(input.Feet, hovered, input.Yaw);
			}
		}
		return aim;
	}

	TileCoord OriginInFront(const TileCoord& target, const glm::ivec2& facing, int w, int h)
	{
		TileCoord origin = target;
		if (facing.x != 0)
		{
			origin.Y = target.Y - h / 2;
			if (facing.x < 0)
				origin.X = target.X - (w - 1);
		}
		else
		{
			origin.X = target.X - w / 2;
			if (facing.y < 0)
				origin.Y = target.Y - (h - 1);
		}
		return origin;
	}

	TileCoord OriginCentred(const TileCoord& target, int w, int h)
	{
		return TileCoord{ target.X - w / 2, target.Y - h / 2 };
	}

}
