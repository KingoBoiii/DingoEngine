#pragma once

// Engine-internal: drives ParticleEmitterComponents' runtime emitters (RuntimeComponents.h) from the
// scene's update and its 3D pass.

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <unordered_map>
#include <utility>
#include <vector>

namespace Dingo
{

	class ParticleEmitter;
	class Renderer3D;

	namespace Internal::ParticleSync
	{

		// Frees an emitter's runtime with its component, and stops the range emitters of a
		// ParticleEventComponent removed with its entity or alone. entityMap is the scene's UUID map,
		// which outlives the registry's hooks.
		void Connect(entt::registry& registry, const std::unordered_map<UUID, entt::entity>& entityMap);

		// Fires the bursts and starts and stops the range emitters ParticleEventComponents bind to the
		// frame's animation events.
		void ApplyAnimationEvents(entt::registry& registry, const std::unordered_map<UUID, entt::entity>& entityMap, const std::vector<std::pair<entt::entity, AnimationEvent>>& events);

		// Adds the scene's (capped) delta to every emitter's pending step.
		void Update(entt::registry& registry, float deltaTime);

		// Submits every emitter on an entity with a Transform3DComponent to the renderer, through the
		// runtime emitter this renderer made (made on its first pass, or after the effect changed), and
		// hands it the bursts it hasn't had. An emitter no pass submitted for 300 frames is released.
		void Submit(entt::registry& registry, Renderer3D& renderer, HierarchySystem::WorldMemo& memo);

		void Emit(entt::registry& registry, entt::entity entity, uint32_t count, const glm::vec3* worldPosition);
		ParticleEmitter* GetEmitter(entt::registry& registry, entt::entity entity);

	}

}
