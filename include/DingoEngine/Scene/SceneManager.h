#pragma once

#include <string>
#include <unordered_map>

namespace Dingo
{

	class Scene;
	struct SceneParams;

	/// Owns a set of named scenes and tracks which one is active. Switching scenes
	/// (e.g. Menu -> Game -> GameOver) is a single SetActiveScene() call, which stops the
	/// outgoing scene and starts the incoming one. The manager updates and renders only the
	/// active scene each frame.
	class SceneManager
	{
	public:
		/// An empty manager with no scenes and no active scene.
		SceneManager() = default;
		/// Destroys every scene it owns (see Clear).
		~SceneManager();

		/// Not copyable: it owns its scenes.
		SceneManager(const SceneManager&) = delete;
		/// Not copyable: it owns its scenes.
		SceneManager& operator=(const SceneManager&) = delete;

		/// Creates and registers a scene under the given name (owned by the manager).
		/// If a scene with that name already exists, the existing one is returned.
		/// A scene does not become active until SetActiveScene - that is what starts it.
		Scene* CreateScene(const std::string& name);
		/// As CreateScene, made with the given options. An existing scene of that name is returned
		/// as it is, with its own options.
		Scene* CreateScene(const std::string& name, const SceneParams& params);

		/// The scene registered under `name`, or nullptr if there is none. The manager owns it.
		Scene* GetScene(const std::string& name) const;
		/// True when a scene is registered under `name`.
		bool HasScene(const std::string& name) const;

		/// Makes a scene active: stops the outgoing scene, if any (OnStop), and starts this one
		/// (OnStart, a no-op on a scene already running). So build the scene's entities before
		/// activating it, as OnStart bakes their bodies. No-op if already active; false if the
		/// name is unknown.
		bool SetActiveScene(const std::string& name);
		/// The active scene, or nullptr if none has been activated (or after Clear).
		Scene* GetActiveScene() const { return m_ActiveScene; }
		/// The name the active scene was registered under; empty if there is none.
		const std::string& GetActiveSceneName() const { return m_ActiveSceneName; }

		/// Updates the active scene's behaviours + physics (no-op if none is active), then
		/// checks whether that update requested a scene transition (Scene::RequestSceneTransition,
		/// e.g. from a ScriptableEntity) and, if so, performs the normal SetActiveScene
		/// switch and clears the request. This is how a script switches scenes without
		/// reaching the SceneManager directly.
		void OnUpdate(float deltaTime);

		/// Renders the active scene through the engine's SceneRenderer, reading its
		/// camera + lights from ECS components (no-op if none is active).
		void OnRender();

		/// Destroys all scenes.
		void Clear();

	private:
		std::unordered_map<std::string, Scene*> m_Scenes;
		Scene* m_ActiveScene = nullptr;
		std::string m_ActiveSceneName;
	};

}
