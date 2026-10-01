#include "KeepDirector.h"
#include "Audio.h"
#include "Braziers.h"
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

	KeepDirectorScript::KeepDirectorScript(RunResult* result)
		: m_Result(result)
	{}

	KeepDirectorScript::~KeepDirectorScript() = default;

	void KeepDirectorScript::OnStart()
	{
		const LaunchOptions& options = GetLaunchOptions();
		const int startRoom = std::clamp(options.Room, 1, static_cast<int>(m_Map.GetRooms().size())) - 1;
		const KeepRoom& room = m_Map.GetRooms()[startRoom];
		m_Overview = options.Overview;

		glm::ivec2 spawnTile = room.Spawn;
		std::optional<glm::ivec2> checkpointTile;
		if (options.Room > 1)
			checkpointTile = spawnTile;
		if (options.Spawn)
		{
			if (m_Map.IsWalkable(*options.Spawn))
			{
				spawnTile = *options.Spawn;
				checkpointTile = spawnTile;
			}
			else
			{
				DE_WARN("Candlewick: ignoring --spawn={},{} (not a walkable tile)", options.Spawn->x, options.Spawn->y);
			}
		}
		const int labelRoom = m_Map.RoomOf(spawnTile) >= 0 ? m_Map.RoomOf(spawnTile) : startRoom;
		const float startOil = options.Oil >= 0 ? static_cast<float>(options.Oil) : OIL_MAX;

		Scene& scene = GetScene();
		m_Audio = std::make_unique<GameAudio>();
		m_World = std::make_unique<KeepWorld>(scene, m_Map);
		if (m_Overview)
			m_World->HideSouthWalls(room.Rect);
		m_Player = std::make_unique<Player>(scene, m_Map, spawnTile);

		m_Listener = scene.CreateEntity("AudioListener");
		m_Listener.AddComponent<Transform3DComponent>();
		m_Listener.AddComponent<AudioListenerComponent>();
		UpdateListener();
		Application::Get().GetAudioEngine().SetListenerPosition(m_Listener.GetComponent<Transform3DComponent>().Position);

		m_Camera = std::make_unique<CameraRig>(scene, m_Player->GetPosition(), m_Overview ? std::optional<TileRect>(room.Rect) : std::nullopt);
		m_Lantern = std::make_unique<Lantern>(scene, *m_Player, m_World->GetBrassMaterial(), *m_Audio, startOil, !options.Freeze);
		m_Wardens = std::make_unique<Wardens>(scene, m_Map, *m_Audio, options.Freeze, !options.NoRangeClamp);
		m_Detection = std::make_unique<Detection>(scene, *m_World, m_Wardens->GetCount(), options.DebugCone, !options.Freeze);

		m_LightLod = std::make_unique<LightLod>(m_World->GetFlames(), !options.NoLightLod);
		m_LightLod->AddGameplayLight(m_Lantern->GetLight());
		for (size_t i = 0; i < m_Wardens->GetCount(); ++i)
		{
			m_LightLod->AddGameplayLight(m_Wardens->GetLamp(i));
			m_LightLod->AddGameplayLight(m_Wardens->GetEye(i));
		}

		m_Braziers = std::make_unique<Braziers>(scene, m_Map, *m_World, *m_LightLod, *m_Audio, startOil, checkpointTile, !options.Freeze, options.AllLit);
		m_LightLod->Update(0.0f, m_Camera->GetFocus(), m_Camera->GetViewProjection());

		m_Hud = std::make_unique<Hud>(scene, m_Map, *m_Lantern, labelRoom);
		m_Audio->StartDrone();
	}

	void KeepDirectorScript::SetPaused(bool paused)
	{
		m_Paused = paused;
		if (paused)
			m_Player->Halt();
		m_Audio->SetMuted(paused);
		m_Hud->SetPaused(paused);
		DE_INFO("Candlewick: {}", paused ? "paused" : "resumed");
	}

	void KeepDirectorScript::OnUpdate(float deltaTime)
	{
		const bool toggle = Input::IsKeyPressed(Key::Escape) || Input::IsGamepadButtonPressed(GamepadButton::Start);
		if (toggle && m_WinTime < 0.0f)
			SetPaused(!m_Paused);

		if (m_Paused)
		{
			if (Input::IsKeyPressed(Key::Enter) || Input::IsGamepadButtonPressed(GamepadButton::B))
				RequestSceneTransition(SCENE_TITLE);
			m_Hud->Update(0.0f, *m_Lantern);
			return;
		}

		if (m_WinTime < 0.0f)
			m_Seconds += m_Result->FrameSeconds;

		m_Player->Update(deltaTime);
		UpdateListener();

		if (const size_t flasks = m_World->CollectFlasks(m_Player->GetPosition(), m_Lantern->GetFlaskCapacity()))
		{
			m_Lantern->AddOil(static_cast<float>(flasks) * OIL_PER_FLASK);
			m_Audio->PlayAt(Sfx::Flask, m_Player->GetPosition());
			DE_INFO("Candlewick: picked up {} oil flask(s), oil now {:.1f}", flasks, m_Lantern->GetOil());
		}

		m_Lantern->Update(deltaTime, *m_Player);

		bool respawned = false;
		if (m_WinTime >= 0.0f)
		{
			UpdateWin(deltaTime);
		}
		else
		{
			m_Wardens->Update(deltaTime);
			respawned = UpdateDetection(deltaTime);
			UpdateFootsteps(deltaTime);
		}

		const bool canLight = m_CaughtTime < 0.0f && m_WinTime < 0.0f;
		const Braziers::Outcome braziers = m_Braziers->Update(deltaTime, *m_Player, *m_Lantern, canLight);
		if (braziers.Lit)
		{
			if (m_World->GetBraziers()[*braziers.Lit].IsAltar)
				BeginWin();
			else
				m_Hud->ShowToast("Checkpoint");
		}
		else if (braziers.Refuelled)
		{
			m_Hud->ShowToast("Lantern refilled");
		}

		m_Camera->Update(respawned ? 0.0f : deltaTime, m_Player->GetPosition());
		m_LightLod->Update(deltaTime, m_Camera->GetFocus(), m_Camera->GetViewProjection());

		if (!m_Overview)
			m_World->UpdateCutaway(m_Camera->GetEye(), m_Player->GetPosition() + glm::vec3(0.0f, CUTAWAY_TARGET_HEIGHT, 0.0f));

		m_Hud->SetRoom(m_Map.RoomOf(m_Map.TileOf(m_Player->GetPosition())));
		m_Hud->SetFade(m_Fade);
		m_Hud->SetBrazierPrompt(m_Braziers->GetPrompt(), m_Braziers->GetProgress());
		m_Hud->Update(deltaTime, *m_Lantern);
	}

	void KeepDirectorScript::UpdateListener()
	{
		const glm::vec3 ears = m_Player->GetPosition() + glm::vec3(0.0f, LISTENER_HEIGHT, 0.0f);
		m_Listener.GetComponent<Transform3DComponent>().Position = ears;
		m_Audio->SetListener(ears);
	}

	void KeepDirectorScript::UpdateFootsteps(float deltaTime)
	{
		if (m_CaughtTime >= 0.0f || !m_Player->IsMoving())
		{
			m_StepTimer = 0.0f;
			return;
		}

		m_StepTimer -= deltaTime;
		if (m_StepTimer <= 0.0f)
		{
			m_Audio->PlayAt(Sfx::Footstep, m_Player->GetPosition());
			m_StepTimer = FOOTSTEP_INTERVAL;
		}
	}

	bool KeepDirectorScript::UpdateDetection(float deltaTime)
	{
		if (m_CaughtTime < 0.0f)
		{
			m_Fade = std::max(0.0f, m_Fade - deltaTime / RESPAWN_FADE_TIME);
			if (m_Grace > 0.0f)
			{
				m_Grace = std::max(0.0f, m_Grace - deltaTime);
				m_Detection->UpdateDebugView(*m_Wardens, *m_Player);
				return false;
			}

			if (const std::optional<size_t> warden = m_Detection->Update(deltaTime, *m_Wardens, *m_Player, *m_Lantern))
			{
				m_CaughtTime = 0.0f;
				m_Player->Halt();
				++m_Catches;
				m_Audio->Play(Sfx::Caught);
				DE_INFO("Candlewick: caught by warden {} in {} (catch {}, oil {:.0f})", *warden + 1, m_Map.GetRooms()[m_Wardens->GetRoom(*warden)].Name, m_Catches, m_Lantern->GetOil());
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

		const Braziers::Checkpoint& checkpoint = m_Braziers->GetCheckpoint();
		m_Player->Teleport(m_Map.TileCenter(checkpoint.Tile) + glm::vec3(0.0f, PLAYER_SPAWN_LIFT, 0.0f));
		m_Wardens->Reset();
		m_Detection->UpdateDebugView(*m_Wardens, *m_Player);
		m_Lantern->SetOil(checkpoint.Oil);
		m_CaughtTime = -1.0f;
		m_Grace = RESPAWN_GRACE_TIME;
		DE_INFO("Candlewick: respawned at the checkpoint ({}, {}) with oil {:.0f}, unseen for {:.1f} s", checkpoint.Tile.x, checkpoint.Tile.y, m_Lantern->GetOil(), m_Grace);
		return true;
	}

	void KeepDirectorScript::BeginWin()
	{
		m_WinTime = 0.0f;
		m_Player->Halt();
		m_Audio->Play(Sfx::Win);

		m_Result->Seconds = m_Seconds;
		m_Result->Catches = m_Catches;
		DE_INFO("Candlewick: the altar burns after {:.1f} s, caught {} time(s)", m_Seconds, m_Catches);
	}

	void KeepDirectorScript::UpdateWin(float deltaTime)
	{
		m_Player->SetMovementLocked(true);
		m_WinTime += deltaTime;
		m_Fade = std::clamp((m_WinTime - WIN_LINGER_TIME) / WIN_FADE_TIME, 0.0f, 1.0f);
		if (m_WinTime >= WIN_LINGER_TIME + WIN_FADE_TIME)
			RequestSceneTransition(SCENE_END);
	}

	void KeepDirectorScript::OnDestroy()
	{
		m_Hud.reset();
		m_Braziers.reset();
		m_LightLod.reset();
		m_Detection.reset();
		m_Wardens.reset();
		m_Lantern.reset();
		m_Camera.reset();
		m_Player.reset();
		m_World.reset();
		m_Audio.reset();
	}

}
