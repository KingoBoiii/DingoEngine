#pragma once

#include <string>
#include <unordered_map>

namespace Dingo
{

	class Scene;

	// Owns a set of named scenes and tracks which one is active. Switching scenes
	// (e.g. Menu -> Game -> GameOver) is a single SetActiveScene() call, which stops
	// the outgoing scene and starts the incoming one; the manager updates and renders
	// only the active scene each frame.
	class SceneManager
	{
	public:
		SceneManager() = default;
		~SceneManager();

		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		// Creates and registers a scene under the given name (owned by the manager).
		// If a scene with that name already exists, the existing one is returned.
		// A scene does not become active until SetActiveScene — that is what starts it.
		Scene* CreateScene(const std::string& name);

		Scene* GetScene(const std::string& name) const;
		bool HasScene(const std::string& name) const;

		// Activates a scene by name: stops the outgoing scene (OnStop) and starts the
		// incoming one (OnStart). A no-op if it is already active; false if unknown.
		bool SetActiveScene(const std::string& name);
		Scene* GetActiveScene() const { return m_ActiveScene; }
		const std::string& GetActiveSceneName() const { return m_ActiveSceneName; }

		// Updates the active scene's behaviours + physics (no-op if none is active).
		void OnUpdate(float deltaTime);

		// Renders the active scene through the engine's SceneRenderer, reading its
		// camera + lights from ECS components (no-op if none is active).
		void OnRender();

		// Destroys all scenes.
		void Clear();

	private:
		std::unordered_map<std::string, Scene*> m_Scenes;
		Scene* m_ActiveScene = nullptr;
		std::string m_ActiveSceneName;
	};

}
