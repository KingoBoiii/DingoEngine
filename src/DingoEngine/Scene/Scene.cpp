#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"

namespace Dingo
{

	Scene::Scene(const std::string& name)
		: m_Name(name)
	{
	}

	Scene::~Scene()
	{
		m_Registry.clear();
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);

		m_EntityMap[uuid] = entity;
		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity)
			return;

		m_EntityMap.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	void Scene::Clear()
	{
		m_Registry.clear();
		m_EntityMap.clear();
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto it = m_EntityMap.find(uuid);
		if (it != m_EntityMap.end())
			return { it->second, this };

		return {};
	}

	void Scene::OnRender(Renderer2D& renderer)
	{
		renderer.BeginScene(m_ViewProjection);
		renderer.Clear(m_ClearColor);

		// Sprites (solid-colour or textured quads)
		{
			auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);

				// A null texture maps to the engine white texture so the colour
				// shows through — this keeps a single code path for both cases.
				Texture* texture = sprite.Texture ? sprite.Texture : Renderer::GetWhiteTexture();

				if (transform.Rotation != 0.0f)
					renderer.DrawRotatedQuad(transform.Position, transform.Rotation, transform.Size, texture, sprite.Color);
				else
					renderer.DrawQuad(transform.Position, transform.Size, texture, sprite.Color);
			}
		}

		// Circles
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
				renderer.DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade);
			}
		}

		// Text
		{
			auto view = m_Registry.view<TransformComponent, TextComponent>();
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

		renderer.EndScene();
	}

}
