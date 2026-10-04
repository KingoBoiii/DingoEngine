#include "DriveBrain.h"
#include "Fighter.h"
#include "GameMath.h"
#include "GameTuning.h"

#include <algorithm>
#include <cmath>

namespace
{
	using namespace Dingo;

	glm::vec2 Orbit(const glm::vec2& position, const glm::vec2& center, float radius, float sign)
	{
		glm::vec2 radial = position - center;
		const float distance = glm::length(radial);
		if (distance < 1.0e-3f)
			return glm::vec2(1.0f, 0.0f);

		radial /= distance;
		const glm::vec2 tangent(radial.y * sign, -radial.x * sign);
		return glm::normalize(tangent + radial * ((radius - distance) * DRIVE_ORBIT_GAIN));
	}

	glm::vec2 Toward(const glm::vec2& from, const glm::vec2& to)
	{
		const glm::vec2 delta = to - from;
		return glm::length(delta) > 1.0e-3f ? glm::normalize(delta) : glm::vec2(0.0f);
	}
}

namespace Dingo
{

	FighterIntent DriveBrain::Think(float deltaTime, const Fighter& self, const Fighter& opponent)
	{
		m_Time += deltaTime;
		const glm::vec2 position = self.GetGroundPosition();
		const glm::vec2 other = opponent.GetGroundPosition();

		FighterIntent intent;
		switch (m_Mode)
		{
			case DriveMode::Ramp:
				intent.Move = glm::vec2(1.0f, 0.0f) * std::min(m_Time / DRIVE_RAMP_SECONDS, 1.0f);
				intent.FaceOpponent = false;
				break;

			case DriveMode::Circle:
				intent.Move = Orbit(position, other, DRIVE_CIRCLE_RADIUS, 1.0f);
				intent.FaceOpponent = false;
				break;

			case DriveMode::Strafe:
			{
				const float t = std::fmod(m_Time, DRIVE_STRAFE_LOOP);
				if (t < DRIVE_STRAFE_HOLD)
					break;
				if (t < DRIVE_STRAFE_ORBIT_A)
					intent.Move = Orbit(position, other, DRIVE_ORBIT_RADIUS, 1.0f);
				else if (t < DRIVE_STRAFE_ORBIT_B)
					intent.Move = Orbit(position, other, DRIVE_ORBIT_RADIUS, -1.0f);
				else if (t < DRIVE_STRAFE_RETREAT)
					intent.Move = -Toward(position, other);
				else
					intent.Move = Toward(position, other) * WALK_INTENT;
				break;
			}

			case DriveMode::Wall:
				intent.Move = glm::vec2(1.0f, 0.0f);
				intent.FaceOpponent = false;
				break;

			default:
				break;
		}
		return intent;
	}

	BoutLayout GetBoutLayout(DriveMode mode)
	{
		BoutLayout layout;
		layout.PlayerPosition = glm::vec3(-0.5f * BOUT_DISTANCE, FIGHTER_SPAWN_LIFT, 0.0f);
		layout.PlayerYawDegrees = DRIVE_FACE_RIGHT_DEG;
		layout.OpponentPosition = glm::vec3(0.5f * BOUT_DISTANCE, FIGHTER_SPAWN_LIFT, 0.0f);
		layout.OpponentYawDegrees = DRIVE_FACE_LEFT_DEG;

		switch (mode)
		{
			case DriveMode::Ramp:
			case DriveMode::Wall:
				layout.PlayerPosition = glm::vec3(mode == DriveMode::Wall ? DRIVE_WALL_START_X : DRIVE_LANE_START, FIGHTER_SPAWN_LIFT, 0.0f);
				layout.OpponentPosition = glm::vec3(0.0f, FIGHTER_SPAWN_LIFT, DRIVE_LANE_OFFSET);
				layout.OpponentYawDegrees = 180.0f;
				break;

			case DriveMode::Circle:
				layout.PlayerPosition = glm::vec3(DRIVE_CIRCLE_RADIUS, FIGHTER_SPAWN_LIFT, 0.0f);
				layout.PlayerYawDegrees = 180.0f;
				layout.OpponentPosition = glm::vec3(0.0f, FIGHTER_SPAWN_LIFT, 0.0f);
				break;

			default:
				break;
		}
		return layout;
	}

}
