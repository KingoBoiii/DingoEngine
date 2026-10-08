#pragma once
#include "KeepMap.h"
#include "RunResult.h"

#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class Braziers;
	class CameraRig;
	class Detection;
	class GameAudio;
	class Hud;
	class KeepWorld;
	class Lantern;
	class LightLod;
	class Player;
	class Wardens;

	class KeepDirectorScript : public ScriptableEntity
	{
	public:
		explicit KeepDirectorScript(RunResult* result);
		~KeepDirectorScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void SetPaused(bool paused);
		// The listener rides on the player, not on the high camera, so positional sound centres on the player.
		void UpdateListener();
		void UpdateFootsteps(float deltaTime);
		// Runs detection, or the caught fade and the respawn after it. True on the respawn frame.
		bool UpdateDetection(float deltaTime);
		void BeginWin();
		void UpdateWin(float deltaTime);

	private:
		RunResult* m_Result = nullptr;
		KeepMap m_Map;
		bool m_Overview = false;
		bool m_Paused = false;
		float m_CaughtTime = -1.0f;
		float m_Grace = 0.0f;
		float m_WinTime = -1.0f;
		float m_Fade = 0.0f;
		float m_Seconds = 0.0f;
		int m_Catches = 0;
		float m_StepTimer = 0.0f;
		int m_Frames = 0;

		std::unique_ptr<GameAudio> m_Audio;
		std::unique_ptr<KeepWorld> m_World;
		std::unique_ptr<Player> m_Player;
		Entity m_Listener;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<Lantern> m_Lantern;
		std::unique_ptr<Wardens> m_Wardens;
		std::unique_ptr<Detection> m_Detection;
		std::unique_ptr<LightLod> m_LightLod;
		std::unique_ptr<Braziers> m_Braziers;
		std::unique_ptr<Hud> m_Hud;
	};

}
