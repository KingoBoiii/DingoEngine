#include "depch.h"
#include "DingoEngine/Scene/Systems/ScriptSystem.h"

#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

namespace Dingo
{

	namespace Internal
	{

		ScriptSystem::ScriptSystem(const entt::registry& registry)
			: m_Registry(registry)
		{
		}

		ScriptSystem::~ScriptSystem() = default;

		void ScriptSystem::Attach(entt::entity handle, ScriptableEntity* instance, Entity owner)
		{
			if (m_Scripts.contains(handle))
			{
				DE_CORE_WARN("AddScript: '{}' already has a script, which is replaced (an entity holds one)", owner.GetName());
				Detach(handle);
			}

			instance->m_Entity = owner;
			m_Scripts[handle].reset(instance);
			m_HasUnstarted = true;
			instance->OnCreate();
		}

		ScriptableEntity* ScriptSystem::Find(entt::entity handle) const
		{
			auto it = m_Scripts.find(handle);
			return it != m_Scripts.end() ? it->second.get() : nullptr;
		}

		void ScriptSystem::Detach(entt::entity handle)
		{
			auto it = m_Scripts.find(handle);
			if (it == m_Scripts.end())
				return;

			m_Detached.push_back(std::move(it->second));
			m_Scripts.erase(it);

			m_Detached.back()->OnDestroy();
		}

		void ScriptSystem::StartPending()
		{
			// Called every frame from Update, but only ever has work right after a script is
			// attached. Without this the walk (and its allocation) ran on every steady-state
			// frame just to find nothing.
			if (!m_HasUnstarted)
				return;

			// Cleared before the pass, not after: an OnStart that attaches more scripts sets
			// it again, and those run on a later pass since this snapshot cannot see them.
			m_HasUnstarted = false;

			m_StartBuffer.clear();
			for (auto& [handle, script] : m_Scripts)
				if (!script->m_Started)
					m_StartBuffer.push_back(handle);

			for (entt::entity handle : m_StartBuffer)
			{
				auto it = m_Scripts.find(handle);
				if (it == m_Scripts.end() || it->second->m_Started)
					continue;

				// Waits for its entity to become active, which marks it pending again.
				if (!IsActive(handle))
					continue;

				it->second->m_Started = true; // set first so a re-entrant spawn can't double-fire
				it->second->OnStart();
			}
		}

		bool ScriptSystem::IsActive(entt::entity handle) const
		{
			return m_Registry.valid(handle) && !HierarchySystem::IsInactive(m_Registry, handle);
		}

		void ScriptSystem::FixedUpdate(float step)
		{
			m_UpdateBuffer.clear();
			for (auto& [handle, script] : m_Scripts)
				m_UpdateBuffer.push_back(handle);

			for (entt::entity handle : m_UpdateBuffer)
			{
				auto it = m_Scripts.find(handle);
				if (it != m_Scripts.end() && it->second->m_Started && IsActive(handle))
					it->second->OnFixedUpdate(step);
			}
		}

		void ScriptSystem::Update(float deltaTime)
		{
			// Scripts attached since the scene started (e.g. spawned at runtime) get OnStart
			// before their first OnUpdate.
			StartPending();

			// Snapshot the current scripts so spawning new entities mid-update doesn't
			// invalidate iteration (new scripts run next frame).
			m_UpdateBuffer.clear();
			for (auto& [handle, script] : m_Scripts)
				m_UpdateBuffer.push_back(handle);

			for (entt::entity handle : m_UpdateBuffer)
			{
				// A script whose entity another one activated during this pass starts next frame.
				auto it = m_Scripts.find(handle);
				if (it != m_Scripts.end() && it->second->m_Started && IsActive(handle))
					it->second->OnUpdate(deltaTime);
			}
		}

		void ScriptSystem::DeliverAnimationEvent(entt::entity handle, const AnimationEvent& event)
		{
			auto it = m_Scripts.find(handle);
			if (it != m_Scripts.end() && it->second->m_Started)
				it->second->OnAnimationEvent(event);
		}

		bool ScriptSystem::IsStarted(entt::entity handle) const
		{
			auto it = m_Scripts.find(handle);
			return it != m_Scripts.end() && it->second->m_Started;
		}

		void ScriptSystem::ForEach(const std::function<void(ScriptableEntity*)>& fn) const
		{
			// Local snapshot, not a member buffer: fn is client code and may spawn or
			// destroy entities (either rehashes the map), and this is reachable from inside
			// Update, whose own snapshot must survive the call.
			std::vector<entt::entity> handles;
			handles.reserve(m_Scripts.size());
			for (const auto& [handle, script] : m_Scripts)
				handles.push_back(handle);

			for (entt::entity handle : handles)
			{
				auto it = m_Scripts.find(handle);
				if (it != m_Scripts.end())
					fn(it->second.get());
			}
		}

		void ScriptSystem::FreeDetached()
		{
			m_Detached.clear();
		}

		void ScriptSystem::DetachAll()
		{
			// Snapshot the handles: OnDestroy may call DestroyEntity, which erases from the
			// map being walked.
			std::vector<entt::entity> handles;
			handles.reserve(m_Scripts.size());
			for (auto& [handle, script] : m_Scripts)
				handles.push_back(handle);

			for (entt::entity handle : handles)
				Detach(handle);

			m_Scripts.clear();
			m_HasUnstarted = false;
		}

	}

}
