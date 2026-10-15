#pragma once
#include <DingoEngine.h>

#include "Checks/CheckReport.h"
#include "Checks/StaticChecks.h"
#include "ValleyDirector.h"
#include "World/ValleyWorld.h"

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
		void RunChecks();

	private:
		std::unique_ptr<ValleyDirector> m_Director;
		std::unique_ptr<ValleyWorld> m_World;
		SceneManager m_SceneManager;
		Scene* m_ValleyScene = nullptr;
		CheckReport m_Report;
		std::unique_ptr<StaticCheckRun> m_StaticChecks;
		bool m_SimChecksDone = false;
		bool m_ChecksDone = false;
	};

}
