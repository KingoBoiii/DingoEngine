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
		// A child's Transform3DComponent and TransformComponent are local to its parent. 3D: world =
		// parentWorld x local. 2D: the position turns with the parent's rotation, z and rotation add,
		// and Size is not inherited. A parent counts as identity in a dimension it has no transform
		// for. A root's local is its world, so nothing changes for an entity that never gets a parent.
		// Destroying or duplicating an entity takes its whole subtree with it.

		// Makes this entity the last child of `parent`, or changes nothing if `parent` already is its
		// parent; a null Entity detaches it. With keepWorldTransform the local transform is rewritten
		// so the entity stays where it is in the world: the Transform3DComponent if it has one, else
		// the TransformComponent (and then its 3D descendants move, with a warning). Without it the
		// old local values now apply relative to the new parent. A parent in another scene, or one
		// that would create a cycle, logs an error and changes nothing.
		void SetParent(Entity parent, bool keepWorldTransform = true);
		// The same, under `joint` of the parent's SkinnedMeshRendererComponent::Model: world =
		// parentWorld x jointFrame x local, so the entity follows the joint as the parent animates (a
		// sword in a hand). The joint passes on its position and rotation, not its scale. Until the
		// model has that joint (an unknown name warns) the entity follows the model's origin. An
		// empty joint is the plain SetParent.
		void SetParent(Entity parent, std::string_view joint, bool keepWorldTransform = true);
		// Without it a string literal would pick the bool overload. A null joint is no joint.
		void SetParent(Entity parent, const char* joint, bool keepWorldTransform = true) { SetParent(parent, joint ? std::string_view(joint) : std::string_view(), keepWorldTransform); }
		void RemoveParent(bool keepWorldTransform = true);
		Entity GetParent() const; // null for a root
		// The joint SetParent attached this entity to; empty when it follows its parent's origin.
		std::string GetParentJoint() const;

		std::uint32_t GetChildCount() const;
		// In insertion order, over a snapshot: fn may reparent or destroy children.
		void ForEachChild(const std::function<void(Entity)>& fn) const;
		std::vector<Entity> GetChildren() const;
		// The first descendant with this name, nearest first; null if none.
		Entity FindChild(std::string_view name, bool recursive = true) const;

		// The 3D world transform, computed on demand from the parent chain and never cached, so it is
		// always current. An entity without a Transform3DComponent (a grouping node) inherits its
		// parent's. A null entity gives identity.
		glm::mat4 GetWorldTransform() const;
		glm::vec3 GetWorldPosition() const;
		glm::quat GetWorldRotation() const;
		glm::vec3 GetWorldScale() const;
		// Writes the local value that produces this world value. No-ops without a Transform3DComponent.
		void SetWorldPosition(const glm::vec3& position);
		void SetWorldRotation(const glm::quat& rotation);

		// The 2D world pose from the TransformComponent chain: position with z, rotation in degrees.
		glm::vec3 GetWorldPosition2D() const;
		float GetWorldRotation2D() const;
		void SetWorldPosition2D(const glm::vec3& position);
		void SetWorldRotation2D(float degrees);

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
