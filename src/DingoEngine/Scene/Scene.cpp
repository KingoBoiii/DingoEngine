#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include "DingoEngine/Scene/SceneData.h"

#include <algorithm>

namespace Dingo
{

	Scene::Scene(const std::string& name)
		: m_Data(new Internal::SceneData()), m_Name(name)
	{
	}

	Scene::~Scene()
	{
		Clear();
		delete m_Data;
	}

	Entity Scene::Wrap(std::uint32_t handle)
	{
		return Entity(handle, this);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		entt::entity handle = m_Data->Registry.create();
		m_Data->Registry.emplace<IDComponent>(handle, uuid);
		m_Data->Registry.emplace<TransformComponent>(handle);
		m_Data->Registry.emplace<TagComponent>(handle, name.empty() ? std::string("Entity") : name);

		m_Data->EntityMap[uuid] = handle;
		return Wrap(static_cast<std::uint32_t>(handle));
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity)
			return;

		if (m_Data->Updating)
		{
			m_Data->PendingDestroy.push_back(static_cast<entt::entity>(entity.m_Handle));
			return;
		}

		DestroyEntityNow(entity.m_Handle);
	}

	void Scene::DestroyEntityNow(std::uint32_t handle)
	{
		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.valid(e))
			return;

		if (auto it = m_Data->Scripts.find(e); it != m_Data->Scripts.end())
		{
			it->second->OnDestroy();
			m_Data->Scripts.erase(it);
		}

		if (m_Data->Registry.all_of<IDComponent>(e))
			m_Data->EntityMap.erase(m_Data->Registry.get<IDComponent>(e).ID);

		m_Data->Registry.destroy(e);
	}

	bool Scene::IsValid(Entity entity) const
	{
		return entity.m_Scene == this
			&& m_Data->Registry.valid(static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::Clear()
	{
		for (auto& [handle, script] : m_Data->Scripts)
			script->OnDestroy();

		m_Data->Scripts.clear();
		m_Data->Registry.clear();
		m_Data->EntityMap.clear();
		m_Data->PendingDestroy.clear();
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto it = m_Data->EntityMap.find(uuid);
		if (it != m_Data->EntityMap.end())
			return Wrap(static_cast<std::uint32_t>(it->second));

		return {};
	}

	void Scene::OnUpdate(float deltaTime)
	{
		m_Data->Updating = true;

		// Snapshot the current scripts so spawning new entities mid-update doesn't
		// invalidate iteration (new scripts run next frame).
		std::vector<entt::entity> handles;
		handles.reserve(m_Data->Scripts.size());
		for (auto& [handle, script] : m_Data->Scripts)
			handles.push_back(handle);

		for (entt::entity handle : handles)
		{
			auto it = m_Data->Scripts.find(handle);
			if (it != m_Data->Scripts.end())
				it->second->OnUpdate(deltaTime);
		}

		m_Data->Updating = false;

		for (entt::entity handle : m_Data->PendingDestroy)
			DestroyEntityNow(static_cast<std::uint32_t>(handle));
		m_Data->PendingDestroy.clear();
	}

	void Scene::ForEachEntity(const std::function<void(Entity)>& fn)
	{
		auto view = m_Data->Registry.view<IDComponent>();
		for (entt::entity handle : view)
			fn(Wrap(static_cast<std::uint32_t>(handle)));
	}

	void Scene::ForEachScript(const std::function<void(ScriptableEntity*)>& fn)
	{
		for (auto& [handle, script] : m_Data->Scripts)
			fn(script.get());
	}

	void Scene::OnRender(Renderer2D& renderer)
	{
		renderer.BeginScene(m_ViewProjection);
		renderer.Clear(m_ClearColor);
		RenderEntities(renderer);
		renderer.EndScene();
	}

	void Scene::RenderEntities(Renderer2D& renderer)
	{
		// Sprites (solid-colour or textured quads), painter-sorted by z: a higher
		// Position.z draws on top. stable_sort keeps creation order within a layer.
		{
			auto view = m_Data->Registry.view<TransformComponent, SpriteRendererComponent>();
			std::vector<entt::entity> sprites(view.begin(), view.end());
			std::stable_sort(sprites.begin(), sprites.end(), [this](entt::entity a, entt::entity b)
			{
				return m_Data->Registry.get<TransformComponent>(a).Position.z
				     < m_Data->Registry.get<TransformComponent>(b).Position.z;
			});

			for (entt::entity entity : sprites)
			{
				auto& transform = m_Data->Registry.get<TransformComponent>(entity);
				auto& sprite = m_Data->Registry.get<SpriteRendererComponent>(entity);
				Texture* texture = sprite.Texture ? sprite.Texture : Renderer::GetWhiteTexture();

				if (transform.Rotation != 0.0f)
					renderer.DrawRotatedQuad(transform.Position, transform.Rotation, transform.Size, texture, sprite.Color);
				else
					renderer.DrawQuad(transform.Position, transform.Size, texture, sprite.Color);
			}
		}

		// Circles
		{
			auto view = m_Data->Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
				renderer.DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade);
			}
		}

		// Text
		{
			auto view = m_Data->Registry.view<TransformComponent, TextComponent>();
			for (auto entity : view)
			{
				auto [transform, text] = view.get<TransformComponent, TextComponent>(entity);
				if (!text.Font || text.Text.empty())
					continue;

				glm::vec3 position = transform.Position;
				if (text.Centered)
					position.x -= text.Font->GetStringWidth(text.Text, text.Size) * 0.5f;

				renderer.DrawText(text.Text, text.Font, position, text.Size, { text.Color });
			}
		}
	}

}
