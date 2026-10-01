#include "depch.h"
#include "DingoEngine/Graphics/Light.h"
#include "DingoEngine/Graphics/LightMath.h"

namespace Dingo
{

	namespace
	{
		// Renderer3D_Lit.glsl's local-light term step for step, without N.L, colour and intensity.
		template<typename LocalLight>
		float Attenuation(const LocalLight& light, const glm::vec3& point)
		{
			if (!Internal::IsUsableLight(light))
				return 0.0f;

			const glm::vec3 toPoint = point - light.Position;
			const float distanceSquared = glm::dot(toPoint, toPoint);
			const float rangeSquared = light.Range * light.Range;
			if (!(distanceSquared < rangeSquared))
				return 0.0f;

			const Internal::LightCone cone = Internal::GetLightCone(light);
			const glm::vec3 direction = toPoint * (1.0f / std::sqrt(std::max(distanceSquared, 1e-8f)));
			const float falloff = 1.0f - distanceSquared / rangeSquared;
			const float coneFactor = glm::clamp(glm::dot(direction, cone.Axis) * cone.Scale + cone.Offset, 0.0f, 1.0f);
			return (falloff * falloff) * (coneFactor * coneFactor);
		}
	}

	float GetLightAttenuation(const PointLight& light, const glm::vec3& point)
	{
		return Attenuation(light, point);
	}

	float GetLightAttenuation(const SpotLight& light, const glm::vec3& point)
	{
		return Attenuation(light, point);
	}

}
