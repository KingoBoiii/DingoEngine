#pragma once
#include <DingoEngine.h>

#include "ValleyDirector.h"

#include <memory>

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
		std::unique_ptr<ValleyDirector> m_Director;
		SceneManager m_SceneManager;
		Scene* m_ValleyScene = nullptr;
		bool m_ChecksDone = false;
	};

}
