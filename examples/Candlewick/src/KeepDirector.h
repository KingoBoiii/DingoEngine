#pragma once
#include "KeepMap.h"

#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class CameraRig;
	class Hud;
	class KeepWorld;
	class Player;

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
		KeepMap m_Map;
		bool m_Overview = false;

		std::unique_ptr<KeepWorld> m_World;
		std::unique_ptr<Player> m_Player;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<Hud> m_Hud;
	};

}
