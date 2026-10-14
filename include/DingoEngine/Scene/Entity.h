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

	/// A lightweight, copyable handle to an entity inside a Scene. The underlying
	/// ECS (EnTT) is fully hidden: the handle is an opaque integer and all access
	/// is routed through the engine. Client code never sees or links EnTT.
	/// The scene owns the entity; a handle goes stale (IsValid is false) once the entity is
	/// destroyed or the scene is cleared. A handle never keeps an entity alive.
	class Entity
	{
	public:
		/// A null handle: IsValid is false, and no component call is safe on it.
		Entity() = default;

		// --- Components (built-in component types only) ---
		//
		// AddComponent replaces a component the entity already has (with a warning). GetComponent
		// asserts the entity has it; HasComponent is false and RemoveComponent a no-op on a null
		// or destroyed entity. IDComponent and TagComponent can be read, not added or removed:
		// rename with SetName.

		/// Adds a copy of `component` (default-constructed by default) and returns the stored one;
		/// the reference stays valid until the component is removed or the entity destroyed.
		template<typename T>
		T& AddComponent(const T& component = T{});

		/// The entity's T; asserts the entity has one (check HasComponent when unsure).
		template<typename T>
		T& GetComponent();

		/// True when the entity has a T; false for a null or destroyed entity.
		template<typename T>
		bool HasComponent() const;

		/// Removes the entity's T; does nothing if it has none.
		template<typename T>
		void RemoveComponent();

		// --- Behaviours ---

		/// Attach a script (must derive from ScriptableEntity). The scene owns the
		/// instance and drives its OnCreate/OnUpdate/OnDestroy. An entity holds one script:
		/// adding another replaces it, after the old one's OnDestroy (with a warning).
		/// Returns the new script, which dies with the entity or RemoveScript; never delete it.
		template<typename T, typename... Args>
		T& AddScript(Args&&... args)
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "T must derive from ScriptableEntity");
			T* instance = new T(std::forward<Args>(args)...);
			AttachScript(static_cast<ScriptableEntity*>(instance));
			return *instance;
		}

		/// The entity's script if it is (dynamically) a T, else nullptr.
		template<typename T>
		T* GetScript()
		{
			return dynamic_cast<T*>(GetScriptInstance());
		}

		/// True when the entity's script is (dynamically) a T.
		template<typename T>
		bool HasScript()
		{
			return GetScript<T>() != nullptr;
		}

		/// Detaches the entity's script, running its OnDestroy. Safe from inside that script: the
		/// instance is freed after the scene's update pass.
		void RemoveScript();

		// --- Hierarchy ---
		//
		// A child's Transform3DComponent and TransformComponent are local to its parent. 3D: world
		// = parentWorld x local. 2D: the position turns with the parent's rotation, z and rotation
		// add, and Size is not inherited. A parent counts as identity in a dimension it has no
		// transform for. A root's local is its world, so nothing changes for an entity that never
		// gets a parent. Destroying or duplicating an entity takes its whole subtree with it.

		/// Makes this entity the last child of `parent`, or changes nothing if `parent` already is
		/// its parent; a null Entity detaches it. With keepWorldTransform the local transform is
		/// rewritten so the entity stays where it is in the world: the Transform3DComponent if it
		/// has one, else the TransformComponent (and then its 3D descendants move, with a warning).
		/// Without it the old local values now apply relative to the new parent. A parent in
		/// another scene, or one that would create a cycle, logs an error and changes nothing.
		void SetParent(Entity parent, bool keepWorldTransform = true);
		/// The same, under `joint` of the parent's SkinnedMeshRendererComponent::Model: world =
		/// parentWorld x jointFrame x local, so the entity follows the joint as the parent animates
		/// (a sword in a hand). The joint passes on its position and rotation, not its scale. Until
		/// the model has that joint (an unknown name, or no skinned model yet, warns) the entity
		/// follows the model's origin. Only a 3D child (Transform3DComponent) follows a joint. An
		/// empty joint is the plain SetParent.
		void SetParent(Entity parent, std::string_view joint, bool keepWorldTransform = true);
		/// The same for a C string; without it a string literal would pick the bool overload. A
		/// null joint is no joint.
		void SetParent(Entity parent, const char* joint, bool keepWorldTransform = true) { SetParent(parent, joint ? std::string_view(joint) : std::string_view(), keepWorldTransform); }
		/// Detaches the entity from its parent so it becomes a root; keepWorldTransform behaves as
		/// in SetParent. Does nothing for a root.
		void RemoveParent(bool keepWorldTransform = true);
		/// The parent entity; null for a root.
		Entity GetParent() const;
		/// The joint SetParent attached this entity to; empty when it follows its parent's origin.
		std::string GetParentJoint() const;

		/// The number of direct children; 0 for a null entity.
		std::uint32_t GetChildCount() const;
		/// In insertion order, over a snapshot: fn may reparent or destroy children.
		void ForEachChild(const std::function<void(Entity)>& fn) const;
		/// A snapshot of the direct children in insertion order.
		std::vector<Entity> GetChildren() const;
		/// The first descendant with this name, nearest first; null if none.
		Entity FindChild(std::string_view name, bool recursive = true) const;

		/// The 3D world transform, computed on demand from the parent chain and never cached, so it
		/// is always current. An entity without a Transform3DComponent (a grouping node) inherits
		/// its parent's. A null entity gives identity.
		glm::mat4 GetWorldTransform() const;
		/// The translation, rotation and scale of GetWorldTransform, in world space.
		///@{
		glm::vec3 GetWorldPosition() const;
		glm::quat GetWorldRotation() const;
		glm::vec3 GetWorldScale() const;
		///@}
		/// Writes the local value that produces this world value. No-ops without a
		/// Transform3DComponent.
		///@{
		void SetWorldPosition(const glm::vec3& position);
		void SetWorldRotation(const glm::quat& rotation);
		///@}

		/// The 2D world pose from the TransformComponent chain: position with z, rotation in
		/// degrees.
		glm::vec3 GetWorldPosition2D() const;
		/// The 2D world rotation in degrees, summed through the parents.
		float GetWorldRotation2D() const;
		/// Writes the local TransformComponent value that produces this world position (with z).
		void SetWorldPosition2D(const glm::vec3& position);
		/// Writes the local TransformComponent rotation that produces this world rotation in
		/// degrees.
		void SetWorldRotation2D(float degrees);

		// --- Active state ---
		//
		// An inactive entity and its whole subtree stay in the scene but don't render, simulate,
		// animate, emit particles, light, hear or play audio, or run their scripts. Their physics
		// bodies and controllers are destroyed and built again, at the current transform and at
		// rest, when they become active; a sound stops, and a PlayOnStart source starts again then.
		// A script's OnStart waits until its entity is first active.

		/// Sets the entity's own active flag and refreshes the subtree; does nothing if unchanged.
		void SetActive(bool active);
		/// The entity's own flag, as SetActive left it.
		bool IsActive() const;
		/// Active, and so is every ancestor.
		bool IsActiveInHierarchy() const;

		// --- Identity / lifetime ---

		/// 0 and "" for a null or destroyed entity.
		UUID GetUUID() const;
		/// The entity's name (its TagComponent); an empty string for a null or destroyed entity.
		const std::string& GetName() const;
		/// An empty name becomes "Entity", as in CreateEntity.
		void SetName(std::string_view name);
		/// The scene that owns the entity; asserts on a null entity.
		Scene& GetScene() const;

		/// True while the handle names a live entity in its scene; false for a null or stale
		/// handle.
		bool IsValid() const;
		/// Destroys this entity and its descendants (see Scene::DestroyEntity).
		void Destroy();

		/// Same as IsValid.
		explicit operator bool() const { return IsValid(); }
		/// Compares the handle and scene, not the components.
		bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_Scene == other.m_Scene; }
		/// Negation of operator==.
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
