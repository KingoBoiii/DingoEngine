#pragma once
#include <DingoEngine.h>

#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Checks/CheckReport.h"
#include "Checks/StaticChecks.h"
#include "Checks/UiChecks.h"
#include "Farm/EconomySim.h"
#include "Farm/FarmSim.h"
#include "Farm/GameState.h"
#include "Farm/SleepFlow.h"
#include "Life/Player.h"
#include "Life/PlayerIntent.h"
#include "Look/BuildGhost.h"
#include "Look/CameraRig.h"
#include "Look/FadeOverlay.h"
#include "Look/FarmerAssets.h"
#include "Look/Sky.h"
#include "Look/WeatherFx.h"
#include "Ui/GameUi.h"
#include "Ui/OptionsModel.h"
#include "Ui/ScreenHost.h"
#include "Ui/SlotProvider.h"
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
		void SetUpMenus();
		void ApplyOptions(const OptionsState& state, bool toEngine);
		const PlayerIntent& ActiveIntent() const;

	private:
		GameState m_State;
		std::unique_ptr<ValleyDirector> m_Director;
		std::unique_ptr<FarmSim> m_Farm;
		std::unique_ptr<EconomySim> m_Economy;
		std::unique_ptr<SleepFlow> m_Sleep;
		WeatherFx m_WeatherFx;
		std::unique_ptr<FadeOverlay> m_Fade;
		std::unique_ptr<ValleyWorld> m_World;
		std::unique_ptr<Sky> m_Sky;
		std::unique_ptr<FarmerAssets> m_FarmerAssets;
		std::unique_ptr<Player> m_Player;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<GameUi> m_Ui;
		std::unique_ptr<BuildGhost> m_Ghost;
		std::unique_ptr<BuildController> m_Build;
		std::unique_ptr<ScreenHost> m_Screens;
		NullSlotProvider m_Slots;
		Settings m_SettingsBase;
		AimInput m_Aim;
		glm::vec2 m_LastMouse{ 0.0f };
		bool m_MouseSeen = false;
		PlayerIntent m_Intent;
		PlayerIntent m_NoIntent;
		SceneManager m_SceneManager;
		Scene* m_ValleyScene = nullptr;
		CheckReport m_Report;
		std::unique_ptr<StaticCheckRun> m_StaticChecks;
		std::unique_ptr<UiCheckRun> m_UiChecks;
		bool m_SimChecksDone = false;
		bool m_UiChecksStarted = false;
		bool m_ChecksDone = false;
	};

}
