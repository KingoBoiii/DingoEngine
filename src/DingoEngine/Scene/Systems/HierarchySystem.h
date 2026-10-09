#pragma once

// Engine-internal: parent-child links between a scene's entities and the world transforms they
// produce. Lives under src/ so EnTT stays a private implementation detail.

#include "DingoEngine/Scene/Components.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

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

		// On a child parented to a joint of its parent's skinned model, whose local transform is then
		// relative to that joint's frame. Kept off HierarchyComponent so a scene without sockets pays
		// one empty-pool check.
		struct SocketComponent
		{
			std::string Joint;
		};

		namespace HierarchySystem
		{

			entt::entity GetParent(const entt::registry& registry, entt::entity handle);
			// Number of ancestors: 0 for a root.
			std::uint32_t Depth(const entt::registry& registry, entt::entity handle);

			// True when `candidate` is `handle` itself or one of its ancestors.
			bool IsSelfOrAncestor(const entt::registry& registry, entt::entity candidate, entt::entity handle);

			// Appends `child` to `parent`'s children, first unlinking it from any previous parent.
			// The caller rules out cycles.
			void Link(entt::registry& registry, entt::entity child, entt::entity parent);
			void Unlink(entt::registry& registry, entt::entity child);

			// An empty joint removes the socket. Link and Unlink leave it alone.
			void SetSocket(entt::registry& registry, entt::entity child, std::string_view joint);
			std::string_view GetSocket(const entt::registry& registry, entt::entity child);

			// Identity without a Transform3DComponent.
			glm::mat4 LocalTransform(const entt::registry& registry, entt::entity handle);
			// What composes onto the parent's world: the local transform, behind the joint frame on a
			// socket. A root's is its local transform.
			glm::mat4 LinkTransform(const entt::registry& registry, entt::entity handle);

			// parentWorld x local, where an entity without a Transform3DComponent counts as identity.
			// A root returns exactly local.GetTransform(), so a flat scene draws what it always did.
			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle);
			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle, const Transform3DComponent& local);
			// The world frame the local transform is relative to: the parent's world, times the joint
			// frame on a socket; identity for a root.
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
			// leaves it alone and returns false when parentWorld has a zero-scale axis.
			bool SetLocalFromWorld(Transform3DComponent& local, const glm::mat4& parentWorld, const glm::mat4& world);

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

			// World transforms for a pass that reads many entities. Each linked entity's world is
			// worked out at most once per pass, root-first, and its children start from it instead of
			// walking the chain again. Begin() opens a pass and forgets every earlier result, so
			// nothing outlives the pass that computed it. Every result is bit-identical to the matching
			// on-demand function above (the same operations in the same order).
			class WorldMemo
			{
			public:
				void Begin(const entt::registry& registry);

				bool HasParent(entt::entity handle) const;
				std::uint32_t Depth(entt::entity handle) const;

				glm::mat4 Transform(entt::entity handle, const Transform3DComponent& local); // WorldTransform; `local` is the entity's own
				const glm::mat4& World(entt::entity handle);                                 // WorldTransform, no local
				glm::vec3 Position(entt::entity handle);                                     // WorldPosition
				glm::quat Rotation(entt::entity handle);                                     // WorldRotation
				glm::vec3 Scale(entt::entity handle);                                        // WorldScale
				void Pose(entt::entity handle, const Transform3DComponent& local, glm::vec3& position, glm::quat& rotation, glm::vec3& scale);

				void Pose2D(entt::entity handle, const TransformComponent& local, glm::vec3& position, float& rotation);
				void Pose2D(entt::entity handle, glm::vec3& position, float& rotation);
				glm::mat4 Transform2D(entt::entity handle, const TransformComponent& local); // WorldTransform2D

				// The on-demand setters, reading the parent's world from this pass. Only the entity's own
				// results are forgotten, not its descendants', so within one pass write parents before
				// reading their descendants (write-back goes shallowest first for this reason).
				void SetWorldPosition(entt::entity handle, Transform3DComponent& local, const glm::vec3& position);
				void SetWorldRotation(entt::entity handle, Transform3DComponent& local, const glm::quat& rotation);
				void SetWorldXY2D(entt::entity handle, TransformComponent& local, const glm::vec2& position);
				void SetWorldRotation2D(entt::entity handle, TransformComponent& local, float rotation);

			private:
				struct Entry3D
				{
					glm::mat4 World;
					std::uint32_t Pass = 0;
				};

				struct Entry2D
				{
					glm::vec3 Position;
					float Rotation;
					std::uint32_t Pass = 0;
				};

				entt::entity ParentOf(entt::entity handle) const;
				std::size_t Slot(entt::entity handle);
				glm::mat4 LocalOf(entt::entity handle) const;
				glm::mat4 LinkOf(entt::entity handle, const glm::mat4& local) const;
				glm::mat4 ParentFrame(entt::entity handle, entt::entity parent);
				void World2D(entt::entity handle, glm::vec3& position, float& rotation);
				void Forget(entt::entity handle);

			private:
				const entt::registry* m_Registry = nullptr;
				const entt::storage_for_t<HierarchyComponent>* m_Links = nullptr;
				const entt::storage_for_t<SocketComponent>* m_Sockets = nullptr;
				const entt::storage_for_t<Transform3DComponent>* m_Transforms3D = nullptr;
				const entt::storage_for_t<TransformComponent>* m_Transforms2D = nullptr;
				std::vector<Entry3D> m_World3D;
				std::vector<Entry2D> m_World2D;
				std::vector<entt::entity> m_Chain;
				std::uint32_t m_Pass = 0;
			};

		}

	}

}
