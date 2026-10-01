#include "KeepDirector.h"
#include "CameraRig.h"
#include "GameTuning.h"
#include "Hud.h"
#include "KeepWorld.h"
#include "Lantern.h"
#include "LaunchOptions.h"
#include "LightLod.h"
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
		m_Lantern = std::make_unique<Lantern>(scene, *m_Player, m_World->GetBrassMaterial(), options.Oil >= 0 ? static_cast<float>(options.Oil) : OIL_MAX, !options.Freeze);

		m_LightLod = std::make_unique<LightLod>(m_World->GetFlames(), !options.NoLightLod);
		m_LightLod->AddGameplayLight(m_Lantern->GetLight());
		for (const BrazierSpot& brazier : m_World->GetBraziers())
			m_LightLod->AddGameplayLight(brazier.Light);
		m_LightLod->Update(0.0f, m_Camera->GetFocus(), m_Camera->GetViewProjection());

		m_Hud = std::make_unique<Hud>(scene, m_Map, *m_Lantern, startRoom);
	}

	void KeepDirectorScript::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
		{
			RequestSceneTransition(SCENE_TITLE);
			return;
		}

		m_Player->Update(deltaTime);

		if (const size_t flasks = m_World->CollectFlasks(m_Player->GetPosition(), m_Lantern->GetFlaskCapacity()))
		{
			m_Lantern->AddOil(static_cast<float>(flasks) * OIL_PER_FLASK);
			DE_INFO("Candlewick: picked up {} oil flask(s), oil now {:.1f}", flasks, m_Lantern->GetOil());
		}

		m_Lantern->Update(deltaTime, *m_Player);

		m_Camera->Update(deltaTime, m_Player->GetPosition());
		m_LightLod->Update(deltaTime, m_Camera->GetFocus(), m_Camera->GetViewProjection());

		if (!m_Overview)
			m_World->UpdateCutaway(m_Camera->GetEye(), m_Player->GetPosition() + glm::vec3(0.0f, CUTAWAY_TARGET_HEIGHT, 0.0f));

		m_Hud->SetRoom(m_Map.RoomOf(m_Map.TileOf(m_Player->GetPosition())));
		m_Hud->Update(*m_Lantern);
	}

	void KeepDirectorScript::OnDestroy()
	{
		m_Hud.reset();
		m_LightLod.reset();
		m_Lantern.reset();
		m_Camera.reset();
		m_Player.reset();
		m_World.reset();
	}

}
