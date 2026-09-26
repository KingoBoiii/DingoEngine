#include "depch.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/ScriptableEntity.h"

#include "DingoEngine/Scene/SceneData.h"

namespace Dingo
{

	// All EnTT access is confined to this .cpp. The component methods are declared
	// in the (EnTT-free) header and explicitly instantiated below for the built-in
	// component types, so client translation units never need EnTT.

	namespace
	{
		template<typename T>
		void ClearRuntimeHandles(T&) {}

		void ClearRuntimeHandles(RigidBody2DComponent& component) { component.RuntimeBody = 0; }
		void ClearRuntimeHandles(BoxCollider2DComponent& component) { component.RuntimeShape = 0; }
		void ClearRuntimeHandles(CircleCollider2DComponent& component) { component.RuntimeShape = 0; }
		void ClearRuntimeHandles(RigidBody3DComponent& component) { component.RuntimeBody = k_InvalidBody3D; }
		void ClearRuntimeHandles(CharacterController3DComponent& component) { component.RuntimeController = CharacterController3DComponent::k_InvalidControllerIndex; }
		void ClearRuntimeHandles(AudioSourceComponent& component) { component.RuntimeSound = k_InvalidSound; }
	}

	// A component copied off a live entity still names that entity's body, shape, controller
	// or sound; added as-is, both entities would drive one object and the second destroy would
	// free it twice. The components themselves stay plain values because EnTT relocates them
	// by copy/move, which must carry the handle along.
	template<typename T>
	T& Entity::AddComponent(const T& component)
	{
		T& added = m_Scene->m_Data->Registry.emplace<T>(static_cast<entt::entity>(m_Handle), component);
		ClearRuntimeHandles(added);
		return added;
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
	DE_INSTANTIATE_COMPONENT(RigidBody2DComponent)
	DE_INSTANTIATE_COMPONENT(BoxCollider2DComponent)
	DE_INSTANTIATE_COMPONENT(CircleCollider2DComponent)
	DE_INSTANTIATE_COMPONENT(Transform3DComponent)
	DE_INSTANTIATE_COMPONENT(MeshRendererComponent)
	DE_INSTANTIATE_COMPONENT(RigidBody3DComponent)
	DE_INSTANTIATE_COMPONENT(BoxCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(SphereCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(CapsuleCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(MeshCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(CharacterController3DComponent)
	DE_INSTANTIATE_COMPONENT(DirectionalLightComponent)
	DE_INSTANTIATE_COMPONENT(AudioSourceComponent)
	DE_INSTANTIATE_COMPONENT(AudioListenerComponent)

#undef DE_INSTANTIATE_COMPONENT

}
