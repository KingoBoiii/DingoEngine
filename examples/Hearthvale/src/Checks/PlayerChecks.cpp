#include "Checks/Checks.h"
#include "GameTuning.h"
#include "Life/PlayerInput.h"
#include "Life/PlayerMath.h"
#include "Look/CameraMath.h"
#include "World/TileGrid.h"
#include "World/Valley.h"
#include "World/ValleyLayout.h"

#include <cmath>

namespace Dingo
{

	namespace
	{
		constexpr float k_Eps = 1e-4f;

		bool Near(float a, float b, float eps = k_Eps)
		{
			return std::abs(a - b) <= eps;
		}

		bool Near(const glm::vec2& a, const glm::vec2& b, float eps = k_Eps)
		{
			return Near(a.x, b.x, eps) && Near(a.y, b.y, eps);
		}

		void CheckMovement(CheckReport& report)
		{
			using namespace PlayerMath;
			const float quarter = 0.5f * PI;

			report.Check(Near(CameraRelativeMove({ 0.0f, 1.0f }, 0.0f), { 0.0f, -1.0f }) && Near(CameraRelativeMove({ 1.0f, 0.0f }, 0.0f), { 1.0f, 0.0f }),
				"Player: at camera yaw 0, forward is -z and right is +x");
			report.Check(Near(CameraRelativeMove({ 0.0f, 1.0f }, quarter), { -1.0f, 0.0f }) && Near(CameraRelativeMove({ 1.0f, 0.0f }, quarter), { 0.0f, -1.0f }),
				"Player: a camera a quarter turn round has forward -x and right -z");
			report.Check(Near(glm::length(CameraRelativeMove(glm::normalize(glm::vec2(1.0f, 1.0f)), 0.7f)), 1.0f), "Player: camera-relative movement keeps its length");

			report.Check(Near(YawOf({ 0.0f, 1.0f }), 0.0f) && Near(YawOf({ 1.0f, 0.0f }), quarter) && Near(std::abs(YawOf({ 0.0f, -1.0f })), PI),
				"Player: yaw 0 faces +z and a quarter turn faces +x");
			report.Check(Near(WrapAngle(2.0f * PI + 0.5f), 0.5f) && Near(WrapAngle(-2.0f * PI - 0.5f), -0.5f), "Player: wrapped angles land in -pi..pi");
			report.Check(Near(ApproachAngle(3.0f, -3.0f, 0.1f), 3.1f) && Near(ApproachAngle(0.0f, 0.05f, 0.1f), 0.05f),
				"Player: yaw turns the short way, at a limited rate, without overshooting");

			report.Check(Near(CombineMove({ 1.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f), { 1.0f, 0.0f }) && Near(glm::length(CombineMove({ 1.0f, 1.0f }, { 0.0f, 0.0f }, 0.0f)), 1.0f)
				&& Near(CombineMove({ 0.0f, 0.0f }, { 0.05f, 0.0f }, 0.1f), { 0.0f, 0.0f }) && Near(CombineMove({ 0.0f, 0.0f }, { 0.5f, 0.0f }, 0.1f), { 0.5f, 0.0f }),
				"Player: keys and stick mix to at most unit length, and a stick inside the deadzone is ignored");
		}

		void CheckWalking(CheckReport& report)
		{
			TileGrid grid(8, 8);
			for (int y = 0; y < 8; ++y)
				grid.SetGround(4, y, Ground::Water);

			const float radius = PLAYER_RADIUS;
			const glm::vec2 open = PlayerMath::ResolveMove(grid, { 2.5f, 3.5f }, { 0.4f, 0.0f }, radius);
			report.Check(Near(open, { 0.4f, 0.0f }), "Player: open ground lets the farmer move the whole step");

			const glm::vec2 blocked = PlayerMath::ResolveMove(grid, { 3.7f, 3.5f }, { 0.2f, 0.0f }, radius);
			report.Check(Near(blocked, { 0.0f, 0.0f }), "Player: water stops the farmer at its edge");

			const glm::vec2 slide = PlayerMath::ResolveMove(grid, { 3.7f, 3.5f }, { 0.2f, 0.3f }, radius);
			report.Check(Near(slide, { 0.0f, 0.3f }), "Player: a wall slides the farmer along it");

			const glm::vec2 escape = PlayerMath::ResolveMove(grid, { 4.5f, 3.5f }, { -0.1f, 0.0f }, radius);
			report.Check(Near(escape, { -0.1f, 0.0f }), "Player: a farmer already overlapping a blocked tile can walk out");

			report.Check(!PlayerMath::IsWalkable(grid, -1, 0) && !PlayerMath::IsWalkable(grid, 8, 0) && PlayerMath::IsWalkable(grid, 3, 3),
				"Player: tiles outside the grid are not walkable");

			TileGrid valley(VALLEY_SIZE, VALLEY_SIZE);
			GenerateValley(valley, 1);
			const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
			const int spawnX = home.DoorX;
			const int spawnY = home.DoorY + 1;
			report.Check(PlayerMath::IsClear(valley, { (static_cast<float>(spawnX) + 0.5f) * TILE_SIZE, (static_cast<float>(spawnY) + 0.5f) * TILE_SIZE }, PLAYER_RADIUS),
				"Player: the spawn in front of the farmhouse door is clear ground");
			report.Check(!PlayerMath::IsWalkable(valley, 0, 0) && !PlayerMath::IsWalkable(valley, POND_CENTER_X, POND_CENTER_Y)
				&& !PlayerMath::IsWalkable(valley, home.Footprint.X + 1, home.Footprint.Y + 1),
				"Player: the rim, the pond and the farmhouse are not walkable in a generated valley");
		}

		void CheckCamera(CheckReport& report)
		{
			const float quarter = 0.5f * PlayerMath::PI;
			report.Check(Near(CameraMath::YawForSteps(0), 0.0f) && Near(CameraMath::YawForSteps(1), quarter) && Near(CameraMath::YawForSteps(-1), -quarter)
				&& Near(CameraMath::YawForSteps(4), 0.0f) && Near(CameraMath::YawForSteps(-3), quarter),
				"Camera: each step is a 90 degree turn and four steps come back round");

			const float easedOnce = CameraMath::EaseYaw(0.0f, quarter, 1.0f / 60.0f);
			report.Check(easedOnce > 0.0f && easedOnce < quarter && Near(CameraMath::EaseYaw(quarter - 0.01f, quarter, 1.0f / 60.0f), quarter),
				"Camera: the yaw eases toward the wanted step and stops on it");

			float yaw = 0.0f;
			int frames = 0;
			while (!Near(yaw, quarter) && frames < 600)
			{
				yaw = CameraMath::EaseYaw(yaw, quarter, 1.0f / 60.0f);
				++frames;
			}
			report.Check(frames > 5 && frames < 30, "Camera: a quarter turn takes a fraction of a second");

			report.Check(CameraMath::ClampDistance(1.0f) == CAMERA_DISTANCE_MIN && CameraMath::ClampDistance(100.0f) == CAMERA_DISTANCE_MAX && CameraMath::ClampDistance(CAMERA_DISTANCE) == CAMERA_DISTANCE,
				"Camera: the zoom distance is held between its limits");
			report.Check(CameraMath::NextDistanceTarget(14.0f, 0.0f, 1.0f, 0.016f) < 14.0f && CameraMath::NextDistanceTarget(14.0f, 1.0f, 0.0f, 0.016f) > 14.0f
				&& CameraMath::NextDistanceTarget(CAMERA_DISTANCE_MIN, 0.0f, 5.0f, 0.016f) == CAMERA_DISTANCE_MIN,
				"Camera: wheel notches zoom in, a held axis zooms out, and neither passes a limit");

			const glm::vec3 offset = CameraMath::EyeOffset(0.0f, glm::radians(45.0f), 14.0f);
			const glm::vec3 turned = CameraMath::EyeOffset(0.5f * PlayerMath::PI, glm::radians(45.0f), 14.0f);
			report.Check(Near(offset.x, 0.0f) && offset.y > 0.0f && Near(offset.y, offset.z, 1e-3f) && Near(glm::length(offset), 14.0f, 1e-3f) && turned.x > 9.0f && Near(turned.z, 0.0f, 1e-3f),
				"Camera: at yaw 0 the eye sits behind and above the focus on +z, and a quarter turn moves it to +x");
		}
	}

	void RunPlayerChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Player");
		CheckMovement(report);
		CheckWalking(report);
		CheckCamera(report);
	}

}
