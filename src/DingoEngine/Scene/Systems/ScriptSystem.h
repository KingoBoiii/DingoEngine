#pragma once

// Engine-internal: owns the ScriptableEntity instances attached to a scene's
// entities and drives their lifecycle hooks. Lives under src/ so EnTT stays a
// private implementation detail.

#include <entt/entt.hpp>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class Entity;
	class ScriptableEntity;

	namespace Internal
	{

		class ScriptSystem
		{
		public:
			// Binds the instance to its entity, registers it, and fires OnCreate. Takes
			// ownership. Replacing an existing script destroys the old instance.
			void Attach(entt::entity handle, ScriptableEntity* instance, Entity owner);

			ScriptableEntity* Find(entt::entity handle) const;

			// Unregisters the entity's script and *then* fires its OnDestroy. Detaching
			// first is what makes a DestroyEntity() call from inside OnDestroy safe: the
			// script is no longer reachable, so it can neither fire twice nor be erased
			// from under an in-flight iteration. No-op when the entity has no script.
			void Detach(entt::entity handle);

			// Fires OnStart on every script that has not started yet.
			void StartPending();

			void Update(float deltaTime);

			void ForEach(const std::function<void(ScriptableEntity*)>& fn) const;

			// Detaches every script (each gets its OnDestroy) and empties the system.
			void DetachAll();

		private:
			std::unordered_map<entt::entity, std::unique_ptr<ScriptableEntity>> m_Scripts;

			// Set by Attach (the only thing that adds to m_Scripts) and cleared by
			// StartPending, so the per-frame OnStart pass can skip the walk entirely once
			// everything has started.
			bool m_HasUnstarted = false;

			// Reused handle snapshots. Separate buffers because StartPending runs inside
			// Update. ForEach deliberately uses a local vector instead: it is reachable
			// from a script's OnUpdate, so sharing a member buffer would clobber the
			// snapshot Update is iterating.
			std::vector<entt::entity> m_StartBuffer;
			std::vector<entt::entity> m_UpdateBuffer;
		};

	}

}
