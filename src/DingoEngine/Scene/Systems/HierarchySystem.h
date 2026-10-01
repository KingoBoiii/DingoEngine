#pragma once

// Engine-internal: parent-child links between a scene's entities and the world transforms they
// produce. Lives under src/ so EnTT stays a private implementation detail.

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace Dingo
{

	struct TransformComponent;
	struct Transform3DComponent;

	namespace Internal
	{

		// Emplaced the first time an entity gains a parent or a child, so a flat scene carries none.
		// Siblings form a doubly linked list in insertion order. Never in DE_INSTANTIATE_COMPONENT:
		// only the hierarchy functions below write it, which keeps every link two-sided.
		struct HierarchyComponent
		{
			entt::entity Parent = entt::null;
			entt::entity FirstChild = entt::null;
			entt::entity LastChild = entt::null;
			entt::entity NextSibling = entt::null;
			entt::entity PrevSibling = entt::null;
			std::uint32_t ChildCount = 0;
		};

		namespace HierarchySystem
		{

			// False while no entity has ever been linked, so a per-entity reader can skip the lookups.
			bool AnyLinks(const entt::registry& registry);

			entt::entity GetParent(const entt::registry& registry, entt::entity handle);
			// Number of ancestors: 0 for a root.
			std::uint32_t Depth(const entt::registry& registry, entt::entity handle);

			// True when `candidate` is `handle` itself or one of its ancestors.
			bool IsSelfOrAncestor(const entt::registry& registry, entt::entity candidate, entt::entity handle);

			// Appends `child` to `parent`'s children, first unlinking it from any previous parent.
			// The caller rules out cycles.
			void Link(entt::registry& registry, entt::entity child, entt::entity parent);
			void Unlink(entt::registry& registry, entt::entity child);

			// Identity without a Transform3DComponent.
			glm::mat4 LocalTransform(const entt::registry& registry, entt::entity handle);

			// parentWorld x local, where an entity without a Transform3DComponent counts as identity.
			// A root returns exactly local.GetTransform(), so a flat scene draws what it always did.
			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle);
			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle, const Transform3DComponent& local);
			glm::mat4 ParentWorldTransform(const entt::registry& registry, entt::entity handle);

			// World position, rotation and scale. A root returns its component's own values untouched;
			// a child's are decomposed from its world matrix (shear from a rotated child under a
			// non-uniformly scaled parent is dropped).
			void WorldPose(const entt::registry& registry, entt::entity handle, const Transform3DComponent& local,
				glm::vec3& position, glm::quat& rotation, glm::vec3& scale);
			glm::vec3 WorldPosition(const entt::registry& registry, entt::entity handle);
			glm::quat WorldRotation(const entt::registry& registry, entt::entity handle);
			glm::vec3 WorldScale(const entt::registry& registry, entt::entity handle);

			// Writes the local position/rotation that put the entity at this world pose. A root takes
			// them as they are. Under a parent with a zero-scale axis the local value is left alone.
			void SetWorldPosition(const entt::registry& registry, entt::entity handle, Transform3DComponent& local, const glm::vec3& position);
			void SetWorldRotation(const entt::registry& registry, entt::entity handle, Transform3DComponent& local, const glm::quat& rotation);

			// Rewrites `local` so that parentWorld x local reproduces `world` as closely as T.R.S allows;
			// leaves it alone when parentWorld has a zero-scale axis.
			void SetLocalFromWorld(Transform3DComponent& local, const glm::mat4& parentWorld, const glm::mat4& world);

			void Decompose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

			// 2D chain, over TransformComponent: a child's position turns with its parent's rotation,
			// z and rotation (degrees) add, and Size is not inherited. An entity without a
			// TransformComponent counts as identity; a root returns its component's values untouched.
			void WorldPose2D(const entt::registry& registry, entt::entity handle, glm::vec3& position, float& rotation);
			void WorldPose2D(const entt::registry& registry, entt::entity handle, const TransformComponent& local, glm::vec3& position, float& rotation);
			void ParentWorldPose2D(const entt::registry& registry, entt::entity handle, glm::vec3& position, float& rotation);
			// Applies `local` under the 2D pose in position/rotation.
			void Compose2D(glm::vec3& position, float& rotation, const TransformComponent& local);
			// The world pose with the entity's own Size, as TransformComponent::GetTransform builds it;
			// exactly local.GetTransform() for a root.
			glm::mat4 WorldTransform2D(const entt::registry& registry, entt::entity handle, const TransformComponent& local);

			// Write the local value that gives this 2D world value; a root takes it as it is.
			// SetWorldXY2D leaves the local z alone.
			void SetWorldPosition2D(const entt::registry& registry, entt::entity handle, TransformComponent& local, const glm::vec3& position);
			void SetWorldXY2D(const entt::registry& registry, entt::entity handle, TransformComponent& local, const glm::vec2& position);
			void SetWorldRotation2D(const entt::registry& registry, entt::entity handle, TransformComponent& local, float rotation);
			void SetLocal2DFromWorld(TransformComponent& local, const glm::vec3& parentPosition, float parentRotation, const glm::vec3& position, float rotation);

		}

	}

}
