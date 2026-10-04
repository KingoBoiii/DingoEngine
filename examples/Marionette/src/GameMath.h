#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Dingo::GameMath
{

	inline float WrapAngle(float angle)
	{
		constexpr float pi = std::numbers::pi_v<float>;
		angle = std::fmod(angle, 2.0f * pi);
		if (angle > pi)
			angle -= 2.0f * pi;
		else if (angle < -pi)
			angle += 2.0f * pi;
		return angle;
	}

	inline float ApproachAngle(float current, float target, float maxStep)
	{
		return WrapAngle(current + std::clamp(WrapAngle(target - current), -maxStep, maxStep));
	}

	// Yaw of a ground direction (x, z): 0 along +Z, positive towards +X.
	inline float YawOf(const glm::vec2& direction)
	{
		return std::atan2(direction.x, direction.y);
	}

	inline glm::quat YawQuat(float yaw)
	{
		return glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	inline glm::vec2 MoveToward(const glm::vec2& current, const glm::vec2& target, float maxStep)
	{
		const glm::vec2 delta = target - current;
		const float distance = glm::length(delta);
		return distance <= maxStep || distance <= 0.0f ? target : current + delta * (maxStep / distance);
	}

}
