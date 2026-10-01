#include "depch.h"
#include "DingoEngine/Scene/Systems/LightSystem.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer3D.h"

namespace Dingo::Internal::LightSystem
{

	namespace
	{
		// A light that can't be placed is skipped without switching the default light off: counting
		// it would leave a scene whose only light lacks a transform lit by nothing at all.
		const Transform3DComponent* PlaceLight(const entt::registry& registry, entt::entity entity)
		{
			const Transform3DComponent* transform = registry.try_get<const Transform3DComponent>(entity);
			static bool s_Warned = false;
			if (!transform && !s_Warned)
			{
				DE_CORE_WARN("A point or spot light component sits on an entity without a Transform3DComponent, so it can't be placed and is ignored. Add a Transform3DComponent to light the scene from it.");
				s_Warned = true;
			}
			return transform;
		}

		// EnTT views run newest first and reshuffle on removal. Entity order is roughly creation order
		// and survives another light's removal, so a scene's sun outranks the lights added after it.
		template<typename Component>
		const std::vector<entt::entity>& InEntityOrder(const entt::registry& registry)
		{
			static std::vector<entt::entity> s_Entities;
			s_Entities.clear();
			for (entt::entity entity : registry.view<const Component>())
				s_Entities.push_back(entity);
			std::sort(s_Entities.begin(), s_Entities.end(), [](entt::entity a, entt::entity b) { return entt::to_entity(a) < entt::to_entity(b); });
			return s_Entities;
		}
	}

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

		for (entt::entity entity : InEntityOrder<DirectionalLightComponent>(registry))
		{
			hasLight = true;
			submitDirectional(registry.get<const DirectionalLightComponent>(entity));
		}

		for (entt::entity entity : InEntityOrder<AmbientLightComponent>(registry))
		{
			hasLight = true;
			const AmbientLightComponent& light = registry.get<const AmbientLightComponent>(entity);
			ambient += light.Color * light.Intensity;
		}

		for (entt::entity entity : InEntityOrder<PointLightComponent>(registry))
		{
			const PointLightComponent& light = registry.get<const PointLightComponent>(entity);
			const Transform3DComponent* transform = PlaceLight(registry, entity);
			if (!transform)
				continue;

			hasLight = true;
			if (light.Enabled)
				renderer.SubmitLight(PointLight{ transform->Position, light.Color, light.Intensity, light.Range });
		}

		for (entt::entity entity : InEntityOrder<SpotLightComponent>(registry))
		{
			const SpotLightComponent& light = registry.get<const SpotLightComponent>(entity);
			const Transform3DComponent* transform = PlaceLight(registry, entity);
			if (!transform)
				continue;

			hasLight = true;
			if (!light.Enabled)
				continue;

			SpotLight spot;
			spot.Position = transform->Position;
			spot.Direction = transform->Rotation * light.Direction;
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
