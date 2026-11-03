#include "Look/CameraMath.h"
#include "GameTuning.h"
#include "Life/PlayerMath.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Dingo::CameraMath
{

	float YawForSteps(int steps)
	{
		return PlayerMath::WrapAngle(glm::radians(CAMERA_YAW_STEP_DEG * static_cast<float>(steps)));
	}

	float EaseYaw(float current, float target, float dt)
	{
		return PlayerMath::ApproachAngle(current, target, glm::radians(CAMERA_YAW_SPEED_DEG) * dt);
	}

	float ClampDistance(float distance)
	{
		return std::clamp(distance, CAMERA_DISTANCE_MIN, CAMERA_DISTANCE_MAX);
	}

	float NextDistanceTarget(float target, float axis, float wheel, float dt)
	{
		return ClampDistance(target + axis * CAMERA_ZOOM_SPEED * dt - wheel * CAMERA_ZOOM_WHEEL_STEP);
	}

	float EaseDistance(float current, float target, float dt)
	{
		return glm::mix(current, target, std::min(1.0f, CAMERA_LAG * dt));
	}

	glm::vec3 EyeOffset(float yaw, float pitch, float distance)
	{
		return glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)) * distance;
	}

}
