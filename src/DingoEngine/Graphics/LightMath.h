#pragma once

#include "DingoEngine/Graphics/Light.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

// Renderer3D::SubmitLight and GetLightAttenuation both set lights up through these, so the cone a
// game tests is the cone Renderer3D_Lit.glsl draws.
namespace Dingo::Internal
{

	inline bool IsFinite(const glm::vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	// One NaN light would turn every lit pixel's sum to NaN, so the comparisons are written to
	// reject NaN too.
	inline bool IsUsableLight(const PointLight& light)
	{
		return light.Intensity > 0.0f && light.Range > 0.0f && IsFinite(light.Position) && IsFinite(light.Color * light.Intensity);
	}

	inline bool IsUsableLight(const SpotLight& light)
	{
		return light.Intensity > 0.0f && light.Range > 0.0f && IsFinite(light.Position) && IsFinite(light.Color * light.Intensity) &&
			IsFinite(light.Direction) && std::isfinite(light.InnerConeAngle) && std::isfinite(light.OuterConeAngle);
	}

	// The shader's cone factor is saturate(dot(axis, direction from the light) * Scale + Offset).
	// A point light is a spot light whose factor is always 1.
	struct LightCone
	{
		glm::vec3 Axis{ 0.0f };
		float Scale = 0.0f;
		float Offset = 1.0f;
	};

	inline LightCone GetLightCone(const PointLight&)
	{
		return {};
	}

	// 1 at the inner angle, 0 at the outer one. Below about 1 degree the cosines are too close in
	// float to reach 1.
	inline LightCone GetLightCone(const SpotLight& light)
	{
		const float outerAngle = glm::clamp(light.OuterConeAngle, 1.0f, 179.0f);
		const float innerAngle = glm::clamp(light.InnerConeAngle, 0.0f, outerAngle);
		const float cosOuter = std::cos(glm::radians(outerAngle));
		const float scale = 1.0f / std::max(std::cos(glm::radians(innerAngle)) - cosOuter, 1e-4f);

		const float length = glm::length(light.Direction);
		const glm::vec3 axis = length > 0.0f ? light.Direction / length : glm::vec3(0.0f, -1.0f, 0.0f);
		return { axis, scale, -cosOuter * scale };
	}

}
