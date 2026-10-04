#pragma once
#include "Combat.h"
#include "FighterIntent.h"
#include "GameTuning.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Dingo
{

	class Fighter;

	// --drive=duel: both fighters act from one script that reads only what a player could see, so a run
	// repeats exactly at a fixed delta. The opponent swings four times and the player answers each in
	// turn: takes it, blocks it past the parry window, parries it (and ripostes), dodges it. Then the
	// player lands a three-hit light chain. The report says whether every outcome fell inside its event.
	class DuelScript
	{
	public:
		explicit DuelScript(bool brokenHitboxes);

		void Advance(float deltaTime, const Fighter& player, const Fighter& opponent, const Combat& combat);

		const FighterIntent& GetPlayerIntent() const { return m_PlayerIntent; }
		const FighterIntent& GetOpponentIntent() const { return m_OpponentIntent; }
		// True a moment after the report, so the last frames are logged.
		bool ShouldClose() const { return m_Phase == Phase::Done && m_DoneTime >= DUEL_END_DELAY; }

	private:
		enum class Response : uint8_t
		{
			Take,
			BlockLate,
			Parry,
			Dodge
		};

		enum class Phase : uint8_t
		{
			Settle,
			Strike,
			Approach,
			Chain,
			Finish,
			Done
		};

		static constexpr std::array<Response, 4> k_Rounds = { Response::Take, Response::BlockLate, Response::Parry, Response::Dodge };

		void AdvanceSettle(float deltaTime, const Fighter& player, const Fighter& opponent, const Combat& combat);
		void AdvanceStrike(const Fighter& player, const Fighter& opponent, const Combat& combat, bool& keepBlock);
		void AdvanceApproach(const Fighter& player, const Fighter& opponent);
		void AdvanceChain(const Fighter& player, const Fighter& opponent, const Combat& combat);
		void EndRound(const Combat& combat, const char* opponentName, bool timedOut);
		void Fail(std::string reason);
		void Report(const Fighter& player, const Fighter& opponent, const Combat& combat);

	private:
		bool m_Broken;
		Phase m_Phase = Phase::Settle;
		double m_Time = 0.0;
		float m_Calm = 0.0f;
		float m_PhaseTime = 0.0f;
		float m_DoneTime = 0.0f;
		float m_ParryTime = -1.0f;
		size_t m_Round = 0;
		size_t m_OutcomeStart = 0;
		int m_ParriesBefore = 0;
		int m_ChainPresses = 0;
		int m_ChainTaken = 0;
		bool m_SawAttack = false;
		bool m_Responded = false;
		bool m_RiposteSent = false;
		bool m_HoldBlock = false;
		std::vector<std::string> m_Failures;
		FighterIntent m_PlayerIntent;
		FighterIntent m_OpponentIntent;
	};

}
