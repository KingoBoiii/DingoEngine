#include "Life/PlayerMath.h"
#include "Defs/BuildDefs.h"
#include "Defs/DecorDefs.h"
#include "GameTuning.h"

#include <algorithm>
#include <cmath>

namespace Dingo::PlayerMath
{

	namespace
	{
		bool IsSolidDecor(uint16_t kind)
		{
			switch (static_cast<DecorKind>(kind))
			{
				case DecorKind::Tree:
				case DecorKind::Pine:
				case DecorKind::Bush:
				case DecorKind::Rock:
				case DecorKind::Stump:
					return true;
				default:
					return false;
			}
		}

		bool DiscTouchesTile(const glm::vec2& position, float radius, int tx, int ty)
		{
			const float minX = static_cast<float>(tx) * TILE_SIZE;
			const float minZ = static_cast<float>(ty) * TILE_SIZE;
			const float nearestX = std::clamp(position.x, minX, minX + TILE_SIZE);
			const float nearestZ = std::clamp(position.y, minZ, minZ + TILE_SIZE);
			const glm::vec2 offset = position - glm::vec2(nearestX, nearestZ);
			return glm::dot(offset, offset) < radius * radius;
		}
	}

	float WrapAngle(float angle)
	{
		angle = std::fmod(angle, 2.0f * PI);
		if (angle > PI)
			angle -= 2.0f * PI;
		else if (angle < -PI)
			angle += 2.0f * PI;
		return angle;
	}

	float ApproachAngle(float current, float target, float maxStep)
	{
		return WrapAngle(current + std::clamp(WrapAngle(target - current), -maxStep, maxStep));
	}

	glm::vec2 CameraRelativeMove(const glm::vec2& move, float cameraYaw)
	{
		const float sine = std::sin(cameraYaw);
		const float cosine = std::cos(cameraYaw);
		const glm::vec2 forward(-sine, -cosine);
		const glm::vec2 right(cosine, -sine);
		return right * move.x + forward * move.y;
	}

	float YawOf(const glm::vec2& direction)
	{
		return std::atan2(direction.x, direction.y);
	}

	bool IsWalkable(const TileGrid& grid, int x, int y)
	{
		if (!grid.InBounds(x, y))
			return false;

		const Tile& tile = grid.At(x, y);
		if (tile.GroundState == Ground::Water || tile.GroundState == Ground::Blocked)
			return false;
		if (tile.Occupant == k_NoOccupant)
			return true;

		const Occupant* occupant = grid.FindOccupant(tile.Occupant);
		if (!occupant)
			return true;

		switch (occupant->Kind)
		{
			case OccupantKind::Decoration: return !IsSolidDecor(occupant->DefId);
			case OccupantKind::Building:   return !GetBuildDef(static_cast<BuildId>(occupant->DefId)).Blocking;
			case OccupantKind::Landmark:   return false;
		}
		return true;
	}

	bool IsClear(const TileGrid& grid, const glm::vec2& position, float radius)
	{
		const int minX = static_cast<int>(std::floor((position.x - radius) / TILE_SIZE));
		const int maxX = static_cast<int>(std::floor((position.x + radius) / TILE_SIZE));
		const int minY = static_cast<int>(std::floor((position.y - radius) / TILE_SIZE));
		const int maxY = static_cast<int>(std::floor((position.y + radius) / TILE_SIZE));
		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				if (!IsWalkable(grid, x, y) && DiscTouchesTile(position, radius, x, y))
					return false;
			}
		}
		return true;
	}

	glm::vec2 ResolveMove(const TileGrid& grid, const glm::vec2& position, const glm::vec2& delta, float radius)
	{
		if (!IsClear(grid, position, radius))
			return delta;

		glm::vec2 allowed(0.0f);
		if (IsClear(grid, position + glm::vec2(delta.x, 0.0f), radius))
			allowed.x = delta.x;
		if (IsClear(grid, position + glm::vec2(allowed.x, delta.y), radius))
			allowed.y = delta.y;
		return allowed;
	}

}
