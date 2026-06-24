#include "depch.h"
#include "DingoEngine/Scene/SceneManager.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/SceneRenderer.h"

#include "DingoEngine/Core/Application.h"

namespace Dingo
{

	SceneManager::~SceneManager()
	{
		Clear();
	}

	Scene* SceneManager::CreateScene(const std::string& name)
	{
		if (auto it = m_Scenes.find(name); it != m_Scenes.end())
		{
			DE_CORE_WARN("SceneManager: scene '{}' already exists; returning the existing scene.", name);
			return it->second;
		}

		Scene* scene = new Scene(name);
		m_Scenes[name] = scene;
		return scene;
	}

	Scene* SceneManager::GetScene(const std::string& name) const
	{
		auto it = m_Scenes.find(name);
		return it != m_Scenes.end() ? it->second : nullptr;
	}

	bool SceneManager::HasScene(const std::string& name) const
	{
		return m_Scenes.contains(name);
	}

	bool SceneManager::SetActiveScene(const std::string& name)
	{
		auto it = m_Scenes.find(name);
		if (it == m_Scenes.end())
		{
			DE_CORE_ERROR("SceneManager: cannot activate unknown scene '{}'.", name);
			return false;
		}

		Scene* target = it->second;
		if (target == m_ActiveScene)
			return true; // already active — no lifecycle churn

		// Stop the outgoing scene, then start the incoming one.
		if (m_ActiveScene)
			m_ActiveScene->OnStop();

		m_ActiveScene = target;
		m_ActiveSceneName = name;
		m_ActiveScene->OnStart();
		return true;
	}

	void SceneManager::OnUpdate(float deltaTime)
	{
		if (m_ActiveScene)
			m_ActiveScene->OnUpdate(deltaTime);
	}

	void SceneManager::OnRender()
	{
		if (m_ActiveScene)
			Application::Get().GetSceneRenderer().Render(*m_ActiveScene);
	}

	void SceneManager::Clear()
	{
		for (auto& [name, scene] : m_Scenes)
			delete scene;

		m_Scenes.clear();
		m_ActiveScene = nullptr;
		m_ActiveSceneName.clear();
	}

}
