#include "depch.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Graphics/Model.h"

#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"

namespace Dingo
{

	namespace
	{

		bool HasTransform3DBelow(const entt::registry& registry, entt::entity handle)
		{
			const Internal::HierarchyComponent* node = registry.try_get<Internal::HierarchyComponent>(handle);
			for (entt::entity child = node ? node->FirstChild : entt::null; child != entt::null; child = registry.get<Internal::HierarchyComponent>(child).NextSibling)
			{
				if (registry.all_of<Transform3DComponent>(child) || HasTransform3DBelow(registry, child))
					return true;
			}

			return false;
		}

		void Reparent(entt::registry& registry, entt::entity child, entt::entity parent, std::string_view joint, bool keepWorldTransform)
		{
			// Only a 2D entity keeps its 2D world: a 3D entity's TransformComponent is unused and keeps its local values.
			const bool is3D = registry.all_of<Transform3DComponent>(child);
			const glm::mat4 world = keepWorldTransform ? Internal::HierarchySystem::WorldTransform(registry, child) : glm::mat4(1.0f);
			glm::vec3 position2D(0.0f);
			float rotation2D = 0.0f;
			if (keepWorldTransform && !is3D)
				Internal::HierarchySystem::WorldPose2D(registry, child, position2D, rotation2D);

			if (parent == entt::null)
				Internal::HierarchySystem::Unlink(registry, child);
			else if (Internal::HierarchySystem::GetParent(registry, child) != parent)
				Internal::HierarchySystem::Link(registry, child, parent);
			Internal::HierarchySystem::SetSocket(registry, child, joint);

			if (!keepWorldTransform)
				return;

			TransformComponent* local2D = is3D ? nullptr : registry.try_get<TransformComponent>(child);
			if (local2D)
			{
				glm::vec3 parentPosition;
				float parentRotation;
				Internal::HierarchySystem::ParentWorldPose2D(registry, child, parentPosition, parentRotation);
				Internal::HierarchySystem::SetLocal2DFromWorld(*local2D, parentPosition, parentRotation, position2D, rotation2D);
			}

			if (Transform3DComponent* local = registry.try_get<Transform3DComponent>(child))
			{
				Internal::HierarchySystem::SetLocalFromWorld(*local, Internal::HierarchySystem::ParentWorldTransform(registry, child), world);
				return;
			}

			if (HasTransform3DBelow(registry, child) && Internal::HierarchySystem::WorldTransform(registry, child) != world)
			{
				const TagComponent* tag = registry.try_get<TagComponent>(child);
				DE_CORE_WARN("SetParent: '{}' has no Transform3DComponent to keep its world transform in, so its 3D descendants move with the new parent",
					tag ? tag->Tag : std::string());
			}
		}

	}

	// All EnTT access is confined to this .cpp. The component methods are declared
	// in the (EnTT-free) header and explicitly instantiated below for the built-in
	// component types, so client translation units never need EnTT.

	template<typename T>
	T& Entity::AddComponent(const T& component)
	{
		return m_Scene->m_Data->Registry.emplace<T>(static_cast<entt::entity>(m_Handle), component);
	}

	template<typename T>
	T& Entity::GetComponent()
	{
		return m_Scene->m_Data->Registry.get<T>(static_cast<entt::entity>(m_Handle));
	}

	template<typename T>
	bool Entity::HasComponent() const
	{
		return m_Scene->m_Data->Registry.all_of<T>(static_cast<entt::entity>(m_Handle));
	}

	template<typename T>
	void Entity::RemoveComponent()
	{
		m_Scene->m_Data->Registry.remove<T>(static_cast<entt::entity>(m_Handle));
	}

	UUID Entity::GetUUID() const
	{
		return m_Scene->m_Data->Registry.get<IDComponent>(static_cast<entt::entity>(m_Handle)).ID;
	}

	const std::string& Entity::GetName() const
	{
		return m_Scene->m_Data->Registry.get<TagComponent>(static_cast<entt::entity>(m_Handle)).Tag;
	}

	Scene& Entity::GetScene() const
	{
		return *m_Scene;
	}

	bool Entity::IsValid() const
	{
		return m_Scene != nullptr
			&& m_Handle != k_InvalidHandle
			&& m_Scene->m_Data->Registry.valid(static_cast<entt::entity>(m_Handle));
	}

	void Entity::Destroy()
	{
		if (m_Scene)
			m_Scene->DestroyEntity(*this);
	}

	void Entity::SetParent(Entity parent, bool keepWorldTransform)
	{
		SetParent(parent, std::string_view(), keepWorldTransform);
	}

