#pragma once

#include "DingoEngine/Core/UUID.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

#include <entt/entt.hpp>

namespace Dingo
{

	class Entity;
	class Renderer2D;

	// A Scene owns a collection of entities (via an EnTT registry) and knows how
	// to render the renderable ones through a Renderer2D. Game logic ("systems")
	// runs externally by querying the scene with GetAllEntitiesWith<...>().
	class Scene
	{
	public:
		Scene(const std::string& name = "Untitled Scene");
		~Scene();

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		// Destroys every entity in the scene (the Scene object itself stays valid).
		void Clear();

		// Renders all renderable entities. Wraps Renderer2D::BeginScene/EndScene
		// using the scene's view-projection matrix and clears to its clear colour.
		void OnRender(Renderer2D& renderer);

		Entity GetEntityByUUID(UUID uuid);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		void SetViewProjection(const glm::mat4& viewProjection) { m_ViewProjection = viewProjection; }
		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

		void SetClearColor(const glm::vec4& clearColor) { m_ClearColor = clearColor; }
		const glm::vec4& GetClearColor() const { return m_ClearColor; }

		entt::registry& GetRegistry() { return m_Registry; }
		const std::string& GetName() const { return m_Name; }

	private:
		entt::registry m_Registry;
		std::unordered_map<UUID, entt::entity> m_EntityMap;

		std::string m_Name;
		glm::mat4 m_ViewProjection{ 1.0f };
		glm::vec4 m_ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

		friend class Entity;
		friend class SceneManager;
	};

}
