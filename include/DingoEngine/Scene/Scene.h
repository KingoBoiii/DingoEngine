#pragma once

#include "DingoEngine/Core/UUID.h"

#include <glm/glm.hpp>

#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	class Entity;
	class Renderer2D;
	class ScriptableEntity;

	namespace Internal { struct SceneData; }

	// A Scene owns a collection of entities and the behaviours attached to them,
	// and knows how to render the renderable ones. The ECS backend (EnTT) is held
	// behind an opaque pointer so it never appears in this public header.
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
		bool IsValid(Entity entity) const;

		// Destroys every entity (and its scripts) in the scene; the Scene stays usable.
		void Clear();

		// Drives every attached ScriptableEntity's OnUpdate. Safe to create/destroy
		// entities from within a script — destroys are deferred to the end of the pass.
		void OnUpdate(float deltaTime);

		// Draws all entities that have a Transform plus a Sprite/Circle/Text component,
		// wrapping Renderer2D::BeginScene/EndScene with the scene's view-projection.
		void OnRender(Renderer2D& renderer);

		// Issues only the entity draw calls (no BeginScene/Clear/EndScene). Call it
		// between your own Renderer2D::BeginScene/EndScene to compose the scene's
		// entities with custom drawing (e.g. a HUD) in a single pass / camera.
		void RenderEntities(Renderer2D& renderer);

		void ForEachEntity(const std::function<void(Entity)>& fn);

		// Returns every attached script that is (dynamically) a T. Handy for systems
		// that need to find other entities by behaviour, e.g. all invaders.
		template<typename T>
		std::vector<T*> GetScriptsOfType()
		{
			std::vector<T*> result;
			ForEachScript([&result](ScriptableEntity* script)
			{
				if (T* typed = dynamic_cast<T*>(script))
					result.push_back(typed);
			});
			return result;
		}

		Entity GetEntityByUUID(UUID uuid);

		void SetViewProjection(const glm::mat4& viewProjection) { m_ViewProjection = viewProjection; }
		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

		void SetClearColor(const glm::vec4& clearColor) { m_ClearColor = clearColor; }
		const glm::vec4& GetClearColor() const { return m_ClearColor; }

		const std::string& GetName() const { return m_Name; }

	private:
		void ForEachScript(const std::function<void(ScriptableEntity*)>& fn);
		void DestroyEntityNow(std::uint32_t handle);
		Entity Wrap(std::uint32_t handle);

	private:
		Internal::SceneData* m_Data = nullptr;

		std::string m_Name;
		glm::mat4 m_ViewProjection{ 1.0f };
		glm::vec4 m_ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

		friend class Entity;
		friend class SceneManager;
	};

}
