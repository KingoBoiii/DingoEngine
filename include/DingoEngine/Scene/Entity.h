#pragma once

#include "DingoEngine/Assertion.h"
#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Components.h"

#include <entt/entt.hpp>

#include <utility>

namespace Dingo
{

	// A lightweight, copyable handle to an entity inside a Scene. It stores only an
	// EnTT entity id plus a back-pointer to the owning scene; all component access
	// is forwarded to the scene's registry.
	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_EntityHandle(handle), m_Scene(scene) {}
		Entity(const Entity&) = default;

		// Note: returns decltype(auto) rather than T& because EnTT's empty-type
		// optimization makes emplace<T>() return void for stateless tag components
		// (e.g. PlayerTag); for components with data it still returns T&.
		template<typename T, typename... Args>
		decltype(auto) AddComponent(Args&&... args)
		{
			DE_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
			return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T, typename... Args>
		decltype(auto) AddOrReplaceComponent(Args&&... args)
		{
			return m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent()
		{
			DE_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		bool HasComponent() const
		{
			return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent()
		{
			DE_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		UUID GetUUID() { return GetComponent<IDComponent>().ID; }
		const std::string& GetName() { return GetComponent<TagComponent>().Tag; }

		operator bool() const { return m_EntityHandle != entt::null && m_Scene != nullptr; }
		operator entt::entity() const { return m_EntityHandle; }
		operator uint32_t() const { return static_cast<uint32_t>(m_EntityHandle); }

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}
		bool operator!=(const Entity& other) const { return !(*this == other); }

	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr;

		friend class Scene;
	};

}
