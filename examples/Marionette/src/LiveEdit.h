#pragma once
#include <chrono>
#include <filesystem>

namespace Dingo
{

	class GameAssets;

	// Where --live-edit-demo reads its assets from: a fresh copy under the temp directory.
	std::filesystem::path GetLiveEditRoot();

	// Replaces the live-edit copy with `source`. False, with the game left on its own assets, when the copy fails.
	bool PrepareLiveEditAssets(const std::filesystem::path& source);

	// Removes the copy PrepareLiveEditAssets made, and nothing else: the guards that made the copy have to hold again.
	void CleanupLiveEditAssets();

	// After LIVE_EDIT_DELAY seconds, moves the hitbox and the combo window after it of one move in the copy's .events
	// file later, so the running game is seen to change when the file does. It writes nowhere but into the copy.
	class LiveEditDemo
	{
	public:
		explicit LiveEditDemo(const GameAssets& assets);

		// `deltaTime` is scene time.
		void Update(float deltaTime);
		// The game saw some clip's events change.
		void OnReload();

	private:
		enum class Stage
		{
			Waiting,
			Written,
			Done
		};

		void Write();

	private:
		const GameAssets& m_Assets;
		Stage m_Stage = Stage::Waiting;
		float m_Time = 0.0f;
		std::chrono::steady_clock::time_point m_WrittenAt;
		float m_Begin = 0.0f;
		float m_End = 0.0f;
		bool m_HasCombo = false;
		float m_ComboBegin = 0.0f;
		float m_ComboEnd = 0.0f;
	};

}
