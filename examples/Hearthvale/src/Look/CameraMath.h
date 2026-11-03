#pragma once
#include <glm/glm.hpp>

namespace Dingo::CameraMath
{

	// The yaw (radians) of the camera after `steps` quarter turns; each step is CAMERA_YAW_STEP_DEG.
	float YawForSteps(int steps);

	// Moves `current` toward `target` by at most CAMERA_YAW_SPEED_DEG per second, the short way round.
	float EaseYaw(float current, float target, float dt);

	float ClampDistance(float distance);

	// A held axis (positive = out) and a wheel notch count (positive = in) move the wanted distance.
	float NextDistanceTarget(float target, float axis, float wheel, float dt);

	// Closes the gap to `target` exponentially; the same frame-rate-independent form as the focus lag.
	float EaseDistance(float current, float target, float dt);

	// The eye relative to the focus: at yaw 0 the camera sits at +z looking toward -z.
	glm::vec3 EyeOffset(float yaw, float pitch, float distance);

}