	void Entity::SetParent(Entity parent, std::string_view joint, bool keepWorldTransform)
	{
		if (!IsValid())
			return;

		if (!parent.m_Scene && parent.m_Handle == k_InvalidHandle)
		{
			RemoveParent(keepWorldTransform);
			return;
		}

		if (parent.m_Scene != m_Scene)
		{
			DE_CORE_ERROR("SetParent: '{}' can't take a parent from another scene", GetName());
			return;
		}

		if (!parent.IsValid())
		{
			DE_CORE_ERROR("SetParent: '{}' can't take a destroyed entity as its parent", GetName());
			return;
		}

		entt::registry& registry = m_Scene->m_Data->Registry;
		const entt::entity self = static_cast<entt::entity>(m_Handle);
		const entt::entity newParent = static_cast<entt::entity>(parent.m_Handle);

		if (Internal::HierarchySystem::IsSelfOrAncestor(registry, self, newParent))
		{
			DE_CORE_ERROR("SetParent: parenting '{}' to '{}' would make a cycle", GetName(), parent.GetName());
			return;
		}

		if (Internal::HierarchySystem::GetParent(registry, self) == newParent && Internal::HierarchySystem::GetSocket(registry, self) == joint)
			return;

		if (!joint.empty())
		{
			const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(newParent);
			const Skeleton* skeleton = skinned && skinned->Model ? skinned->Model->GetSkeleton() : nullptr;
			if (!skeleton)
				DE_CORE_WARN("SetParent: '{}' has no skinned model yet, so '{}' follows its origin until one with a joint '{}' is set", parent.GetName(), GetName(), joint);
			else if (skeleton->FindJoint(joint) == Skeleton::k_InvalidJoint)
				DE_CORE_WARN("SetParent: '{}' has no joint '{}', so '{}' follows its origin", parent.GetName(), joint, GetName());
			if (!registry.all_of<Transform3DComponent>(self))
				DE_CORE_WARN("SetParent: '{}' has no Transform3DComponent, and only 3D follows a joint, so joint '{}' is ignored", GetName(), joint);
		}

		Reparent(registry, self, newParent, joint, keepWorldTransform);
	}

	void Entity::RemoveParent(bool keepWorldTransform)
	{
		if (!IsValid())
			return;

		entt::registry& registry = m_Scene->m_Data->Registry;
		const entt::entity self = static_cast<entt::entity>(m_Handle);
		if (Internal::HierarchySystem::GetParent(registry, self) != entt::null)
			Reparent(registry, self, entt::null, std::string_view(), keepWorldTransform);
	}

	Entity Entity::GetParent() const
	{
		if (!IsValid())
			return {};

		const entt::entity parent = Internal::HierarchySystem::GetParent(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle));
		return parent != entt::null ? Entity(static_cast<std::uint32_t>(parent), m_Scene) : Entity();
	}

	std::string Entity::GetParentJoint() const
	{
		if (!IsValid())
			return {};

		return std::string(Internal::HierarchySystem::GetSocket(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle)));
	}

	std::uint32_t Entity::GetChildCount() const
	{
		if (!IsValid())
			return 0;

		const Internal::HierarchyComponent* node = m_Scene->m_Data->Registry.try_get<Internal::HierarchyComponent>(static_cast<entt::entity>(m_Handle));
		return node ? node->ChildCount : 0;
	}

	void Entity::ForEachChild(const std::function<void(Entity)>& fn) const
	{
		for (Entity child : GetChildren())
		{
			if (child.IsValid())
				fn(child);
		}
	}

	std::vector<Entity> Entity::GetChildren() const
	{
		std::vector<Entity> children;
		if (!IsValid())
			return children;

		const entt::registry& registry = m_Scene->m_Data->Registry;
		const Internal::HierarchyComponent* node = registry.try_get<Internal::HierarchyComponent>(static_cast<entt::entity>(m_Handle));
		if (!node)
			return children;

		children.reserve(node->ChildCount);
		for (entt::entity child = node->FirstChild; child != entt::null; child = registry.get<Internal::HierarchyComponent>(child).NextSibling)
			children.push_back(Entity(static_cast<std::uint32_t>(child), m_Scene));

		return children;
	}

	Entity Entity::FindChild(std::string_view name, bool recursive) const
	{
		if (!IsValid())
			return {};

		const entt::registry& registry = m_Scene->m_Data->Registry;
		std::vector<entt::entity> frontier{ static_cast<entt::entity>(m_Handle) };
		for (std::size_t i = 0; i < frontier.size(); i++)
		{
			const Internal::HierarchyComponent* node = registry.try_get<Internal::HierarchyComponent>(frontier[i]);
			for (entt::entity child = node ? node->FirstChild : entt::null; child != entt::null; child = registry.get<Internal::HierarchyComponent>(child).NextSibling)
			{
				const TagComponent* tag = registry.try_get<TagComponent>(child);
				if (tag && tag->Tag == name)
					return Entity(static_cast<std::uint32_t>(child), m_Scene);

				if (recursive)
					frontier.push_back(child);
			}
		}

		return {};
	}

	glm::mat4 Entity::GetWorldTransform() const
	{
		if (!IsValid())
			return glm::mat4(1.0f);

		return Internal::HierarchySystem::WorldTransform(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle));
	}

	glm::vec3 Entity::GetWorldPosition() const
	{
		if (!IsValid())
			return glm::vec3(0.0f);

		return Internal::HierarchySystem::WorldPosition(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle));
	}

	glm::quat Entity::GetWorldRotation() const
	{
		if (!IsValid())
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		return Internal::HierarchySystem::WorldRotation(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle));
	}

	glm::vec3 Entity::GetWorldScale() const
	{
		if (!IsValid())
			return glm::vec3(1.0f);

		return Internal::HierarchySystem::WorldScale(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle));
	}

	glm::vec3 Entity::GetWorldPosition2D() const
	{
		if (!IsValid())
			return glm::vec3(0.0f);

		glm::vec3 position;
		float rotation;
		Internal::HierarchySystem::WorldPose2D(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle), position, rotation);
		return position;
	}

	float Entity::GetWorldRotation2D() const
	{
		if (!IsValid())
			return 0.0f;

		glm::vec3 position;
		float rotation;
		Internal::HierarchySystem::WorldPose2D(m_Scene->m_Data->Registry, static_cast<entt::entity>(m_Handle), position, rotation);
		return rotation;
	}

	void Entity::SetWorldPosition(const glm::vec3& position)
	{
		if (!IsValid())
			return;

		entt::registry& registry = m_Scene->m_Data->Registry;
		const entt::entity self = static_cast<entt::entity>(m_Handle);
		if (Transform3DComponent* local = registry.try_get<Transform3DComponent>(self))
			Internal::HierarchySystem::SetWorldPosition(registry, self, *local, position);
	}

	void Entity::SetWorldRotation(const glm::quat& rotation)
	{
		if (!IsValid())
			return;

		entt::registry& registry = m_Scene->m_Data->Registry;
		const entt::entity self = static_cast<entt::entity>(m_Handle);
		if (Transform3DComponent* local = registry.try_get<Transform3DComponent>(self))
			Internal::HierarchySystem::SetWorldRotation(registry, self, *local, rotation);
	}

	void Entity::SetWorldPosition2D(const glm::vec3& position)
	{
		if (!IsValid())
			return;

		entt::registry& registry = m_Scene->m_Data->Registry;
		const entt::entity self = static_cast<entt::entity>(m_Handle);
		if (TransformComponent* local = registry.try_get<TransformComponent>(self))
			Internal::HierarchySystem::SetWorldPosition2D(registry, self, *local, position);
	}

	void Entity::SetWorldRotation2D(float degrees)
	{
		if (!IsValid())
			return;

		entt::registry& registry = m_Scene->m_Data->Registry;
		const entt::entity self = static_cast<entt::entity>(m_Handle);
		if (TransformComponent* local = registry.try_get<TransformComponent>(self))
			Internal::HierarchySystem::SetWorldRotation2D(registry, self, *local, degrees);
	}

	void Entity::AttachScript(ScriptableEntity* instance)
	{
		m_Scene->m_Data->Scripts.Attach(static_cast<entt::entity>(m_Handle), instance, *this);
	}

	ScriptableEntity* Entity::GetScriptInstance()
	{
		return m_Scene->m_Data->Scripts.Find(static_cast<entt::entity>(m_Handle));
	}

	// --- Explicit instantiations for the built-in component types ---------------

