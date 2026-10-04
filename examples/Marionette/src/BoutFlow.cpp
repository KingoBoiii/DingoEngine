#include "BoutFlow.h"
#include "Fighter.h"

namespace
{
	using namespace Dingo;

	// A fighter whose death clip is missing never reaches Dead, so zero health counts too.
	bool IsDown(const Fighter& fighter)
	{
		return fighter.IsDead() || fighter.GetHealth() <= 0.0f;
	}
}

namespace Dingo
{

	const char* ToString(BoutWinner winner)
	{
		switch (winner)
		{
			case BoutWinner::Player:   return "player";
			case BoutWinner::Opponent: return "opponent";
			case BoutWinner::Draw:     return "draw";
			default:                   return "none";
		}
	}

	BoutFlow::BoutFlow(const BoutRules& rules)
		: m_Rules(rules)
	{}

	void BoutFlow::Begin(Fighter& opponent)
	{
		if (m_Rules.Taunts && !m_Rules.Frozen)
			opponent.Taunt(opponent.GetDef().Intro);
	}

	void BoutFlow::Update(float deltaTime, Fighter& player, Fighter& opponent)
	{
		if (m_Rules.Frozen || m_Finished)
			return;

		m_PhaseTime += deltaTime;
		switch (m_Phase)
		{
			case BoutPhase::Intro:
				if (m_PhaseTime >= m_Rules.IntroSeconds && (opponent.IsCalm() || m_PhaseTime >= BOUT_INTRO_MAX_SECONDS))
				{
					m_Phase = BoutPhase::Fight;
					m_PhaseTime = 0.0f;
				}
				break;

			case BoutPhase::Fight:
				m_FightTime += deltaTime;
				if (IsDown(player) || IsDown(opponent))
				{
					EnterKnockout(player, opponent);
				}
				else if (m_Rules.TimeLimit > 0.0f && m_FightTime >= m_Rules.TimeLimit)
				{
					m_TimedOut = true;
					m_Finished = true;
				}
				break;

			case BoutPhase::Knockout:
				UpdateKnockout(player, opponent);
				break;
		}
	}

	void BoutFlow::EnterKnockout(Fighter& player, Fighter& opponent)
	{
		m_Phase = BoutPhase::Knockout;
		m_PhaseTime = 0.0f;
		if (IsDown(player) && IsDown(opponent))
			m_Winner = BoutWinner::Draw;
		else
			m_Winner = IsDown(player) ? BoutWinner::Opponent : BoutWinner::Player;

		if (!(m_Rules.KnockoutSeconds > 0.0f))
			m_Finished = true;
	}

	void BoutFlow::UpdateKnockout(Fighter& player, Fighter& opponent)
	{
		const bool hasWinner = m_Winner == BoutWinner::Player || m_Winner == BoutWinner::Opponent;
		if (m_Rules.Taunts && hasWinner && !m_Taunted && m_PhaseTime >= BOUT_KO_TAUNT_DELAY)
		{
			Fighter& winner = m_Winner == BoutWinner::Player ? player : opponent;
			if (winner.IsCalm() || m_PhaseTime >= BOUT_KO_TAUNT_FORCE)
			{
				winner.Taunt(Clips::TAUNT);
				m_Taunted = true;
			}
		}

		if (m_PhaseTime >= m_Rules.KnockoutSeconds)
			m_Finished = true;
	}

}
