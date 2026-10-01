#include "KeepDirector.h"
#include "CameraRig.h"
#include "Detection.h"
#include "GameTuning.h"
#include "Hud.h"
#include "KeepWorld.h"
#include "Lantern.h"
#include "LaunchOptions.h"
#include "LightLod.h"
#include "Player.h"
#include "Wardens.h"

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

		glm::ivec2 spawnTile = room.Spawn;
		if (options.Spawn)
		{
			if (m_Map.IsWalkable(*options.Spawn))
				spawnTile = *options.Spawn;
			else
				DE_WARN("Candlewick: ignoring --spawn={},{} (not a walkable tile)", options.Spawn->x, options.Spawn->y);
		}
		m_LastRoom = m_Map.RoomOf(spawnTile) >= 0 ? m_Map.RoomOf(spawnTile) : startRoom;

		Scene& scene = GetScene();
		m_World = std::make_unique<KeepWorld>(scene, m_Map);
		if (m_Overview)
			m_World->HideSouthWalls(room.Rect);
		m_Player = std::make_unique<Player>(scene, m_Map, spawnTile);
		m_Camera = std::make_unique<CameraRig>(scene, m_Player->GetPosition(), m_Overview ? std::optional<TileRect>(room.Rect) : std::nullopt);
		m_Lantern = std::make_unique<Lantern>(scene, *m_Player, m_World->GetBrassMaterial(), options.Oil >= 0 ? static_cast<float>(options.Oil) : OIL_MAX, !options.Freeze);
		m_Wardens = std::make_unique<Wardens>(scene, m_Map, options.Freeze, !options.NoRangeClamp);
		m_Detection = std::make_unique<Detection>(scene, m_Map, *m_World, m_Wardens->GetCount(), options.DebugCone, !options.Freeze);

		m_LightLod = std::make_unique<LightLod>(m_World->GetFlames(), !options.NoLightLod);
		m_LightLod->AddGameplayLight(m_Lantern->GetLight());
		for (const BrazierSpot& brazier : m_World->GetBraziers())
			m_LightLod->AddGameplayLight(brazier.Light);
		for (size_t i = 0; i < m_Wardens->GetCount(); ++i)
		{
			m_LightLod->AddGameplayLight(m_Wardens->GetLamp(i));
			m_LightLod->AddGameplayLight(m_Wardens->GetEye(i));
		}
		m_LightLod->Update(0.0f, m_Camera->GetFocus(), m_Camera->GetViewProjection());

		m_Hud = std::make_unique<Hud>(scene, m_Map, *m_Lantern, m_LastRoom);
	}

	void KeepDirectorScript::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
		{
			RequestSceneTransition(SCENE_TITLE);
			return;
		}

		m_Player->Update(deltaTime);
		const int room = m_Map.RoomOf(m_Map.TileOf(m_Player->GetPosition()));
		if (room >= 0)
			m_LastRoom = room;

		if (const size_t flasks = m_World->CollectFlasks(m_Player->GetPosition(), m_Lantern->GetFlaskCapacity()))
		{
			m_Lantern->AddOil(static_cast<float>(flasks) * OIL_PER_FLASK);
			DE_INFO("Candlewick: picked up {} oil flask(s), oil now {:.1f}", flasks, m_Lantern->GetOil());
		}

		m_Lantern->Update(deltaTime, *m_Player);

		m_Wardens->Update(deltaTime);
		const bool respawned = UpdateDetection(deltaTime);

		m_Camera->Update(respawned ? 0.0f : deltaTime, m_Player->GetPosition());
		m_LightLod->Update(deltaTime, m_Camera->GetFocus(), m_Camera->GetViewProjection());

		if (!m_Overview)
			m_World->UpdateCutaway(m_Camera->GetEye(), m_Player->GetPosition() + glm::vec3(0.0f, CUTAWAY_TARGET_HEIGHT, 0.0f));

		m_Hud->SetRoom(m_Map.RoomOf(m_Map.TileOf(m_Player->GetPosition())));
		m_Hud->SetFade(m_Fade);
		m_Hud->Update(*m_Lantern);
	}

	bool KeepDirectorScript::UpdateDetection(float deltaTime)
	{
		if (m_CaughtTime < 0.0f)
		{
			m_Fade = std::max(0.0f, m_Fade - deltaTime / RESPAWN_FADE_TIME);
			if (const std::optional<size_t> warden = m_Detection->Update(deltaTime, *m_Wardens, *m_Player, *m_Lantern))
			{
				m_CaughtTime = 0.0f;
				m_Player->Halt();
				DE_INFO("Candlewick: caught by warden {} in {}", *warden + 1, m_Map.GetRooms()[m_Wardens->GetRoom(*warden)].Name);
			}
			return false;
		}

		// After the Lantern, whose own strike lock would otherwise overwrite this one.
		m_Player->SetMovementLocked(true);
		m_CaughtTime += deltaTime;
		m_Fade = std::min(1.0f, m_CaughtTime / CAUGHT_FADE_TIME);
		if (m_CaughtTime < CAUGHT_FADE_TIME)
		{
			m_Detection->UpdateDebugView(*m_Wardens, *m_Player);
			return false;
		}

		const KeepRoom& room = m_Map.GetRooms()[m_LastRoom];
		m_Player->Teleport(m_Map.TileCenter(room.Spawn) + glm::vec3(0.0f, PLAYER_SPAWN_LIFT, 0.0f));
		m_Wardens->Reset();
		m_Detection->UpdateDebugView(*m_Wardens, *m_Player);
		// Until a checkpoint refills it: a catch at 0 oil must not leave the lantern unable to relight.
		m_Lantern->AddOil(std::max(0.0f, RESPAWN_MIN_OIL - m_Lantern->GetOil()));
		m_CaughtTime = -1.0f;
		DE_INFO("Candlewick: respawned at {} ({}, {})", room.Name, room.Spawn.x, room.Spawn.y);
		return true;
	}

	void KeepDirectorScript::OnDestroy()
	{
		m_Hud.reset();
		m_LightLod.reset();
		m_Detection.reset();
		m_Wardens.reset();
		m_Lantern.reset();
		m_Camera.reset();
		m_Player.reset();
		m_World.reset();
	}

}
