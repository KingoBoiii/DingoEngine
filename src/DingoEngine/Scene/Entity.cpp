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

	void Entity::AttachScript(ScriptableEntity* instance)
	{
		instance->m_Entity = *this;
		m_Scene->m_Data->Scripts[static_cast<entt::entity>(m_Handle)].reset(instance);
		instance->OnCreate();
	}

	ScriptableEntity* Entity::GetScriptInstance()
	{
		auto& scripts = m_Scene->m_Data->Scripts;
		auto it = scripts.find(static_cast<entt::entity>(m_Handle));
		return it != scripts.end() ? it->second.get() : nullptr;
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
	DE_INSTANTIATE_COMPONENT(RigidBody2DComponent)
	DE_INSTANTIATE_COMPONENT(BoxCollider2DComponent)
	DE_INSTANTIATE_COMPONENT(CircleCollider2DComponent)
	DE_INSTANTIATE_COMPONENT(Transform3DComponent)
	DE_INSTANTIATE_COMPONENT(MeshRendererComponent)
	DE_INSTANTIATE_COMPONENT(RigidBody3DComponent)
	DE_INSTANTIATE_COMPONENT(BoxCollider3DComponent)
	DE_INSTANTIATE_COMPONENT(SphereCollider3DComponent)

#undef DE_INSTANTIATE_COMPONENT

}
