#include "ArenaDirector.h"
#include "ArenaWorld.h"
#include "Audio.h"
#include "CameraRig.h"
#include "CheckReport.h"
#include "DriveBrain.h"
#include "Fighter.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Moveset.h"
#include "PlayerBrain.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	ArenaDirectorScript::ArenaDirectorScript(const GameAssets* assets)
		: m_Assets(assets)
	{}

	ArenaDirectorScript::~ArenaDirectorScript() = default;

	void ArenaDirectorScript::OnStart()
	{
		const LaunchOptions& options = GetLaunchOptions();
		Scene& scene = GetScene();

		m_World = std::make_unique<ArenaWorld>(scene);
		m_Audio = std::make_unique<GameAudio>(m_Assets->GetFootstep());
		m_Freeze = options.Freeze;
		m_Drive = options.Freeze ? DriveMode::None : options.Drive;

		const FighterContext context{ scene, *m_Assets, *m_Audio, m_Time, options.Check || m_Drive != DriveMode::None };
		if (options.Lineup)
			BuildLineup(context, options);
		else
			BuildBout(context, options);
	}

	void ArenaDirectorScript::BuildLineup(const FighterContext& context, const LaunchOptions& options)
	{
		Scene& scene = GetScene();
		const std::span<const FighterDef> defs = GetFighterDefs();
		const float first = -0.5f * LINEUP_SPACING * static_cast<float>(defs.size() - 1);
		float tallest = 0.0f;
		for (size_t i = 0; i < defs.size(); ++i)
		{
			FighterSpawn spawn;
			spawn.Position = glm::vec3(first + LINEUP_SPACING * static_cast<float>(i), 0.0f, 0.0f);
			spawn.Controlled = false;
			auto fighter = std::make_unique<Fighter>(context, defs[i], spawn);
			fighter->ShowIdle(options.Freeze ? FREEZE_POSE_TIME : LINEUP_IDLE_STAGGER * static_cast<float>(i), options.Freeze);
			tallest = std::max(tallest, fighter->GetModelHeight() * defs[i].Scale);
			m_Fighters.push_back(std::move(fighter));
		}

		if (options.Overview)
		{
			m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f), OVERVIEW_PITCH_DEG, m_World->GetRimPoints());
		}
		else
		{
			const float half = std::abs(first) + LINEUP_MARGIN;
			const float top = std::max(tallest, 0.5f * LINEUP_FIT_HEIGHT);
			std::vector<glm::vec3> fit;
			for (const float x : { -half, half })
				for (const float y : { 0.0f, top })
					fit.emplace_back(x, y, 0.0f);
			m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f, LINEUP_LOOK_HEIGHT, 0.0f), LINEUP_PITCH_DEG, std::move(fit));
		}

		DE_INFO("Marionette: lineup of {} fighters{}{}", defs.size(), options.Freeze ? ", frozen" : "", options.Overview ? ", overview camera" : "");
	}

	void ArenaDirectorScript::BuildBout(const FighterContext& context, const LaunchOptions& options)
	{
		Scene& scene = GetScene();
		const BoutLayout layout = GetBoutLayout(m_Drive);
		const FighterDef& playerDef = GetPlayerDef();
		const FighterDef& opponentDef = GetOpponentDef(options.Bout);

		FighterSpawn playerSpawn;
		playerSpawn.Position = layout.PlayerPosition;
		playerSpawn.YawDegrees = layout.PlayerYawDegrees;
		FighterSpawn opponentSpawn;
		opponentSpawn.Position = layout.OpponentPosition;
		opponentSpawn.YawDegrees = layout.OpponentYawDegrees;
		m_Fighters.push_back(std::make_unique<Fighter>(context, playerDef, playerSpawn));
		m_Fighters.push_back(std::make_unique<Fighter>(context, opponentDef, opponentSpawn));

		Fighter& player = *m_Fighters[0];
		Fighter& opponent = *m_Fighters[1];
		if (m_Freeze)
		{
			player.Freeze(std::max(options.Move, 0.0f), options.Phase);
			opponent.ShowIdle(FREEZE_POSE_TIME, true);
		}
		else
		{
			player.StartLocomotion(0.0f);
			opponent.StartLocomotion(OPPONENT_IDLE_PHASE);
		}

		if (m_Drive != DriveMode::None)
			m_Brain = std::make_unique<DriveBrain>(m_Drive);
		else
			m_Brain = std::make_unique<PlayerBrain>();

		if (options.Overview)
			m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f), OVERVIEW_PITCH_DEG, m_World->GetRimPoints());
		else
			m_FollowCamera = std::make_unique<FollowCamera>(scene);

		for (const Fighter* fighter : { &player, &opponent })
		{
			DE_INFO("Marionette: {} is {:.2f} m tall, capsule radius {:.2f} m, speed factor {:.2f} (walks {:.2f}, runs {:.2f} m/s)",
				fighter->GetDef().Name, fighter->GetModelHeight() * fighter->GetDef().Scale, fighter->GetRadius(), fighter->GetSpeedFactor(),
				WALK_SPEED * fighter->GetSpeedFactor(), RUN_SPEED * fighter->GetSpeedFactor());
		}
		DE_INFO("Marionette: bout against {}{}{}{}", opponentDef.Name, m_Freeze ? ", frozen" : "",
			m_Drive != DriveMode::None ? ", drive " : "", m_Drive != DriveMode::None ? ToString(m_Drive) : "");

		m_Bout = true;
		UpdateBout(0.0f);
	}

	void ArenaDirectorScript::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape) || Input::IsGamepadButtonPressed(GamepadButton::Start))
			RequestSceneTransition(SCENE_TITLE);
