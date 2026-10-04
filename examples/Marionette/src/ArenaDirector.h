#pragma once
#include "FighterIntent.h"
#include "LaunchOptions.h"

#include <DingoEngine.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Dingo
{

	class ArenaWorld;
	class BoutFlow;
	class CameraRig;
	class Combat;
	class DuelScript;
	class Fighter;
	class FollowCamera;
	class GameAssets;
	class GameAudio;
	class HitDebugView;
	class Hud;
	class ReachTable;
	struct FighterContext;
	struct MatchState;

	// Builds the arena and runs everything in a fixed order: brains, fighters, combat, camera, HUD. One
	// director is one bout; the layer builds a fresh one for the next.
	class ArenaDirectorScript : public ScriptableEntity
	{
	public:
		ArenaDirectorScript(const GameAssets* assets, const ReachTable* reach, MatchState* match);
		~ArenaDirectorScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void BuildLineup(const FighterContext& context, const LaunchOptions& options);
		void BuildBout(const FighterContext& context, const LaunchOptions& options);
		void UpdateBout(float deltaTime);
		void FinishBout();
		void FinishTournament();
		void LogDrive(float deltaTime);
		void CheckWall(float deltaTime);

	private:
		const GameAssets* m_Assets = nullptr;
		const ReachTable* m_Reach = nullptr;
		MatchState* m_Match = nullptr;
		double m_Time = 0.0;
		std::unique_ptr<ArenaWorld> m_World;
		std::unique_ptr<GameAudio> m_Audio;
		std::unique_ptr<HitDebugView> m_DebugView;
		std::vector<std::unique_ptr<Fighter>> m_Fighters;
		std::unique_ptr<Combat> m_Combat;
		std::unique_ptr<DuelScript> m_Duel;
		std::unique_ptr<Brain> m_Brain;
		std::unique_ptr<Brain> m_OpponentBrain;
		std::unique_ptr<BoutFlow> m_Flow;
		std::unique_ptr<Hud> m_Hud;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<FollowCamera> m_FollowCamera;
		bool m_Bout = false;
		bool m_BoutDone = false;
		bool m_Freeze = false;
		bool m_Tournament = false;
		int m_BoutNumber = 1;
		uint32_t m_Seed = 1;
		DriveMode m_Drive = DriveMode::None;
		float m_LogTimer = 0.0f;
		float m_WallPressed = 0.0f;
		bool m_WallChecked = false;
	};

}
