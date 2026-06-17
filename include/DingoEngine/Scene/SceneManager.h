#pragma once

#include <string>
#include <unordered_map>

namespace Dingo
{

	class Scene;
	class Renderer2D;

	// Owns a set of named scenes and tracks which one is active. Switching scenes
	// (e.g. Menu -> Game -> GameOver) is a single SetActiveScene() call; the manager
	// renders only the active scene each frame.
	class SceneManager
	{
	public:
		SceneManager() = default;
		~SceneManager();

		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		// Creates and registers a scene under the given name (owned by the manager).
		// If a scene with that name already exists, the existing one is returned.
		// The first scene created becomes the active scene.
		Scene* CreateScene(const std::string& name);

		Scene* GetScene(const std::string& name) const;
		bool HasScene(const std::string& name) const;

		bool SetActiveScene(const std::string& name);
		Scene* GetActiveScene() const { return m_ActiveScene; }
		const std::string& GetActiveSceneName() const { return m_ActiveSceneName; }

		// Updates the active scene's behaviours (no-op if none is active).
		void OnUpdate(float deltaTime);

		// Renders the active scene (no-op if none is active).
		void OnRender(Renderer2D& renderer);

		// Destroys all scenes.
		void Clear();

	private:
		std::unordered_map<std::string, Scene*> m_Scenes;
		Scene* m_ActiveScene = nullptr;
		std::string m_ActiveSceneName;
	};

}