#ifdef DE_DEBUG
		else if (Input::IsKeyPressed(Key::End))
			RequestSceneTransition(SCENE_END);
#endif

		m_Time += deltaTime;
		if (m_Bout)
			UpdateBout(deltaTime);

		if (m_Camera)
			m_Camera->Update();
	}

	void ArenaDirectorScript::UpdateBout(float deltaTime)
	{
		Fighter& player = *m_Fighters[0];
		Fighter& opponent = *m_Fighters[1];

		if (!m_Freeze)
			player.SetIntent(m_Brain->Think(deltaTime, player, opponent));

		player.Think(deltaTime, &opponent);
		opponent.Think(deltaTime, &player);
		Fighter::Separate(player, opponent, deltaTime);
		player.Apply(deltaTime);
		opponent.Apply(deltaTime);

		if (m_FollowCamera)
			m_FollowCamera->Update(deltaTime, player.GetPosition(), opponent.GetPosition(), m_Freeze);

		if (m_Drive != DriveMode::None)
			LogDrive(deltaTime);
		if (m_Drive == DriveMode::Wall)
			CheckWall(deltaTime);
	}

	void ArenaDirectorScript::CheckWall(float deltaTime)
	{
		if (m_WallChecked)
			return;

		const Fighter& player = *m_Fighters[0];
		const bool pressed = player.GetGroundPosition().x >= GetArenaApothem() - player.GetRadius() - DRIVE_WALL_CONTACT_SLACK;
		m_WallPressed = pressed ? m_WallPressed + deltaTime : 0.0f;

		if (m_WallPressed >= DRIVE_WALL_HOLD)
		{
			const float quiet = static_cast<float>(m_Time - player.GetLastStepTime());
			CheckReport().Check(player.GetMoveParameter() < DRIVE_WALL_MOVE_LIMIT && quiet > DRIVE_WALL_QUIET,
				std::format("a fighter stopped by a wall stops its feet (Move {:.2f}, limit {:.1f}; last step {:.2f} s ago, none allowed within {:.1f} s)",
					player.GetMoveParameter(), DRIVE_WALL_MOVE_LIMIT, quiet, DRIVE_WALL_QUIET));
			m_WallChecked = true;
		}
		else if (m_Time > DRIVE_WALL_DEADLINE)
		{
			CheckReport().Check(false, std::format("the fighter reaches the wall within {:.0f} s", DRIVE_WALL_DEADLINE));
			m_WallChecked = true;
		}
	}

	void ArenaDirectorScript::LogDrive(float deltaTime)
	{
		m_LogTimer += deltaTime;
		if (m_LogTimer < DRIVE_LOG_INTERVAL)
			return;
		m_LogTimer -= DRIVE_LOG_INTERVAL;

		const Fighter& player = *m_Fighters[0];
		const Fighter& opponent = *m_Fighters[1];
		const glm::vec2 position = player.GetGroundPosition();
		DE_INFO("[Move] t={:.2f} pos=({:.2f}, {:.2f}) speed={:.2f} param={:.2f} zone={} yaw={:.1f} gap={:.2f}", m_Time, position.x, position.y,
			player.GetGroundSpeed(), player.GetMoveParameter(), ToString(player.GetZone()), glm::degrees(player.GetYaw()),
			glm::length(opponent.GetGroundPosition() - position));
	}

	void ArenaDirectorScript::OnDestroy()
	{
		m_Camera.reset();
		m_FollowCamera.reset();
		m_Brain.reset();
		m_Fighters.clear();
		m_Audio.reset();
		m_World.reset();
	}

}
