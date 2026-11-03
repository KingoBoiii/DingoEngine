#pragma once

#include <glm/glm.hpp>

namespace Dingo
{

	struct TileCoord
	{
		int X = 0;
		int Y = 0;

		bool operator==(const TileCoord&) const = default;
	};

	// What the farmer is aiming at this step. `MouseGround` is where the mouse ray met the ground plane, in world
	// (x, z), when the mouse is the active aim; the layer decides that and fills it.
	struct AimInput
	{
		glm::vec2 Feet{ 0.0f };
		float Yaw = 0.0f;
		bool HasMouse = false;
		glm::vec2 MouseGround{ 0.0f };
	};

	struct Aim
	{
		TileCoord Target;
		glm::ivec2 Facing{ 0, 1 };    // the cardinal step the farmer faces, in tiles (x, y)
		bool FromMouse = false;
		float FaceYaw = 0.0f;         // the yaw that looks at the target from the feet
	};

	namespace Targeting
	{
		TileCoord TileOf(const glm::vec2& worldXZ);

		// The cardinal step of the facing: yaw 0 is +y (world +z), a quarter turn is +x.
		glm::ivec2 FacingStep(float yaw);

		// The tile next to the farmer's own, in the facing direction.
		TileCoord FacingTile(const glm::vec2& feet, float yaw);

		bool InReach(const TileCoord& farmer, const TileCoord& tile, int reachTiles);

		// The yaw that looks from `feet` to the centre of `tile`; `fallback` when the farmer stands on its centre.
		float YawToward(const glm::vec2& feet, const TileCoord& tile, float fallback);

		// Mouse aim when the ground point is within `reachTiles` of the farmer's tile, else the tile in front.
		Aim Resolve(const AimInput& input, int reachTiles);

		// The footprint's minimum corner, so the footprint lies in front of the farmer and not on their tile:
		// centred across the facing direction, starting at the target along it.
		TileCoord OriginInFront(const TileCoord& target, const glm::ivec2& facing, int w, int h);

		// The footprint's minimum corner with the target tile at its centre (mouse aim).
		TileCoord OriginCentred(const TileCoord& target, int w, int h);
	}

}
