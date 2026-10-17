#pragma once
#include <DingoEngine.h>

#include "Audio/GameAudio.h"
#include "Placing/BuildController.h"
#include "Placing/Inventory.h"
#include "Save/GameSaves.h"
#include "Checks/Autoplay.h"
#include "Checks/CheckReport.h"
#include "Checks/LifeWorldChecks.h"
#include "Checks/ReplaceChecks.h"
#include "Checks/StaticChecks.h"
#include "Checks/UiChecks.h"
#include "Farm/DemoLayout.h"
#include "Farm/EconomySim.h"
#include "Farm/FarmSim.h"
#include "Farm/GameState.h"
#include "Farm/SleepFlow.h"
#include "Life/AnimalWorld.h"
#include "Life/Player.h"
#include "Life/PlayerIntent.h"
#include "Life/VillagerDialogue.h"
#include "Life/VillagerSim.h"
#include "Life/VillagerWorld.h"
#include "Look/BuildGhost.h"
#include "Look/CameraRig.h"
#include "Look/FadeOverlay.h"
#include "Look/FarmerAssets.h"
#include "Look/NightLights.h"
#include "Look/Sky.h"
#include "Look/Vfx.h"
#include "Look/WeatherFx.h"
#include "Sim/PerfWindow.h"
#include "Ui/GameUi.h"
#include "Ui/OptionsModel.h"
#include "Ui/ScreenHost.h"
#include "ValleyDirector.h"
#include "World/ValleyWorld.h"

#include <memory>
#include <optional>

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
		std::optional<VillagerId> FindTalkTarget(const PlayerIntent& intent) const;
		void SetUpMenus();
		void LoadAtLaunch(const std::string& slotText);
		void OnGameReplaced();
		void ApplyOptions(const OptionsState& state, bool toEngine);
		const PlayerIntent& ActiveIntent() const;
		void UpdateVfxAudio(const ValleyDirector& director, float dt);
		void OnActionFeedback(ActionFeedback feedback, int x, int y);
		void PollUiSounds();
		void RecordPerf(const PerfSample& sample);
		void FinishAutoplay();

	private:
		GameState m_State;
		std::unique_ptr<ValleyDirector> m_Director;
		std::unique_ptr<FarmSim> m_Farm;
		std::unique_ptr<EconomySim> m_Economy;
		std::unique_ptr<SleepFlow> m_Sleep;
		std::unique_ptr<VillagerSim> m_Villagers;
		std::unique_ptr<AnimalSim> m_AnimalSim;
		std::unique_ptr<VillagerDialogue> m_Dialogue;
		std::unique_ptr<VillagerWorld> m_VillagerWorld;
		std::unique_ptr<AnimalWorld> m_AnimalWorld;
		WeatherFx m_WeatherFx;
		std::unique_ptr<FadeOverlay> m_Fade;
		std::unique_ptr<ValleyWorld> m_World;
		std::unique_ptr<Sky> m_Sky;
		std::unique_ptr<FarmerAssets> m_FarmerAssets;
		std::unique_ptr<Player> m_Player;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<GameUi> m_Ui;
		std::unique_ptr<Vfx> m_Vfx;
		std::unique_ptr<NightLights> m_Lights;
		std::unique_ptr<GameAudio> m_Audio;
		AudioFrame m_AudioFrame;
		uint64_t m_LightsSeenCells = ~0ull;
		glm::vec3 m_LastFeet{ 0.0f };
		bool m_HasLastFeet = false;
		bool m_ScreenWasOpen = false;
		int m_LastCoins = 0;
		int m_LastSlot = 0;
		std::unique_ptr<BuildGhost> m_Ghost;
		std::unique_ptr<BuildController> m_Build;
		std::unique_ptr<AutoplayFarmer> m_Bot;
		int m_DaysPlayed = 0;
		bool m_AutoplayDone = false;
		std::unique_ptr<PerfWindow> m_Perf;
		bool m_PerfReported = false;
		std::unique_ptr<ScreenHost> m_Screens;
		std::unique_ptr<GameSaves> m_Saves;
		bool m_LoadedAtLaunch = false;
		Settings m_SettingsBase;
		AimInput m_Aim;
		Ray m_MouseRay;
		glm::vec2 m_LastMouse{ 0.0f };
		bool m_MouseSeen = false;
		PlayerIntent m_Intent;
		PlayerIntent m_NoIntent;
		SceneManager m_SceneManager;
		Scene* m_ValleyScene = nullptr;
		CheckReport m_Report;
		std::unique_ptr<StaticCheckRun> m_StaticChecks;
		std::unique_ptr<LifeWorldCheckRun> m_LifeChecks;
		std::unique_ptr<UiCheckRun> m_UiChecks;
		std::unique_ptr<ReplaceCheckRun> m_ReplaceChecks;
		bool m_SimChecksDone = false;
		bool m_LifeChecksStarted = false;
		bool m_UiChecksStarted = false;
		bool m_ReplaceChecksStarted = false;
		bool m_ChecksDone = false;
	};

}
