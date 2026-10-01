#pragma once

#include <algorithm>
#include <cmath>

namespace Dingo::GameMath
{

	inline constexpr float PI = 3.14159265f;

	inline float WrapAngle(float angle)
	{
		angle = std::fmod(angle, 2.0f * PI);
		if (angle > PI)
			angle -= 2.0f * PI;
		else if (angle < -PI)
			angle += 2.0f * PI;
		return angle;
	}

	inline float ApproachAngle(float current, float target, float maxStep)
	{
		return WrapAngle(current + std::clamp(WrapAngle(target - current), -maxStep, maxStep));
	}

}
