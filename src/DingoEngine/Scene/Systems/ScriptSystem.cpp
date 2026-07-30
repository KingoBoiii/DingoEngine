#include "depch.h"
#include "DingoEngine/Scene/Systems/ScriptSystem.h"

#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"

namespace Dingo
{

	namespace Internal
	{

		void ScriptSystem::Attach(entt::entity handle, ScriptableEntity* instance, Entity owner)
		{
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

			std::unique_ptr<ScriptableEntity> script = std::move(it->second);
			m_Scripts.erase(it);

			script->OnDestroy();
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

				it->second->m_Started = true; // set first so a re-entrant spawn can't double-fire
				it->second->OnStart();
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
				auto it = m_Scripts.find(handle);
				if (it != m_Scripts.end())
					it->second->OnUpdate(deltaTime);
			}
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
