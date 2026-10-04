#pragma once
#include "FighterIntent.h"
#include "LaunchOptions.h"

#include <DingoEngine.h>

#include <memory>
#include <vector>

namespace Dingo
{

	class ArenaWorld;
	class CameraRig;
	class Fighter;
	class FollowCamera;
	class GameAssets;
	class GameAudio;
	struct FighterContext;

	class ArenaDirectorScript : public ScriptableEntity
	{
	public:
		explicit ArenaDirectorScript(const GameAssets* assets);
		~ArenaDirectorScript() override;

	protected:
		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;

	private:
		void BuildLineup(const FighterContext& context, const LaunchOptions& options);
		void BuildBout(const FighterContext& context, const LaunchOptions& options);
		void UpdateBout(float deltaTime);
		void LogDrive(float deltaTime);
		void CheckWall(float deltaTime);

	private:
		const GameAssets* m_Assets = nullptr;
		double m_Time = 0.0;
		std::unique_ptr<ArenaWorld> m_World;
		std::unique_ptr<GameAudio> m_Audio;
		std::vector<std::unique_ptr<Fighter>> m_Fighters;
		std::unique_ptr<Brain> m_Brain;
		std::unique_ptr<CameraRig> m_Camera;
		std::unique_ptr<FollowCamera> m_FollowCamera;
		bool m_Bout = false;
		bool m_Freeze = false;
		DriveMode m_Drive = DriveMode::None;
		float m_LogTimer = 0.0f;
		float m_WallPressed = 0.0f;
		bool m_WallChecked = false;
	};

}
