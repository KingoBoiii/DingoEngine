#pragma once

#include <glm/glm.hpp>

namespace Dingo
{

	// Lights for Renderer3D::SubmitLight. Intensity scales Color, and a scene's lights and ambient
	// add up: past 1.0 the frame clips, since there is no tone mapping yet.

	struct DirectionalLight
	{
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f }; // the way the light travels
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
	};

}
