#pragma once
#include "KeepMap.h"

#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class CameraRig;
	class Detection;
	class Hud;
	class KeepWorld;
	class Lantern;
	class LightLod;
	class Player;
	class Wardens;

	class KeepDirectorScript : public ScriptableEntity
	{
	public:
		KeepDirectorScript();
		~KeepDirectorScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		// Runs detection, or the caught fade and the respawn after it. True on the respawn frame.
		bool UpdateDetection(float deltaTime);

	private:
		KeepMap m_Map;
		bool m_Overview = false;
		int m_LastRoom = 0;
		float m_CaughtTime = -1.0f;
		float m_Fade = 0.0f;

		std::unique_ptr<KeepWorld> m_World;
		std::unique_ptr<Player> m_Player;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<Lantern> m_Lantern;
		std::unique_ptr<Wardens> m_Wardens;
		std::unique_ptr<Detection> m_Detection;
		std::unique_ptr<LightLod> m_LightLod;
		std::unique_ptr<Hud> m_Hud;
	};

}
