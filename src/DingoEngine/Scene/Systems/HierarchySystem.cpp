#include "depch.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/AnimationSystem.h"

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

				// Identity while the parent's model has no such joint, so the child sits at its origin.
				glm::mat4 SocketFrame(const entt::registry& registry, entt::entity parent, const SocketComponent& socket)
				{
					glm::mat4 frame(1.0f);
					AnimationSystem::JointFrame(registry, parent, socket.Joint, frame);
					return frame;
				}

				glm::quat FrameRotation(const glm::mat4& frame)
				{
					glm::vec3 position, scale;
					glm::quat rotation;
					Decompose(frame, position, rotation, scale);
					return rotation;
				}

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

			glm::mat4 LinkTransform(const entt::registry& registry, entt::entity handle)
			{
				const SocketComponent* socket = registry.try_get<SocketComponent>(handle);
				const entt::entity parent = socket ? GetParent(registry, handle) : entt::null;
				if (parent == entt::null)
					return LocalTransform(registry, handle);

				return SocketFrame(registry, parent, *socket) * LocalTransform(registry, handle);
			}

			void SetSocket(entt::registry& registry, entt::entity child, std::string_view joint)
			{
				if (joint.empty())
				{
					registry.remove<SocketComponent>(child);
					return;
				}

				// Copied before the registry is touched: `joint` may view a socket in the very storage
				// that replacing or growing would overwrite.
				std::string name(joint);
				registry.emplace_or_replace<SocketComponent>(child, SocketComponent{ std::move(name) });
			}

			std::string_view GetSocket(const entt::registry& registry, entt::entity child)
			{
				const SocketComponent* socket = registry.try_get<SocketComponent>(child);
				return socket ? std::string_view(socket->Joint) : std::string_view();
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
					world = world * LinkTransform(registry, *it);

				return world;
			}

			glm::mat4 WorldTransform(const entt::registry& registry, entt::entity handle, const Transform3DComponent& local)
			{
				const entt::entity parent = GetParent(registry, handle);
				if (parent == entt::null)
					return local.GetTransform();

				const SocketComponent* socket = registry.try_get<SocketComponent>(handle);
				if (!socket)
					return WorldTransform(registry, parent) * local.GetTransform();

				return WorldTransform(registry, parent) * (SocketFrame(registry, parent, *socket) * local.GetTransform());
			}

			glm::mat4 ParentWorldTransform(const entt::registry& registry, entt::entity handle)
			{
				const entt::entity parent = GetParent(registry, handle);
				if (parent == entt::null)
					return glm::mat4(1.0f);

				const SocketComponent* socket = registry.try_get<SocketComponent>(handle);
				return socket ? WorldTransform(registry, parent) * SocketFrame(registry, parent, *socket) : WorldTransform(registry, parent);
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
				if (TryInverse(ParentWorldTransform(registry, handle), inverseParent))
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

				const glm::mat4 parentFrame = ParentWorldTransform(registry, handle);
				glm::mat4 inverseParent;
				if (!TryInverse(parentFrame, inverseParent))
					return;

				const glm::quat parentRotation = registry.try_get<SocketComponent>(handle) ? FrameRotation(parentFrame) : WorldRotation(registry, parent);
				local.Rotation = glm::normalize(glm::inverse(parentRotation) * rotation);
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

			void WorldMemo::Begin(const entt::registry& registry)
			{
				m_Registry = &registry;
				m_Transforms3D = registry.storage<Transform3DComponent>();
				m_Transforms2D = registry.storage<TransformComponent>();
				m_Links = registry.storage<HierarchyComponent>();
				if (m_Links && m_Links->empty())
					m_Links = nullptr;
				m_Sockets = registry.storage<SocketComponent>();
				if (m_Sockets && m_Sockets->empty())
					m_Sockets = nullptr;

				if (++m_Pass == 0)
				{
					for (Entry3D& entry : m_World3D)
						entry.Pass = 0;
					for (Entry2D& entry : m_World2D)
						entry.Pass = 0;
					m_Pass = 1;
				}

				if (!m_Links)
					return;

				const std::size_t entities = registry.storage<entt::entity>()->size();
				if (m_World3D.size() < entities)
				{
					m_World3D.resize(entities);
					m_World2D.resize(entities);
				}
			}

			entt::entity WorldMemo::ParentOf(entt::entity handle) const
			{
				return m_Links && m_Links->contains(handle) ? m_Links->get(handle).Parent : entt::null;
			}

			bool WorldMemo::HasParent(entt::entity handle) const
			{
				return ParentOf(handle) != entt::null;
			}

			std::uint32_t WorldMemo::Depth(entt::entity handle) const
			{
				std::uint32_t depth = 0;
				for (entt::entity e = ParentOf(handle); e != entt::null; e = ParentOf(e))
					depth++;
				return depth;
			}

			std::size_t WorldMemo::Slot(entt::entity handle)
			{
				const std::size_t index = static_cast<std::size_t>(entt::to_entity(handle));
				if (index >= m_World3D.size())
				{
					m_World3D.resize(index + 1);
					m_World2D.resize(index + 1);
				}
				return index;
			}

			glm::mat4 WorldMemo::LocalOf(entt::entity handle) const
			{
				return m_Transforms3D && m_Transforms3D->contains(handle) ? m_Transforms3D->get(handle).GetTransform() : glm::mat4(1.0f);
			}

			glm::mat4 WorldMemo::LinkOf(entt::entity handle, const glm::mat4& local) const
			{
				if (!m_Sockets || !m_Sockets->contains(handle))
					return local;

				const entt::entity parent = ParentOf(handle);
				return parent == entt::null ? local : SocketFrame(*m_Registry, parent, m_Sockets->get(handle)) * local;
			}

			glm::mat4 WorldMemo::ParentFrame(entt::entity handle, entt::entity parent)
			{
				if (!m_Sockets || !m_Sockets->contains(handle))
					return World(parent);

				return World(parent) * SocketFrame(*m_Registry, parent, m_Sockets->get(handle));
			}

			void WorldMemo::Forget(entt::entity handle)
			{
				const std::size_t index = static_cast<std::size_t>(entt::to_entity(handle));
				if (index < m_World3D.size())
				{
					m_World3D[index].Pass = 0;
					m_World2D[index].Pass = 0;
				}
			}

			const glm::mat4& WorldMemo::World(entt::entity handle)
			{
				// Climb to the nearest entity already resolved this pass (or past the root), then
				// compose back down, storing every entity on the way for its other descendants.
				m_Chain.clear();
				std::size_t resolved = static_cast<std::size_t>(-1);
				for (entt::entity e = handle; e != entt::null; e = ParentOf(e))
				{
					const std::size_t slot = Slot(e);
					if (m_World3D[slot].Pass == m_Pass)
					{
						resolved = slot;
						break;
					}
					m_Chain.push_back(e);
				}

				for (auto it = m_Chain.rbegin(); it != m_Chain.rend(); ++it)
				{
					Entry3D& entry = m_World3D[Slot(*it)];
					if (resolved == static_cast<std::size_t>(-1))
						entry.World = LocalOf(*it);
					else if (m_Sockets)
						entry.World = m_World3D[resolved].World * LinkOf(*it, LocalOf(*it));
					else
						entry.World = m_World3D[resolved].World * LocalOf(*it);
					entry.Pass = m_Pass;
					resolved = Slot(*it);
				}

				return m_World3D[resolved].World;
			}

			glm::mat4 WorldMemo::Transform(entt::entity handle, const Transform3DComponent& local)
			{
				const HierarchyComponent* links = m_Links && m_Links->contains(handle) ? &m_Links->get(handle) : nullptr;
				if (!links || links->Parent == entt::null)
					return local.GetTransform();

				// Only a parent's world is read again this pass, so a leaf skips the store.
				if (links->FirstChild != entt::null)
					return World(handle);
				if (m_Sockets)
					return World(links->Parent) * LinkOf(handle, local.GetTransform());
				return World(links->Parent) * local.GetTransform();
			}

			glm::vec3 WorldMemo::Position(entt::entity handle)
			{
				if (!HasParent(handle))
					return m_Transforms3D && m_Transforms3D->contains(handle) ? m_Transforms3D->get(handle).Position : glm::vec3(0.0f);
				return glm::vec3(World(handle)[3]);
			}

			glm::quat WorldMemo::Rotation(entt::entity handle)
			{
				if (!HasParent(handle))
					return m_Transforms3D && m_Transforms3D->contains(handle) ? m_Transforms3D->get(handle).Rotation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

				glm::vec3 position, scale;
				glm::quat rotation;
				Decompose(World(handle), position, rotation, scale);
				return rotation;
			}

			glm::vec3 WorldMemo::Scale(entt::entity handle)
			{
				if (!HasParent(handle))
					return m_Transforms3D && m_Transforms3D->contains(handle) ? m_Transforms3D->get(handle).Scale : glm::vec3(1.0f);

				glm::vec3 position, scale;
				glm::quat rotation;
				Decompose(World(handle), position, rotation, scale);
				return scale;
			}

			void WorldMemo::Pose(entt::entity handle, const Transform3DComponent& local, glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
			{
				if (!HasParent(handle))
				{
					position = local.Position;
					rotation = local.Rotation;
					scale = local.Scale;
					return;
				}

				Decompose(World(handle), position, rotation, scale);
			}

			void WorldMemo::World2D(entt::entity handle, glm::vec3& position, float& rotation)
			{
				m_Chain.clear();
				std::size_t resolved = static_cast<std::size_t>(-1);
				for (entt::entity e = handle; e != entt::null; e = ParentOf(e))
				{
					const std::size_t slot = Slot(e);
					if (m_World2D[slot].Pass == m_Pass)
					{
						resolved = slot;
						break;
					}
					m_Chain.push_back(e);
				}

				if (resolved != static_cast<std::size_t>(-1))
				{
					position = m_World2D[resolved].Position;
					rotation = m_World2D[resolved].Rotation;
				}

				for (auto it = m_Chain.rbegin(); it != m_Chain.rend(); ++it)
				{
					const TransformComponent* local = m_Transforms2D && m_Transforms2D->contains(*it) ? &m_Transforms2D->get(*it) : nullptr;
					if (it == m_Chain.rbegin() && resolved == static_cast<std::size_t>(-1))
					{
						position = local ? local->Position : glm::vec3(0.0f);
						rotation = local ? local->Rotation : 0.0f;
					}
					else if (local)
					{
						Compose2D(position, rotation, *local);
					}

					Entry2D& entry = m_World2D[Slot(*it)];
					entry.Position = position;
					entry.Rotation = rotation;
					entry.Pass = m_Pass;
				}
			}

			void WorldMemo::Pose2D(entt::entity handle, const TransformComponent& local, glm::vec3& position, float& rotation)
			{
				const entt::entity parent = ParentOf(handle);
				if (parent == entt::null)
				{
					position = local.Position;
					rotation = local.Rotation;
					return;
				}

				World2D(parent, position, rotation);
				Compose2D(position, rotation, local);
			}

			void WorldMemo::Pose2D(entt::entity handle, glm::vec3& position, float& rotation)
			{
				const TransformComponent* local = m_Transforms2D && m_Transforms2D->contains(handle) ? &m_Transforms2D->get(handle) : nullptr;
				const TransformComponent identity;
				Pose2D(handle, local ? *local : identity, position, rotation);
			}

			glm::mat4 WorldMemo::Transform2D(entt::entity handle, const TransformComponent& local)
			{
				if (!HasParent(handle))
					return local.GetTransform();

				TransformComponent world(local);
				Pose2D(handle, local, world.Position, world.Rotation);
				return world.GetTransform();
			}

			void WorldMemo::SetWorldPosition(entt::entity handle, Transform3DComponent& local, const glm::vec3& position)
			{
				const entt::entity parent = ParentOf(handle);
				if (parent == entt::null)
				{
					local.Position = position;
				}
				else
				{
					glm::mat4 inverseParent;
					if (TryInverse(ParentFrame(handle, parent), inverseParent))
						local.Position = glm::vec3(inverseParent * glm::vec4(position, 1.0f));
				}
				Forget(handle);
			}

			void WorldMemo::SetWorldRotation(entt::entity handle, Transform3DComponent& local, const glm::quat& rotation)
			{
				const entt::entity parent = ParentOf(handle);
				if (parent == entt::null)
				{
					local.Rotation = rotation;
				}
				else
				{
					const glm::mat4 parentFrame = ParentFrame(handle, parent);
					glm::mat4 inverseParent;
					if (TryInverse(parentFrame, inverseParent))
					{
						const glm::quat parentRotation = m_Sockets && m_Sockets->contains(handle) ? FrameRotation(parentFrame) : Rotation(parent);
						local.Rotation = glm::normalize(glm::inverse(parentRotation) * rotation);
					}
				}
				Forget(handle);
			}

			void WorldMemo::SetWorldXY2D(entt::entity handle, TransformComponent& local, const glm::vec2& position)
			{
				const entt::entity parent = ParentOf(handle);
				if (parent == entt::null)
				{
					local.Position.x = position.x;
					local.Position.y = position.y;
				}
				else
				{
					glm::vec3 parentPosition;
					float parentRotation;
					World2D(parent, parentPosition, parentRotation);
					const glm::vec2 xy = Rotate2D(position - glm::vec2(parentPosition), -parentRotation);
					local.Position.x = xy.x;
					local.Position.y = xy.y;
				}
				Forget(handle);
			}

			void WorldMemo::SetWorldRotation2D(entt::entity handle, TransformComponent& local, float rotation)
			{
				const entt::entity parent = ParentOf(handle);
				if (parent == entt::null)
				{
					local.Rotation = rotation;
				}
				else
				{
					glm::vec3 parentPosition;
					float parentRotation;
					World2D(parent, parentPosition, parentRotation);
					local.Rotation = rotation - parentRotation;
				}
				Forget(handle);
			}

		}

	}

}
