#include "depch.h"
#include "DingoEngine/Scene/SceneManager.h"
#include "DingoEngine/Scene/Scene.h"

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

		if (!m_ActiveScene)
		{
			m_ActiveScene = scene;
			m_ActiveSceneName = name;
		}

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

		m_ActiveScene = it->second;
		m_ActiveSceneName = name;
		return true;
	}

	void SceneManager::OnRender(Renderer2D& renderer)
	{
		if (m_ActiveScene)
			m_ActiveScene->OnRender(renderer);
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
