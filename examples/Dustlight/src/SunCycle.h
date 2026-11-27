#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace Dingo
{

	struct SunState
	{
		glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float ElevationDegrees = 0.0f;
		float AzimuthDegrees = 0.0f;
	};

	namespace SunCycle
	{
		constexpr float k_DawnHours = 7.0f;
		constexpr float k_DuskHours = 18.0f;
		constexpr float k_MinElevation = 8.0f;
		constexpr float k_MaxElevation = 60.0f;
		constexpr float k_AzimuthSpan = 20.0f;

		inline SunState Evaluate(float hours)
		{
			constexpr float k_Pi = 3.14159265358979f;
			const float u = std::clamp((hours - k_DawnHours) / (k_DuskHours - k_DawnHours), 0.0f, 1.0f);
			const float height = std::sin(k_Pi * u);

			SunState sun;
			sun.ElevationDegrees = k_MinElevation + (k_MaxElevation - k_MinElevation) * height;
			sun.AzimuthDegrees = -k_AzimuthSpan + 2.0f * k_AzimuthSpan * u;

			const float elevation = glm::radians(sun.ElevationDegrees);
			const float azimuth = glm::radians(sun.AzimuthDegrees);
			sun.Direction = glm::normalize(glm::vec3(std::sin(azimuth) * std::cos(elevation), -std::sin(elevation), std::cos(azimuth) * std::cos(elevation)));

			const float warmth = 1.0f - std::clamp((sun.ElevationDegrees - k_MinElevation) / 30.0f, 0.0f, 1.0f);
			sun.Color = glm::mix(glm::vec3(1.0f, 0.94f, 0.84f), glm::vec3(1.0f, 0.56f, 0.26f), warmth);
			sun.Intensity = glm::mix(5.0f, 3.2f, warmth);
			return sun;
		}
	}

}
