#include "depch.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include "DingoEngine/Scene/Components.h"

#include <cmath>
#include <vector>

namespace Dingo
{

	namespace Internal
	{

		namespace HierarchySystem
		{

			namespace
			{

				glm::vec3 NormalizeOr(const glm::vec3& v, const glm::vec3& fallback)
				{
					const float length = glm::length(v);
					return length > 1e-12f ? v / length : fallback;
				}

				// A parent with a zero-scale axis has no inverse; callers keep the old local instead.
				bool TryInverse(const glm::mat4& matrix, glm::mat4& inverse)
				{
					const float determinant = glm::determinant(glm::mat3(matrix));
					if (!(std::abs(determinant) > 1e-24f))
						return false;

					inverse = glm::inverse(matrix);
					return true;
				}

			}

			bool AnyLinks(const entt::registry& registry)
			{
				const auto* storage = registry.storage<HierarchyComponent>();
				return storage && !storage->empty();
			}

			entt::entity GetParent(const entt::registry& registry, entt::entity handle)
			{
				const HierarchyComponent* node = registry.try_get<HierarchyComponent>(handle);
				return node ? node->Parent : entt::null;
			}

			std::uint32_t Depth(const entt::registry& registry, entt::entity handle)
			{
				std::uint32_t depth = 0;
				for (entt::entity e = GetParent(registry, handle); e != entt::null; e = GetParent(registry, e))
					depth++;
				return depth;
			}

			glm::mat4 LocalTransform(const entt::registry& registry, entt::entity handle)
			{
				const Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle);
				return transform ? transform->GetTransform() : glm::mat4(1.0f);
			}

			bool IsSelfOrAncestor(const entt::registry& registry, entt::entity candidate, entt::entity handle)
			{
				for (entt::entity e = handle; e != entt::null; e = GetParent(registry, e))
				{
					if (e == candidate)
						return true;
				}

				return false;
			}

			void Link(entt::registry& registry, entt::entity child, entt::entity parent)
			{
				Unlink(registry, child);

				if (!registry.all_of<HierarchyComponent>(parent))
					registry.emplace<HierarchyComponent>(parent);
				if (!registry.all_of<HierarchyComponent>(child))
					registry.emplace<HierarchyComponent>(child);

				HierarchyComponent& parentNode = registry.get<HierarchyComponent>(parent);
				HierarchyComponent& childNode = registry.get<HierarchyComponent>(child);

				childNode.Parent = parent;
				childNode.PrevSibling = parentNode.LastChild;
				childNode.NextSibling = entt::null;

				if (parentNode.LastChild != entt::null)
					registry.get<HierarchyComponent>(parentNode.LastChild).NextSibling = child;
				else
					parentNode.FirstChild = child;

				parentNode.LastChild = child;
				parentNode.ChildCount++;
			}

			void Unlink(entt::registry& registry, entt::entity child)
			{
				HierarchyComponent* childNode = registry.try_get<HierarchyComponent>(child);
				if (!childNode || childNode->Parent == entt::null)
					return;

				HierarchyComponent& parentNode = registry.get<HierarchyComponent>(childNode->Parent);

				if (childNode->PrevSibling != entt::null)
					registry.get<HierarchyComponent>(childNode->PrevSibling).NextSibling = childNode->NextSibling;
				else
					parentNode.FirstChild = childNode->NextSibling;

				if (childNode->NextSibling != entt::null)
					registry.get<HierarchyComponent>(childNode->NextSibling).PrevSibling = childNode->PrevSibling;
				else
					parentNode.LastChild = childNode->PrevSibling;

				parentNode.ChildCount--;

				childNode->Parent = entt::null;
				childNode->PrevSibling = entt::null;
				childNode->NextSibling = entt::null;
			}

			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle)
			{
				entt::entity parent = GetParent(registry, handle);
				if (parent == entt::null)
					return LocalTransform(registry, handle);

				// Composed root-first, so a child's world is bit-for-bit its parent's world x its local.
				thread_local std::vector<entt::entity> chain;
				chain.clear();
				chain.push_back(handle);
				for (; parent != entt::null; parent = GetParent(registry, parent))
					chain.push_back(parent);

				glm::mat4 world = LocalTransform(registry, chain.back());
				for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it)
					world = world * LocalTransform(registry, *it);

