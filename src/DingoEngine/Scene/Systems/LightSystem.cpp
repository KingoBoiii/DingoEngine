#include "depch.h"
#include "DingoEngine/Scene/Systems/LightSystem.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include "DingoEngine/Graphics/Renderer3D.h"

namespace Dingo::Internal::LightSystem
{

	void SubmitLights(const entt::registry& registry, Renderer3D& renderer)
	{
		bool hasLight = false;
		glm::vec3 ambient(0.0f);
		uint32_t directionalCount = 0;

		// A legacy Ambient keeps the engine's original formula, ambient + (1 - ambient) * N.L, so a
		// scene tuned before Intensity existed looks the same. Only lights Renderer3D can use add
		// their ambient.
		auto submitDirectional = [&](const DirectionalLightComponent& light)
		{
			renderer.SubmitLight(DirectionalLight{ light.Direction, light.Color, light.Intensity * std::max(1.0f - light.Ambient, 0.0f) });
			if (directionalCount++ < Renderer3D::k_MaxDirectionalLights)
				ambient += glm::vec3(light.Ambient);
		};

		for (auto [entity, light] : registry.view<const DirectionalLightComponent>().each())
		{
			hasLight = true;
			submitDirectional(light);
		}

		for (auto [entity, light] : registry.view<const AmbientLightComponent>().each())
		{
			hasLight = true;
			ambient += light.Color * light.Intensity;
		}

		for (auto [entity, light] : registry.view<const PointLightComponent>().each())
		{
			hasLight = true;
			if (light.Enabled && registry.all_of<Transform3DComponent>(entity))
				renderer.SubmitLight(PointLight{ HierarchySystem::WorldPosition(registry, entity), light.Color, light.Intensity, light.Range });
		}

		for (auto [entity, light] : registry.view<const SpotLightComponent>().each())
		{
			hasLight = true;
			if (!light.Enabled || !registry.all_of<Transform3DComponent>(entity))
				continue;

			SpotLight spot;
			spot.Position = HierarchySystem::WorldPosition(registry, entity);
			spot.Direction = HierarchySystem::WorldRotation(registry, entity) * light.Direction;
			spot.Color = light.Color;
			spot.Intensity = light.Intensity;
			spot.Range = light.Range;
			spot.InnerConeAngle = light.InnerConeAngle;
			spot.OuterConeAngle = light.OuterConeAngle;
			renderer.SubmitLight(spot);
		}

		if (!hasLight)
			submitDirectional(DirectionalLightComponent{});

		// Always set, even to black: it tells Renderer3D the scene chose its lighting, so a scene
		// whose lights are all switched off goes dark instead of falling back to the default light.
		renderer.SetAmbientLight(ambient, 1.0f);
	}

}
