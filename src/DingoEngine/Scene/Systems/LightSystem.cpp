#include "depch.h"
#include "DingoEngine/Scene/Systems/LightSystem.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include "DingoEngine/Graphics/Renderer3D.h"

namespace Dingo::Internal::LightSystem
{

	namespace
	{
		// The default light keeps the engine's original look, ambient + (1 - ambient) * N.L.
		constexpr float k_DefaultLightAmbient = 0.35f;

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

		// ToLight reads a world-space transform; under a parent the component holds a local one.
		Transform3DComponent WorldTransform(entt::entity entity, HierarchySystem::WorldMemo& memo)
		{
			Transform3DComponent world;
			world.Position = memo.Position(entity);
			world.Rotation = memo.Rotation(entity);
			return world;
		}
	}

	namespace
	{
		uint64_t MixKey(uint64_t value)
		{
			value += 0x9e3779b97f4a7c15ull;
			value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
			value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
			return value ^ (value >> 31);
		}

		void IssueShadowProbes(Renderer3D& renderer, ShadowProbeState& probes, const std::unordered_map<entt::entity, ShadowProbeLight>& lights)
		{
			const uint64_t frame = Renderer::GetFrameIndex();
			for (const auto& [localKey, request] : probes.Pending)
			{
				const uint64_t rendererKey = MixKey(probes.Salt ^ localKey);
				const auto light = lights.find(request.Light);
				ShadowProbeState::Answer& answer = probes.Answers[localKey];
				answer.Frame = frame;

				// A light that wasn't drawn casts nothing.
				if (light == lights.end() || !light->second.IsValid())
				{
					answer.Value = 1.0f;
					continue;
				}

				renderer.AddShadowProbe(light->second, request.Point, rendererKey, request.Clearance);
				if (const std::optional<float> result = renderer.GetShadowProbeResult(rendererKey))
					answer.Value = *result;
			}
			probes.Pending.clear();

			if (frame - probes.PruneFrame > 256)
			{
				probes.PruneFrame = frame;
				std::erase_if(probes.Answers, [frame](const auto& entry) { return entry.second.Frame + 600 < frame; });
			}
		}
	}

	namespace
	{
		// A camera's own fog belongs to its view: another camera's view never takes it, and a pass
		// that names no camera only when the scene has no fog on an entity without a camera.
		void SubmitFog(const entt::registry& registry, Renderer3D& renderer, const glm::vec3& clearColor, entt::entity camera)
		{
			const FogComponent* chosen = nullptr;
			if (camera != entt::null && registry.valid(camera))
			{
				const FogComponent* own = registry.try_get<const FogComponent>(camera);
				if (own && own->Enabled && !HierarchySystem::IsInactive(registry, camera))
					chosen = own;
			}

			if (!chosen)
			{
				const FogComponent* cameraFog = nullptr;
				uint32_t sceneWide = 0, onCameras = 0;
				for (entt::entity entity : InEntityOrder<FogComponent>(registry))
				{
					const FogComponent& component = registry.get<const FogComponent>(entity);
					if (!component.Enabled || HierarchySystem::IsInactive(registry, entity))
						continue;

					if (registry.all_of<CameraComponent>(entity))
					{
						if (onCameras++ == 0)
							cameraFog = &component;
					}
					else if (sceneWide++ == 0)
						chosen = &component;
				}

				const bool noCamera = camera == entt::null;
				static bool s_Warned = false;
				if (!s_Warned && (sceneWide > 1 || (noCamera && sceneWide == 0 && onCameras > 1)))
				{
					DE_CORE_WARN("A scene has more than one enabled FogComponent that could fog this view; only the first one in entity order is used. A FogComponent on the rendered camera's entity fogs that view alone.");
					s_Warned = true;
				}

				if (!chosen && noCamera)
					chosen = cameraFog;
			}

			if (!chosen)
				return;

			Fog fog = chosen->Settings;
			if (chosen->UseClearColor)
				fog.Color = clearColor;
			renderer.SetFog(fog);
		}
	}