				return world;
			}

			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle, const Transform3DComponent& local)
			{
				const entt::entity parent = GetParent(registry, handle);
				if (parent == entt::null)
					return local.GetTransform();

				return WorldTransform(registry, parent) * local.GetTransform();
			}

			glm::mat4 ParentWorldTransform(const entt::registry& registry, entt::entity handle)
			{
				const entt::entity parent = GetParent(registry, handle);
				return parent != entt::null ? WorldTransform(registry, parent) : glm::mat4(1.0f);
			}

			void WorldPose(const entt::registry& registry, entt::entity handle, const Transform3DComponent& local,
				glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					position = local.Position;
					rotation = local.Rotation;
					scale = local.Scale;
					return;
				}

				Decompose(WorldTransform(registry, handle, local), position, rotation, scale);
			}

			glm::vec3 WorldPosition(const entt::registry& registry, entt::entity handle)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					const Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle);
					return transform ? transform->Position : glm::vec3(0.0f);
				}

				return glm::vec3(WorldTransform(registry, handle)[3]);
			}

			glm::quat WorldRotation(const entt::registry& registry, entt::entity handle)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					const Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle);
					return transform ? transform->Rotation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
				}

				glm::vec3 position, scale;
				glm::quat rotation;
				Decompose(WorldTransform(registry, handle), position, rotation, scale);
				return rotation;
			}

			glm::vec3 WorldScale(const entt::registry& registry, entt::entity handle)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					const Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle);
					return transform ? transform->Scale : glm::vec3(1.0f);
				}

				glm::vec3 position, scale;
				glm::quat rotation;
				Decompose(WorldTransform(registry, handle), position, rotation, scale);
				return scale;
			}

			void SetWorldPosition(const entt::registry& registry, entt::entity handle, Transform3DComponent& local, const glm::vec3& position)
			{
				const entt::entity parent = GetParent(registry, handle);
				if (parent == entt::null)
				{
					local.Position = position;
					return;
				}

				glm::mat4 inverseParent;
				if (TryInverse(WorldTransform(registry, parent), inverseParent))
					local.Position = glm::vec3(inverseParent * glm::vec4(position, 1.0f));
			}

			void SetWorldRotation(const entt::registry& registry, entt::entity handle, Transform3DComponent& local, const glm::quat& rotation)
			{
				const entt::entity parent = GetParent(registry, handle);
				if (parent == entt::null)
				{
					local.Rotation = rotation;
					return;
				}

				glm::mat4 inverseParent;
				if (TryInverse(WorldTransform(registry, parent), inverseParent))
					local.Rotation = glm::normalize(glm::inverse(WorldRotation(registry, parent)) * rotation);
			}

			void SetLocalFromWorld(Transform3DComponent& local, const glm::mat4& parentWorld, const glm::mat4& world)
			{
				glm::mat4 inverseParent;
				if (TryInverse(parentWorld, inverseParent))
					Decompose(inverseParent * world, local.Position, local.Rotation, local.Scale);
			}

			void Decompose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
			{
				translation = glm::vec3(matrix[3]);

				const glm::vec3 axisX(matrix[0]);
				const glm::vec3 axisY(matrix[1]);
				const glm::vec3 axisZ(matrix[2]);
				scale = { glm::length(axisX), glm::length(axisY), glm::length(axisZ) };
				if (glm::dot(glm::cross(axisX, axisY), axisZ) < 0.0f)
					scale.x = -scale.x;

				// Gram-Schmidt from X: shear has nowhere to go in T.R.S, so it is dropped.
				const glm::vec3 x = NormalizeOr(scale.x < 0.0f ? -axisX : axisX, { 1.0f, 0.0f, 0.0f });
				const glm::vec3 helper = std::abs(x.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 y = NormalizeOr(axisY - glm::dot(axisY, x) * x, glm::normalize(helper - glm::dot(helper, x) * x));
				const glm::vec3 z = glm::cross(x, y);

				rotation = glm::normalize(glm::quat_cast(glm::mat3(x, y, z)));
			}

			namespace
			{

				glm::vec2 Rotate2D(const glm::vec2& v, float degrees)
				{
					const float radians = glm::radians(degrees);
					const float c = std::cos(radians);
					const float s = std::sin(radians);
					return { c * v.x - s * v.y, s * v.x + c * v.y };
				}

			}

			void Compose2D(glm::vec3& position, float& rotation, const TransformComponent& local)
			{
				const glm::vec2 offset = Rotate2D(glm::vec2(local.Position), rotation);
				position = { position.x + offset.x, position.y + offset.y, position.z + local.Position.z };
				rotation += local.Rotation;
			}

			void WorldPose2D(const entt::registry& registry, entt::entity handle, glm::vec3& position, float& rotation)
			{
				const TransformComponent* local = registry.try_get<TransformComponent>(handle);
				const TransformComponent identity;
				WorldPose2D(registry, handle, local ? *local : identity, position, rotation);
			}

			void WorldPose2D(const entt::registry& registry, entt::entity handle, const TransformComponent& local, glm::vec3& position, float& rotation)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					position = local.Position;
					rotation = local.Rotation;
					return;
				}

				ParentWorldPose2D(registry, handle, position, rotation);
				Compose2D(position, rotation, local);
			}

			void ParentWorldPose2D(const entt::registry& registry, entt::entity handle, glm::vec3& position, float& rotation)
			{
				position = glm::vec3(0.0f);
				rotation = 0.0f;

				thread_local std::vector<entt::entity> chain;
				chain.clear();
				for (entt::entity e = GetParent(registry, handle); e != entt::null; e = GetParent(registry, e))
					chain.push_back(e);

				for (auto it = chain.rbegin(); it != chain.rend(); ++it)
				{
					const TransformComponent* ancestor = registry.try_get<TransformComponent>(*it);
					if (it == chain.rbegin())
					{
						position = ancestor ? ancestor->Position : glm::vec3(0.0f);
						rotation = ancestor ? ancestor->Rotation : 0.0f;
					}
					else if (ancestor)
					{
						Compose2D(position, rotation, *ancestor);
					}
				}
			}

			glm::mat4 WorldTransform2D(const entt::registry& registry, entt::entity handle, const TransformComponent& local)
			{
				if (GetParent(registry, handle) == entt::null)
					return local.GetTransform();

				TransformComponent world(local);
				WorldPose2D(registry, handle, local, world.Position, world.Rotation);
				return world.GetTransform();
			}

			void SetWorldPosition2D(const entt::registry& registry, entt::entity handle, TransformComponent& local, const glm::vec3& position)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					local.Position = position;
					return;
				}

				glm::vec3 parentPosition;
				float parentRotation;
				ParentWorldPose2D(registry, handle, parentPosition, parentRotation);
				const glm::vec2 xy = Rotate2D(glm::vec2(position) - glm::vec2(parentPosition), -parentRotation);
				local.Position = { xy.x, xy.y, position.z - parentPosition.z };
			}

			void SetWorldXY2D(const entt::registry& registry, entt::entity handle, TransformComponent& local, const glm::vec2& position)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					local.Position.x = position.x;
					local.Position.y = position.y;
					return;
				}

				glm::vec3 parentPosition;
				float parentRotation;
				ParentWorldPose2D(registry, handle, parentPosition, parentRotation);
				const glm::vec2 xy = Rotate2D(position - glm::vec2(parentPosition), -parentRotation);
				local.Position.x = xy.x;
				local.Position.y = xy.y;
			}

			void SetWorldRotation2D(const entt::registry& registry, entt::entity handle, TransformComponent& local, float rotation)
			{
				if (GetParent(registry, handle) == entt::null)
				{
					local.Rotation = rotation;
					return;
				}

				glm::vec3 parentPosition;
				float parentRotation;
				ParentWorldPose2D(registry, handle, parentPosition, parentRotation);
				local.Rotation = rotation - parentRotation;
			}

			void SetLocal2DFromWorld(TransformComponent& local, const glm::vec3& parentPosition, float parentRotation, const glm::vec3& position, float rotation)
			{
				const glm::vec2 xy = Rotate2D(glm::vec2(position) - glm::vec2(parentPosition), -parentRotation);
				local.Position = { xy.x, xy.y, position.z - parentPosition.z };
				local.Rotation = rotation - parentRotation;
			}

		}

	}

}