#define DE_INSTANTIATE_COMPONENT(T) \
	template T& Entity::AddComponent<T>(const T&); \
	template T& Entity::GetComponent<T>(); \
	template bool Entity::HasComponent<T>() const; \
	template void Entity::RemoveComponent<T>();

	DE_INSTANTIATE_COMPONENT(IDComponent)
	DE_INSTANTIATE_COMPONENT(TagComponent)
	DE_INSTANTIATE_COMPONENT(TransformComponent)
	DE_INSTANTIATE_COMPONENT(SpriteRendererComponent)
	DE_INSTANTIATE_COMPONENT(CircleRendererComponent)
	DE_INSTANTIATE_COMPONENT(TextComponent)
	DE_INSTANTIATE_COMPONENT(CameraComponent)
	DE_INSTANTIATE_COMPONENT(PostProcessComponent)
	DE_INSTANTIATE_COMPONENT(RigidBody2DComponent)
	DE_INSTANTIATE_COMPONENT(BoxCollider2DComponent)
	DE_INSTANTIATE_COMPONENT(CircleCollider2DComponent)
	DE_INSTANTIATE_COMPONENT(Transform3DComponent)
	DE_INSTANTIATE_COMPONENT(MeshRendererComponent)
	DE_INSTANTIATE_COMPONENT(SkinnedMeshRendererComponent)
	DE_INSTANTIATE_COMPONENT(AnimatorComponent)
	DE_INSTANTIATE_COMPONENT(ParticleEmitterComponent)
	DE_INSTANTIATE_COMPONENT(RigidBody3DComponent)
	DE_INSTANTIATE_COMPONENT(BoxCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(SphereCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(CapsuleCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(MeshCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(CharacterController3DComponent)
	DE_INSTANTIATE_COMPONENT(DirectionalLightComponent)
	DE_INSTANTIATE_COMPONENT(AmbientLightComponent)
	DE_INSTANTIATE_COMPONENT(PointLightComponent)
	DE_INSTANTIATE_COMPONENT(SpotLightComponent)
	DE_INSTANTIATE_COMPONENT(AudioSourceComponent)
	DE_INSTANTIATE_COMPONENT(AudioListenerComponent)

#undef DE_INSTANTIATE_COMPONENT

}