	void SubmitLights(const entt::registry& registry, Renderer3D& renderer, HierarchySystem::WorldMemo& memo, const glm::vec3& clearColor, ShadowProbeState* probes, entt::entity camera)
	{
		memo.Begin(registry);
		bool hasLight = false;
		glm::vec3 ambient(0.0f);

		// The lights probes ask about, and what Renderer3D made of each.
		std::unordered_map<entt::entity, ShadowProbeLight> probedLights;
		if (probes)
		{
			for (const auto& [key, request] : probes->Pending)
				probedLights.emplace(request.Light, ShadowProbeLight());
		}
		auto noteLight = [&](entt::entity entity)
		{
			if (const auto it = probedLights.find(entity); it != probedLights.end())
				it->second = renderer.GetLastSubmittedLight();
		};

		// A legacy Ambient keeps the engine's original formula, ambient + (1 - ambient) * N.L, so a
		// scene tuned before Intensity existed looks the same. Only lights Renderer3D accepted add
		// their ambient, and a non-finite one is skipped: one NaN term would make the summed ambient
		// NaN, which SetAmbientLight rejects, losing every other source with it.
		DE_SUPPRESS_DEPRECATED_BEGIN
		auto submitDirectional = [&](const DirectionalLightComponent& light, float legacyAmbient)
		{
			const bool accepted = renderer.SubmitLight(DirectionalLight{ light.Direction, light.Color, light.Intensity * std::max(1.0f - legacyAmbient, 0.0f), light.CastShadows, light.ShadowStrength, light.ShadowCasterGroups });
			if (accepted && std::isfinite(legacyAmbient))
				ambient += glm::vec3(legacyAmbient);
		};
		DE_SUPPRESS_DEPRECATED_END

		// An inactive light still counts as the scene's choice of lighting, as a disabled one does.
		for (entt::entity entity : InEntityOrder<DirectionalLightComponent>(registry))
		{
			hasLight = true;
			if (HierarchySystem::IsInactive(registry, entity))
				continue;
			const DirectionalLightComponent& light = registry.get<const DirectionalLightComponent>(entity);
			DE_SUPPRESS_DEPRECATED_BEGIN
			submitDirectional(light, light.Ambient);
			DE_SUPPRESS_DEPRECATED_END
			noteLight(entity);
		}

		for (entt::entity entity : InEntityOrder<AmbientLightComponent>(registry))
		{
			hasLight = true;
			if (HierarchySystem::IsInactive(registry, entity))
				continue;
			const AmbientLightComponent& light = registry.get<const AmbientLightComponent>(entity);
			const glm::vec3 contribution = light.Color * light.Intensity;
			if (std::isfinite(contribution.x) && std::isfinite(contribution.y) && std::isfinite(contribution.z))
				ambient += contribution;
		}

		for (entt::entity entity : InEntityOrder<PointLightComponent>(registry))
		{
			const PointLightComponent& light = registry.get<const PointLightComponent>(entity);
			const Transform3DComponent* transform = PlaceLight(registry, entity);
			if (!transform)
				continue;

			hasLight = true;
			if (light.Enabled && !HierarchySystem::IsInactive(registry, entity))
			{
				renderer.SubmitLight(light.ToLight(WorldTransform(entity, memo)));
				noteLight(entity);
			}
		}

		for (entt::entity entity : InEntityOrder<SpotLightComponent>(registry))
		{
			const SpotLightComponent& light = registry.get<const SpotLightComponent>(entity);
			const Transform3DComponent* transform = PlaceLight(registry, entity);
			if (!transform)
				continue;

			hasLight = true;
			if (light.Enabled && !HierarchySystem::IsInactive(registry, entity))
			{
				renderer.SubmitLight(light.ToLight(WorldTransform(entity, memo)));
				noteLight(entity);
			}
		}

		if (probes && !probes->Pending.empty())
			IssueShadowProbes(renderer, *probes, probedLights);

		if (!hasLight)
			submitDirectional(DirectionalLightComponent{}, k_DefaultLightAmbient);

		// Always set, even to black: it tells Renderer3D the scene chose its lighting, so a scene
		// whose lights are all switched off goes dark instead of falling back to the default light.
		renderer.SetAmbientLight(ambient, 1.0f);

		SubmitFog(registry, renderer, clearColor, camera);
	}

}
