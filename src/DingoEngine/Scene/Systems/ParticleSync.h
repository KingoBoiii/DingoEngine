#pragma once

// Engine-internal: drives ParticleEmitterComponents' runtime emitters (RuntimeComponents.h) from the
// scene's update and its 3D pass.

#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Dingo
{

	class ParticleEmitter;
	class Renderer3D;

	namespace Internal::ParticleSync
	{

		// Frees an emitter's runtime with its component.
		void Connect(entt::registry& registry);

		// Adds the scene's (capped) delta to every emitter's pending step.
		void Update(entt::registry& registry, float deltaTime);

		// Submits every emitter on an entity with a Transform3DComponent to the renderer, making its
		// runtime emitter on the first pass (or after its effect or renderer changed) and handing it
		// the bursts queued before then.
		void Submit(entt::registry& registry, Renderer3D& renderer, HierarchySystem::WorldMemo& memo);

		void Emit(entt::registry& registry, entt::entity entity, uint32_t count, const glm::vec3* worldPosition);
		ParticleEmitter* GetEmitter(entt::registry& registry, entt::entity entity);

	}

}
