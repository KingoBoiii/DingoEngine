#pragma once
#include "GameTuning.h"

#include <cstdint>

namespace Dingo
{

	class Fighter;
	class GameAudio;

	enum class BoutPhase : uint8_t
	{
		Intro,
		Fight,
		Knockout
	};

	enum class BoutWinner : uint8_t
	{
		None,
		Player,
		Opponent,
		Draw
	};

	const char* ToString(BoutWinner winner);

	struct BoutRules
	{
		float IntroSeconds = BOUT_INTRO_SECONDS;
		float KnockoutSeconds = BOUT_KO_SECONDS;
		// Fight seconds before the bout is called with no winner; 0 never calls it.
		float TimeLimit = 0.0f;
		bool Taunts = true;
		bool Frozen = false;
	};

	// The bout's phases, on the scene's clock: an intro nobody can act in, the fight, and the knockout where
	// the winner taunts over the loser. The director feeds the brains only while AcceptsInput(). With `audio`, the
	// K.O. sounds a sting, and a moment later the victory or the defeat one.
	class BoutFlow
	{
	public:
		explicit BoutFlow(const BoutRules& rules, const GameAudio* audio = nullptr);

		void Begin(Fighter& opponent);
		void Update(float deltaTime, Fighter& player, Fighter& opponent);

		BoutPhase GetPhase() const { return m_Phase; }
		float GetPhaseTime() const { return m_PhaseTime; }
		float GetFightTime() const { return m_FightTime; }
		const BoutRules& GetRules() const { return m_Rules; }
		bool AcceptsInput() const { return m_Phase == BoutPhase::Fight && !m_Rules.Frozen && !m_Finished; }
		bool IsFinished() const { return m_Finished; }
		bool IsTimedOut() const { return m_TimedOut; }
		BoutWinner GetWinner() const { return m_Winner; }

	private:
		void EnterKnockout(Fighter& player, Fighter& opponent);
		void UpdateKnockout(Fighter& player, Fighter& opponent);

	private:
		BoutRules m_Rules;
		const GameAudio* m_Audio;
		BoutPhase m_Phase = BoutPhase::Intro;
		BoutWinner m_Winner = BoutWinner::None;
		float m_PhaseTime = 0.0f;
		float m_FightTime = 0.0f;
		bool m_Taunted = false;
		bool m_Stung = false;
		bool m_TimedOut = false;
		bool m_Finished = false;
	};

}
