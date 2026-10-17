#pragma once
#include "World/TileGrid.h"

#include <glm/glm.hpp>

namespace Dingo::PlayerMath
{

	inline constexpr float PI = 3.14159265f;

	float WrapAngle(float angle);
	float ApproachAngle(float current, float target, float maxStep);

	// Move is x right, y forward as the camera sees it; the result is a world (x, z) direction. The camera sits at
	// +z of its target when `cameraYaw` is 0 and looks toward -z.
	glm::vec2 CameraRelativeMove(const glm::vec2& move, float cameraYaw);

	// The yaw (about +y, 0 facing +z) that looks along a world (x, z) direction.
	float YawOf(const glm::vec2& direction);

	// Whether a walker can stand on the tile: in bounds, dry open ground, nothing solid on it.
	bool IsWalkable(const TileGrid& grid, int x, int y);

	// True when a disc of `radius` at world (x, z) overlaps no tile that is not walkable.
	bool IsClear(const TileGrid& grid, const glm::vec2& position, float radius);

	// The part of `delta` a disc can move from `position` without entering a tile that is not walkable: the x and z
	// parts are tried separately, so a wall slides the walker along it. A disc that already overlaps a blocked tile moves freely, so it can walk out.
	glm::vec2 ResolveMove(const TileGrid& grid, const glm::vec2& position, const glm::vec2& delta, float radius);

}
