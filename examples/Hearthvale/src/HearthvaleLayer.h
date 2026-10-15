#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class HearthvaleLayer : public Layer
	{
	public:
		HearthvaleLayer();
		virtual ~HearthvaleLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;

	private:
		void BuildValleyScene();

	private:
		SceneManager m_SceneManager;
		Scene* m_ValleyScene = nullptr;
		bool m_ChecksDone = false;
	};

}
