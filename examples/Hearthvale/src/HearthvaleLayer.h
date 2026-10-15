#pragma once
#include <DingoEngine.h>

#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Checks/CheckReport.h"
#include "Checks/StaticChecks.h"
#include "Life/Player.h"
#include "Life/PlayerIntent.h"
#include "Look/BuildGhost.h"
#include "Look/BuildMenuHud.h"
#include "Look/CameraRig.h"
#include "Look/ClockHud.h"
#include "Look/FarmerAssets.h"
#include "Look/Sky.h"
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
		void ReleaseScenePieces();
		void RegisterStages();
		void RunChecks();
		void UpdateMouseAim();

	private:
		std::unique_ptr<ValleyDirector> m_Director;
		std::unique_ptr<ValleyWorld> m_World;
		std::unique_ptr<Sky> m_Sky;
		std::unique_ptr<FarmerAssets> m_FarmerAssets;
		std::unique_ptr<Player> m_Player;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<ClockHud> m_Hud;
		std::unique_ptr<BuildGhost> m_Ghost;
		std::unique_ptr<BuildMenuHud> m_BuildHud;
		Inventory m_Inventory;
		std::unique_ptr<BuildController> m_Build;
		AimInput m_Aim;
		glm::vec2 m_LastMouse{ 0.0f };
		bool m_MouseSeen = false;
		PlayerIntent m_Intent;
		SceneManager m_SceneManager;
		Scene* m_ValleyScene = nullptr;
		CheckReport m_Report;
		std::unique_ptr<StaticCheckRun> m_StaticChecks;
		bool m_SimChecksDone = false;
		bool m_ChecksDone = false;
	};

}
