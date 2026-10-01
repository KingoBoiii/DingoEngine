#include "KeepDirector.h"
#include "CameraRig.h"
#include "GameTuning.h"
#include "Hud.h"
#include "KeepWorld.h"
#include "LaunchOptions.h"
#include "Player.h"

#include <algorithm>

namespace Dingo
{

	KeepDirectorScript::KeepDirectorScript() = default;
	KeepDirectorScript::~KeepDirectorScript() = default;

	void KeepDirectorScript::OnStart()
	{
		const LaunchOptions& options = GetLaunchOptions();
		const int startRoom = std::clamp(options.Room, 1, static_cast<int>(m_Map.GetRooms().size())) - 1;
		const KeepRoom& room = m_Map.GetRooms()[startRoom];
		m_Overview = options.Overview;

		Scene& scene = GetScene();
		m_World = std::make_unique<KeepWorld>(scene, m_Map);
		if (m_Overview)
			m_World->HideSouthWalls(room.Rect);
		m_Player = std::make_unique<Player>(scene, m_Map, room.Spawn);
		m_Camera = std::make_unique<CameraRig>(scene, m_Player->GetPosition(), m_Overview ? std::optional<TileRect>(room.Rect) : std::nullopt);
		m_Hud = std::make_unique<Hud>(scene, m_Map, startRoom);
	}

	void KeepDirectorScript::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
		{
			RequestSceneTransition(SCENE_TITLE);
			return;
		}

		m_Player->Update(deltaTime);
		m_Camera->Update(deltaTime, m_Player->GetPosition());
		if (!m_Overview)
			m_World->UpdateCutaway(m_Camera->GetEye(), m_Player->GetPosition() + glm::vec3(0.0f, CUTAWAY_TARGET_HEIGHT, 0.0f));

		m_Hud->SetRoom(m_Map.RoomOf(m_Map.TileOf(m_Player->GetPosition())));
		m_Hud->Update();
	}

	void KeepDirectorScript::OnDestroy()
	{
		m_Hud.reset();
		m_Camera.reset();
		m_Player.reset();
		m_World.reset();
	}

}
