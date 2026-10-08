#include "ArenaDirector.h"
#include "AiBrain.h"
#include "ArenaVfx.h"
#include "ArenaWorld.h"
#include "Audio.h"
#include "BoutFlow.h"
#include "CameraRig.h"
#include "CheckReport.h"
#include "CheckTuning.h"
#include "Combat.h"
#include "DriveBrain.h"
#include "DuelScript.h"
#include "Fighter.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Hud.h"
#include "MatchState.h"
#include "Moveset.h"
#include "Overlay.h"
#include "PlayerBrain.h"
#include "ReachTable.h"
#include "Showcase.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Dingo
{

	ArenaDirectorScript::ArenaDirectorScript(const GameAssets* assets, const ReachTable* reach, MatchState* match)
		: m_Assets(assets), m_Reach(reach), m_Match(match)
	{}

	ArenaDirectorScript::~ArenaDirectorScript() = default;

	void ArenaDirectorScript::OnStart()
	{
		const LaunchOptions& options = GetLaunchOptions();
		Scene& scene = GetScene();

		if (options.Lineup)
		{
			ShowcaseParams params;
			params.Kind = ShowcaseKind::Lineup;
			params.Freeze = options.Freeze;
			params.Overview = options.Overview;
			params.Debug = m_Assets->GetDebugView();
			m_Showcase = std::make_unique<Showcase>(scene, *m_Assets, params);
			return;
		}

		if (!m_Assets->IsPlayable())
		{
			ShowMissingAssets();
			return;
		}

		m_Audio = std::make_unique<GameAudio>(m_Assets->GetSounds());
		m_Audio->SetMuted(options.StepsPerFrame > 1 || options.Tournament > 0);
		if (!options.NoParticles)
			m_Vfx = std::make_unique<ArenaVfx>(scene);
		m_World = std::make_unique<ArenaWorld>(scene, *m_Assets, m_Audio.get(), m_Vfx.get());
		m_EventGeneration = m_Assets->GetEventGeneration();
		m_Freeze = options.Freeze;
		m_Tournament = options.Tournament > 0;
		m_Drive = options.Freeze ? DriveMode::None : options.Drive;
		m_BoutNumber = std::clamp(m_Match->Bout, 1, BOUT_COUNT);
		// A tournament run takes the next seed; a retry takes one too, or a fixed-delta autoplay would replay the same loss.
		m_Seed = options.Seed + (m_Tournament ? static_cast<uint32_t>(m_Match->Records.size()) : static_cast<uint32_t>(m_Match->Retries));

		const bool logSteps = options.Check || (m_Drive != DriveMode::None && m_Drive != DriveMode::Duel);
		const bool logCombat = options.Check || options.DebugHitbox || m_Drive != DriveMode::None || (options.Autoplay && !m_Tournament);
		const FighterContext context{ scene, *m_Assets, *m_Audio, m_Time, logSteps, logCombat, m_Assets->GetDebugView(), m_Vfx.get() };
		BuildBout(context, options);
	}

	void ArenaDirectorScript::ShowMissingAssets()
	{
		Scene& scene = GetScene();
		Font* font = m_Assets->GetFont();
		Overlay::MakeCamera(scene, "MissingCamera", HUD_ORTHO_SIZE);
		Overlay::MakeText(scene, font, "MissingHeading", MISSING_HEADING_SIZE, COLOR_TEXT_ALERT, { 0.0f, MISSING_HEADING_Y, 0.0f }, "assets missing (see log)");
		Overlay::MakeText(scene, font, "MissingPrompt", MISSING_PROMPT_SIZE, COLOR_TEXT, { 0.0f, MISSING_PROMPT_Y, 0.0f }, "press any key / button to return to the title");
		m_AssetsMissing = true;
	}

	void ArenaDirectorScript::BuildBout(const FighterContext& context, const LaunchOptions& options)
	{
		Scene& scene = GetScene();
		const bool poseFrame = m_Freeze && !options.PoseClip.empty();
		const bool driven = m_Drive != DriveMode::None;
		// A bout proper: brains, the bout's phases and the HUD. A drive or a pose frame stays the bare arena.
		const bool playing = !driven && !poseFrame;

		BoutLayout layout = GetBoutLayout(m_Drive);
		if (poseFrame)
			layout.OpponentPosition.x = layout.PlayerPosition.x + POSE_DISTANCE;
		const FighterDef& playerDef = GetPlayerDef();
		const FighterDef& opponentDef = GetOpponentDef(m_BoutNumber);

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
			const AnimationClip* pose = poseFrame ? m_Assets->FindAnyClip(options.PoseClip) : nullptr;
			if (poseFrame && !pose)
			{
				DE_ERROR("Marionette: --pose clip '{}' is in none of the libraries", options.PoseClip);
			}

			if (pose)
				player.FreezePose(*pose, options.PoseTime);
			else
				player.Freeze(std::max(options.Move, 0.0f), options.Phase);

			const AnimationClip* taunt = playing && opponentDef.Intro ? m_Assets->GetClip(opponentDef.Intro) : nullptr;
			if (taunt)
				opponent.FreezePose(*taunt, INTRO_FREEZE_TIME);
			else
				opponent.ShowIdle(FREEZE_POSE_TIME, true);
		}
		else
		{
			player.StartLocomotion(0.0f);
			opponent.StartLocomotion(OPPONENT_IDLE_PHASE);
		}

		m_Combat = std::make_unique<Combat>(*m_Audio, context.LogCombat, m_Vfx.get());
		if (m_Drive == DriveMode::Duel)
			m_Duel = std::make_unique<DuelScript>(options.BreakHitbox);
		else if (driven)
			m_Brain = std::make_unique<DriveBrain>(m_Drive);
		else if (options.Autoplay)
			m_Brain = std::make_unique<AiBrain>(AI_TIERS[static_cast<size_t>(options.PlayerTier) - 1], m_Seed + static_cast<uint32_t>(m_BoutNumber) + AI_PLAYER_SEED_OFFSET, m_Reach);
		else
			m_Brain = std::make_unique<PlayerBrain>();

		if (playing)
		{
			m_OpponentBrain = std::make_unique<AiBrain>(AI_TIERS[static_cast<size_t>(m_BoutNumber) - 1], m_Seed + static_cast<uint32_t>(m_BoutNumber), m_Reach);

			BoutRules rules;
			rules.Frozen = m_Freeze;
			if (m_Tournament)
			{
				rules.IntroSeconds = 0.0f;
				rules.KnockoutSeconds = 0.0f;
				rules.Taunts = false;
				rules.TimeLimit = TOURNAMENT_BOUT_LIMIT;
			}
			m_Flow = std::make_unique<BoutFlow>(rules, m_Audio.get());
			m_Flow->Begin(opponent);

			m_Hud = std::make_unique<Hud>(scene, *m_Assets, m_BoutNumber, !m_Freeze && !m_Tournament, !options.Autoplay && !m_Freeze && m_BoutNumber == 1);

			if (!m_Tournament && !m_Freeze && m_Reach)
			{
				m_Reach->Log(playerDef, opponentDef);
				m_Reach->Log(opponentDef, playerDef);
			}
		}

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
		if (!m_Tournament)
		{
			DE_INFO("Marionette: bout {} against {}{}{}{}{}", m_BoutNumber, opponentDef.Name, m_Freeze ? ", frozen" : "",
				driven ? ", drive " : "", driven ? ToString(m_Drive) : "", options.Autoplay ? ", autoplay" : "");
		}

		m_Bout = true;
		UpdateBout(0.0f);
	}

	void ArenaDirectorScript::OnUpdate(float deltaTime)
	{
		if (m_Match->Done)
			return;

		if (m_AssetsMissing)
		{
			if (IsScripted(GetLaunchOptions()))
				Application::Get().Close();
			else if (Overlay::AnyInputPressed())
				RequestSceneTransition(SCENE_TITLE);
			return;
		}

		if (Input::IsKeyPressed(Key::Escape) || Input::IsGamepadButtonPressed(GamepadButton::Start))
			RequestSceneTransition(SCENE_TITLE);
#ifdef DE_DEBUG
		else if (Input::IsKeyPressed(Key::End))
			RequestSceneTransition(SCENE_END);
#endif

		m_Time += deltaTime;
		if (m_Showcase)
			m_Showcase->Update(deltaTime);
		if (m_Bout)
			UpdateBout(deltaTime);

		if (m_Camera)
			m_Camera->Update();
	}

	void ArenaDirectorScript::OnEventsChanged()
	{
		m_EventGeneration = m_Assets->GetEventGeneration();
		for (const std::unique_ptr<Fighter>& fighter : m_Fighters)
			fighter->OnEventsChanged();
		if (m_Brain)
			m_Brain->OnEventsChanged();
		if (m_OpponentBrain)
			m_OpponentBrain->OnEventsChanged();

		// The reach table was emptied by the reload; asking it again rebuilds the rows this pair uses.
		if (m_Hud && m_Reach && !m_Tournament && !m_Freeze)
		{
			const FighterDef& opponentDef = GetOpponentDef(m_BoutNumber);
			m_Reach->Log(GetPlayerDef(), opponentDef);
			m_Reach->Log(opponentDef, GetPlayerDef());
		}
	}

	void ArenaDirectorScript::UpdateBout(float deltaTime)
	{
		if (m_EventGeneration != m_Assets->GetEventGeneration())
			OnEventsChanged();

		Fighter& player = *m_Fighters[0];
		Fighter& opponent = *m_Fighters[1];

		if (m_Duel)
		{
			m_Duel->Advance(deltaTime, player, opponent, *m_Combat);
			player.SetIntent(m_Duel->GetPlayerIntent());
			opponent.SetIntent(m_Duel->GetOpponentIntent());
			if (m_Duel->ShouldClose())
				Application::Get().Close();
		}
		else if (m_Flow)
		{
			m_Flow->Update(deltaTime, player, opponent);
			if (m_Flow->AcceptsInput())
			{
				player.SetIntent(m_Brain->Think(deltaTime, player, opponent));
				opponent.SetIntent(m_OpponentBrain->Think(deltaTime, opponent, player));
			}
			else
			{
				player.SetIntent(FighterIntent());
				opponent.SetIntent(FighterIntent());
			}
		}
		else if (!m_Freeze)
		{
			player.SetIntent(m_Brain->Think(deltaTime, player, opponent));
		}

		player.Update(deltaTime, &opponent);
		opponent.Update(deltaTime, &player);
		Fighter::Separate(player, opponent, deltaTime);
		player.Apply(deltaTime);
		opponent.Apply(deltaTime);

		if (m_Freeze)
			m_Combat->UpdateDebug(player, opponent);
		else
			m_Combat->Update(player, opponent);
		if (m_Vfx)
			m_Vfx->Update(deltaTime);

		if (m_FollowCamera)
		{
			const CameraSubject subjects[] = { { player.GetPosition(), player.GetModelHeight() * player.GetDef().Scale },
				{ opponent.GetPosition(), opponent.GetModelHeight() * opponent.GetDef().Scale } };
			m_FollowCamera->Update(deltaTime, subjects[0], subjects[1], m_Freeze);
			m_World->UpdateOcclusion(deltaTime, m_FollowCamera->GetEye(), subjects);
		}

		if (m_Hud)
			m_Hud->Update(deltaTime, player, opponent, *m_Flow);
		if (m_Flow && m_Flow->IsFinished() && !m_BoutDone)
			FinishBout();

		if (m_Drive != DriveMode::None && m_Drive != DriveMode::Duel)
			LogDrive(deltaTime);
		if (m_Drive == DriveMode::Wall)
			CheckWall(deltaTime);
	}

	void ArenaDirectorScript::FinishBout()
	{
		m_BoutDone = true;
		const Fighter& player = *m_Fighters[0];
		const Fighter& opponent = *m_Fighters[1];
		const BoutWinner winner = m_Flow->GetWinner();
		const float seconds = m_Flow->GetFightTime();
		m_Match->Seconds += seconds;

		if (m_Tournament)
		{
			const char* name = ToString(winner);
			if (winner == BoutWinner::Player)
				name = player.GetDef().Name;
			else if (winner == BoutWinner::Opponent)
				name = opponent.GetDef().Name;

			m_Match->Records.push_back({ m_Seed, winner, seconds, player.GetHealth(), opponent.GetHealth() });
			const CombatStats& stats = m_Combat->GetStats();
			DE_INFO("[Bout] seed={} winner={} time={:.2f} player health={:.1f} opponent health={:.1f}", m_Seed, name, seconds, player.GetHealth(), opponent.GetHealth());
			DE_INFO("[Stats] seed={} hits={} blocks={} parries={} dodges={} best chain={}{}", m_Seed, stats.Hits, stats.Blocks, stats.Parries, stats.Dodges,
				stats.BestChain, m_Flow->IsTimedOut() ? " (timed out)" : "");

			if (static_cast<int>(m_Match->Records.size()) < GetLaunchOptions().Tournament)
			{
				m_Match->Restart = true;
			}
			else
			{
				FinishTournament();
				m_Match->Done = true;
				Application::Get().Close();
			}
			return;
		}

		if (GetLaunchOptions().Autoplay)
		{
			DE_INFO("[Bout] {} winner={} time={:.2f} player health={:.1f} opponent health={:.1f}", m_BoutNumber,
				winner == BoutWinner::Player ? player.GetDef().Name : (winner == BoutWinner::Opponent ? opponent.GetDef().Name : ToString(winner)),
				seconds, player.GetHealth(), opponent.GetHealth());
		}

		if (winner == BoutWinner::Player)
		{
			if (m_BoutNumber < BOUT_COUNT)
			{
				m_Match->Bout = m_BoutNumber + 1;
				m_Match->Restart = true;
			}
			else
			{
				m_Match->Victory = true;
				RequestSceneTransition(SCENE_END);
			}
		}
		else
		{
			++m_Match->Retries;
			m_Match->Restart = true;
		}
	}

	void ArenaDirectorScript::FinishTournament()
	{
		const int total = static_cast<int>(m_Match->Records.size());
		int wins = 0;
		int losses = 0;
		int draws = 0;
		int timeouts = 0;
		float seconds = 0.0f;
		for (const BoutRecord& record : m_Match->Records)
		{
			seconds += record.Seconds;
			switch (record.Winner)
			{
				case BoutWinner::Player:   ++wins; break;
				case BoutWinner::Opponent: ++losses; break;
				case BoutWinner::Draw:     ++draws; break;
				default:                   ++timeouts; break;
			}
		}

		const int tier = m_BoutNumber;
		const int playerTier = GetLaunchOptions().PlayerTier;
		DE_INFO("[Tournament] tier {} against tier {} ({}), seeds {} to {}: {} wins, {} losses, {} draws, {} timeouts; {:.1f} s of fighting, {:.1f} s a bout",
			playerTier, tier, GetOpponentDef(tier).Name, m_Match->Records.front().Seed, m_Match->Records.back().Seed, wins, losses, draws, timeouts, seconds,
			total > 0 ? seconds / static_cast<float>(total) : 0.0f);

		const int needed = static_cast<int>(std::ceil(TOURNAMENT_PASS_FRACTION * static_cast<float>(total) - 1.0e-3f));
		if (playerTier > tier)
		{
			CheckReport().Check(wins >= needed, std::format("tournament: tier {} beat tier {} in {} of {} (need {} of {})", playerTier, tier, wins, total, needed, total));
		}
		else if (playerTier < tier)
		{
			CheckReport().Check(losses >= needed, std::format("tournament: tier {} beat tier {} in {} of {} (need {} of {})", tier, playerTier, losses, total, needed, total));
		}
		else
		{
			DE_INFO("[INFO] tournament: tier {} beat tier {} in {} of {} (a mirror: no pass bar)", playerTier, tier, wins, total);
		}
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
		m_Showcase.reset();
		m_Hud.reset();
		m_Flow.reset();
		m_Camera.reset();
		m_FollowCamera.reset();
		m_OpponentBrain.reset();
		m_Brain.reset();
		m_Duel.reset();
		m_Combat.reset();
		m_Fighters.clear();
		m_Audio.reset();
		m_World.reset();
		m_Vfx.reset();
	}

}
