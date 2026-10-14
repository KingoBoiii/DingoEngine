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
	struct AnimationEvent;

	namespace Internal
	{

		class ScriptSystem
		{
		public:
			explicit ScriptSystem(const entt::registry& registry);
			~ScriptSystem();

			// Binds the instance to its entity, registers it, and fires OnCreate. Takes
			// ownership. An existing script is detached first (OnDestroy).
			void Attach(entt::entity handle, ScriptableEntity* instance, Entity owner);

			ScriptableEntity* Find(entt::entity handle) const;

			// Unregisters the entity's script and *then* fires its OnDestroy. Detaching
			// first is what makes a DestroyEntity() call from inside OnDestroy safe: the
			// script is no longer reachable, so it can neither fire twice nor be erased
			// from under an in-flight iteration. No-op when the entity has no script.
			void Detach(entt::entity handle);

			// Fires OnStart on every script of an active entity that has not started yet.
			void StartPending();

			// The scripts of active entities that have started; Update starts the pending ones first.
			void FixedUpdate(float step);
			void Update(float deltaTime);

			// An entity became active: its script may still be waiting for OnStart.
			void MarkPending() { m_HasUnstarted = true; }

			// To the entity's script, if it has one that has started.
			void DeliverAnimationEvent(entt::entity handle, const AnimationEvent& event);
			// The entity has a script and its OnStart has run.
			bool IsStarted(entt::entity handle) const;

			void ForEach(const std::function<void(ScriptableEntity*)>& fn) const;

			// Detaches every script (each gets its OnDestroy) and empties the system.
			void DetachAll();

			// Frees the detached scripts unless a script callback is on the stack: a script may
			// detach itself (RemoveScript, AddScript, destroying its own entity).
			void FreeDetached();

		private:
			bool IsActive(entt::entity handle) const;

			const entt::registry& m_Registry;
			std::unordered_map<entt::entity, std::unique_ptr<ScriptableEntity>> m_Scripts;
			std::vector<std::unique_ptr<ScriptableEntity>> m_Detached;
			mutable uint32_t m_CallDepth = 0;

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
