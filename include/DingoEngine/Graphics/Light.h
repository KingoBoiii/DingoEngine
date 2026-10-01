#pragma once

#include <glm/glm.hpp>

namespace Dingo
{

	// Lights for Renderer3D::SubmitLight. Intensity scales Color, and a scene's lights and ambient
	// add up: past 1.0 the frame clips, since there is no tone mapping yet.
	//
	// Point and spot lights are local lights: their brightness falls off smoothly from Intensity
	// at the light to exactly zero at Range, as (1 - (d / Range)^2)^2, and they share one
	// per-scene budget (Renderer3DCapabilities::MaxLocalLights).

	struct DirectionalLight
	{
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f }; // the way the light travels
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
	};

	struct PointLight
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
	};

	struct SpotLight
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Direction{ 0.0f, -1.0f, 0.0f }; // the way the cone points
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		// Angles from the cone's axis, in degrees: full strength inside InnerConeAngle, fading
		// to zero at OuterConeAngle (1 to 179).
		float InnerConeAngle = 20.0f;
		float OuterConeAngle = 30.0f;
	};

}
