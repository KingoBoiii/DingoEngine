#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Scene/Components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Dingo
{

	class Scene;
	class ScriptableEntity;

	// A lightweight, copyable handle to an entity inside a Scene. The underlying
	// ECS (EnTT) is fully hidden: the handle is an opaque integer and all access
	// is routed through the engine. Client code never sees or links EnTT.
	class Entity
	{
	public:
		Entity() = default;

		// --- Components (built-in component types only) ---

		template<typename T>
		T& AddComponent(const T& component = T{});

		template<typename T>
		T& GetComponent();

		template<typename T>
		bool HasComponent() const;

		template<typename T>
		void RemoveComponent();

		// --- Behaviours ---

		// Attach a script (must derive from ScriptableEntity). The scene owns the
		// instance and drives its OnCreate/OnUpdate/OnDestroy.
		template<typename T, typename... Args>
		T& AddScript(Args&&... args)
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "T must derive from ScriptableEntity");
			T* instance = new T(std::forward<Args>(args)...);
			AttachScript(static_cast<ScriptableEntity*>(instance));
			return *instance;
		}

		template<typename T>
		T* GetScript()
		{
			return dynamic_cast<T*>(GetScriptInstance());
		}

		template<typename T>
		bool HasScript()
		{
			return GetScript<T>() != nullptr;
		}

		// --- Hierarchy ---
		//
		// A child's Transform3DComponent is local to its parent: world = parentWorld x local. A root's
		// local is its world, so nothing changes for an entity that never gets a parent. Destroying or
		// duplicating an entity takes its whole subtree with it.

		// Makes this entity the last child of `parent`, or changes nothing if `parent` already is its
		// parent; a null Entity detaches it. With keepWorldTransform the local transform is rewritten
		// so the entity stays where it is in the world (an entity without a Transform3DComponent has
		// none to rewrite, so its 3D descendants move, with a warning); without it the old local
		// values now apply relative to the new parent. A parent in another scene, or one that would
		// create a cycle, logs an error and changes nothing.
		void SetParent(Entity parent, bool keepWorldTransform = true);
		void RemoveParent(bool keepWorldTransform = true);
		Entity GetParent() const; // null for a root

		std::uint32_t GetChildCount() const;
		// In insertion order, over a snapshot: fn may reparent or destroy children.
		void ForEachChild(const std::function<void(Entity)>& fn) const;
		std::vector<Entity> GetChildren() const;
		// The first descendant with this name, nearest first; null if none.
		Entity FindChild(std::string_view name, bool recursive = true) const;

		// Computed on demand from the parent chain, never cached, so they are always current. A null
		// entity gives identity.
		glm::mat4 GetWorldTransform() const;
		glm::vec3 GetWorldPosition() const;
		glm::quat GetWorldRotation() const;
		glm::vec3 GetWorldScale() const;
		// Writes the local value that produces this world value. No-ops without a Transform3DComponent.
		void SetWorldPosition(const glm::vec3& position);
		void SetWorldRotation(const glm::quat& rotation);

		// --- Identity / lifetime ---

		UUID GetUUID() const;
		const std::string& GetName() const;
		Scene& GetScene() const;

		bool IsValid() const;
		// Destroys this entity and its descendants (see Scene::DestroyEntity).
		void Destroy();

		explicit operator bool() const { return IsValid(); }
		bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return !(*this == other); }

	private:
		Entity(std::uint32_t handle, Scene* scene) : m_Handle(handle), m_Scene(scene) {}

		void AttachScript(ScriptableEntity* instance);
		ScriptableEntity* GetScriptInstance();

	private:
		static constexpr std::uint32_t k_InvalidHandle = 0xFFFFFFFFu;

		std::uint32_t m_Handle = k_InvalidHandle;
		Scene* m_Scene = nullptr;

		friend class Scene;
	};

}
