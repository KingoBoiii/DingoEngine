#include "DuelScript.h"
#include "CheckReport.h"
#include "Fighter.h"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

namespace
{
	using namespace Dingo;

	constexpr std::array<const char*, 4> k_ResponseNames = { "takes it", "blocks it past the parry window", "parries it", "dodges it" };
	constexpr std::array<Outcome, 4> k_Expected = { Outcome::Hit, Outcome::Blocked, Outcome::Parry, Outcome::Dodged };

	std::string Join(const std::vector<std::string>& reasons)
	{
		std::string joined;
		for (const std::string& reason : reasons)
			joined += std::format("{}{}", joined.empty() ? "" : "; ", reason);
		return joined;
	}
}

namespace Dingo
{

	DuelScript::DuelScript(bool brokenHitboxes)
		: m_Broken(brokenHitboxes)
	{}

	void DuelScript::Advance(float deltaTime, const Fighter& player, const Fighter& opponent, const Combat& combat)
	{
		m_PlayerIntent = FighterIntent();
		m_OpponentIntent = FighterIntent();
		if (m_Phase == Phase::Done)
		{
			m_DoneTime += deltaTime;
			return;
		}

		m_Time += deltaTime;
		m_PhaseTime += deltaTime;
		if (m_Time > DUEL_DEADLINE)
		{
			Fail(std::format("the {:.0f} s deadline passed", DUEL_DEADLINE));
			Report(player, opponent, combat);
			return;
		}

		bool keepBlock = false;
		switch (m_Phase)
		{
			case Phase::Settle:   AdvanceSettle(deltaTime, player, opponent, combat); break;
			case Phase::Strike:   AdvanceStrike(player, opponent, combat, keepBlock); break;
			case Phase::Approach: AdvanceApproach(player, opponent); break;
			case Phase::Chain:    AdvanceChain(player, opponent, combat); break;
			case Phase::Finish:
				m_Calm = player.IsCalm() && opponent.IsCalm() ? m_Calm + deltaTime : 0.0f;
				if (m_Calm >= DUEL_SETTLE)
					Report(player, opponent, combat);
				break;
			default:
				break;
		}
		m_PlayerIntent.Block = m_HoldBlock || keepBlock;
	}

	void DuelScript::AdvanceSettle(float deltaTime, const Fighter& player, const Fighter& opponent, const Combat& combat)
	{
		m_Calm = player.IsCalm() && opponent.IsCalm() ? m_Calm + deltaTime : 0.0f;
		if (m_Time < DUEL_START_DELAY || m_Calm < DUEL_SETTLE)
			return;

		if (m_Round >= k_Rounds.size())
		{
			m_Phase = Phase::Approach;
			m_PhaseTime = 0.0f;
			return;
		}

		m_Phase = Phase::Strike;
		m_PhaseTime = 0.0f;
		m_SawAttack = false;
		m_Responded = false;
		m_RiposteSent = false;
		m_ParryTime = -1.0f;
		m_ParriesBefore = combat.GetStats().Parries;
		m_OutcomeStart = combat.GetOutcomes().size();
		m_OpponentIntent.Light = true;
		DE_INFO("[Duel] round {}: the opponent swings and the player {}", m_Round + 1, k_ResponseNames[static_cast<size_t>(k_Rounds[m_Round])]);
	}

	void DuelScript::AdvanceStrike(const Fighter& player, const Fighter& opponent, const Combat& combat, bool& keepBlock)
	{
		const Response response = k_Rounds[m_Round];
		const CombatStats& stats = combat.GetStats();
		if (opponent.GetState() == FighterState::Attack)
			m_SawAttack = true;

		const float toHit = opponent.GetSecondsToHitbox();
		switch (response)
		{
			case Response::BlockLate:
				if (toHit <= DUEL_LATE_BLOCK_LEAD)
					m_HoldBlock = true;
				break;
			case Response::Parry:
				if (!m_Responded && toHit <= DUEL_PARRY_LEAD)
				{
					m_HoldBlock = true;
					m_Responded = true;
				}
				break;
			case Response::Dodge:
				if (!m_Responded && toHit <= DUEL_DODGE_LEAD)
				{
					// A swing that does not lunge is outrun by a dodge back, and an avoided blow logs no outcome.
					const glm::vec2 toOpponent = opponent.GetGroundPosition() - player.GetGroundPosition();
					const float distance = glm::length(toOpponent);
					m_PlayerIntent.Dodge = true;
					m_PlayerIntent.Move = distance > 1.0e-3f ? toOpponent / distance : glm::vec2(0.0f);
					m_Responded = true;
				}
				break;
			default:
				break;
		}

		if (response == Response::Parry && !m_RiposteSent && stats.Parries > m_ParriesBefore)
		{
			if (m_ParryTime < 0.0f)
				m_ParryTime = static_cast<float>(m_Time);
			if (m_Time - m_ParryTime >= DUEL_RIPOSTE_DELAY)
			{
				m_PlayerIntent.Light = true;
				m_RiposteSent = true;
				keepBlock = true;
				m_HoldBlock = false;
			}
		}

		const bool attackOver = m_SawAttack && opponent.GetState() != FighterState::Attack;
		const bool parryDone = response != Response::Parry || m_RiposteSent || (stats.Parries == m_ParriesBefore && opponent.IsCalm());
		const bool over = attackOver && parryDone;
		if (over || m_PhaseTime > DUEL_ROUND_TIMEOUT)
			EndRound(combat, opponent.GetDef().Name, !over);
	}

	void DuelScript::EndRound(const Combat& combat, const char* opponentName, bool timedOut)
	{
		const Outcome expected = k_Expected[static_cast<size_t>(k_Rounds[m_Round])];
		if (timedOut)
			Fail(std::format("round {} timed out after {:.0f} s", m_Round + 1, DUEL_ROUND_TIMEOUT));

		if (m_Broken)
		{
			DE_INFO("[Duel] round {} ended; no outcome is expected", m_Round + 1);
		}
		else
		{
			const std::vector<OutcomeRecord>& outcomes = combat.GetOutcomes();
			const bool found = std::any_of(outcomes.begin() + static_cast<std::ptrdiff_t>(m_OutcomeStart), outcomes.end(),
				[&](const OutcomeRecord& record) { return record.Attacker == opponentName && record.Kind == expected; });
			if (found)
			{
				DE_INFO("[Duel] round {} gave the expected [{}]", m_Round + 1, ToString(expected));
			}
			else
			{
				Fail(std::format("round {} gave no [{}] from the opponent's swing", m_Round + 1, ToString(expected)));
			}
		}

		++m_Round;
		m_HoldBlock = false;
		m_Phase = Phase::Settle;
		m_Calm = 0.0f;
		m_PhaseTime = 0.0f;
	}

	void DuelScript::AdvanceApproach(const Fighter& player, const Fighter& opponent)
	{
		if (!player.IsCalm() || !opponent.IsCalm())
			return;

		const glm::vec2 delta = opponent.GetGroundPosition() - player.GetGroundPosition();
		const float gap = glm::length(delta);
		if (gap > DUEL_CHAIN_GAP)
		{
			m_PlayerIntent.Move = delta / gap * WALK_INTENT;
			return;
		}

		m_Phase = Phase::Chain;
		m_PhaseTime = 0.0f;
		m_ChainPresses = 0;
		m_ChainTaken = 0;
		DE_INFO("[Duel] the player closes to {:.2f} m and chains {} light attacks", gap, DUEL_CHAIN_LENGTH);
	}

	void DuelScript::AdvanceChain(const Fighter& player, const Fighter& opponent, const Combat& combat)
	{
		if (player.GetState() == FighterState::Attack)
			m_ChainTaken = std::max(m_ChainTaken, player.GetChainIndex() + 1);

		if (m_ChainTaken >= DUEL_CHAIN_LENGTH)
		{
			if (player.IsCalm())
			{
				m_Phase = Phase::Finish;
				m_Calm = 0.0f;
			}
			return;
		}

		if (m_PhaseTime > DUEL_ROUND_TIMEOUT)
		{
			Fail(std::format("the chain timed out after {:.0f} s with {} of {} presses taken", DUEL_ROUND_TIMEOUT, m_ChainTaken, DUEL_CHAIN_LENGTH));
			Report(player, opponent, combat);
			return;
		}

		if (m_ChainPresses > 0 && player.IsCalm())
		{
			Fail(m_ChainPresses > m_ChainTaken ? std::format("press {} not taken", m_ChainPresses)
				: std::format("the chain ended after {} of {} presses", m_ChainTaken, DUEL_CHAIN_LENGTH));
			Report(player, opponent, combat);
			return;
		}

		if (m_ChainPresses > m_ChainTaken)
			return;

		if (m_ChainPresses == 0)
		{
			m_PlayerIntent.Light = true;
			m_ChainPresses = 1;
			return;
		}

		const AnimationClip* clip = player.GetLayerClip(0);
		const std::optional<ClipRange> combo = clip ? FindRange(*clip, Events::COMBO) : std::nullopt;
		const float lead = m_ChainTaken == 1 ? DUEL_BUFFER_LEAD : 0.0f;
		if (player.GetState() == FighterState::Attack && combo && player.GetLayerTime(0) >= combo->Begin - lead)
		{
			m_PlayerIntent.Light = true;
			++m_ChainPresses;
		}
	}

	void DuelScript::Fail(std::string reason)
	{
		DE_ERROR("[Duel] {}", reason);
		m_Failures.push_back(std::move(reason));
	}

	void DuelScript::Report(const Fighter& player, const Fighter& opponent, const Combat& combat)
	{
		const CombatStats& stats = combat.GetStats();
		const std::string failed = m_Failures.empty() ? std::string() : std::format(", failed: {}", Join(m_Failures));
		CheckReport report;
		if (m_Broken)
		{
			const int outcomes = stats.Hits + stats.Blocks + stats.Parries + stats.Dodges;
			const uint32_t swings = player.GetSwingId() + opponent.GetSwingId();
			const uint32_t expected = static_cast<uint32_t>(k_Rounds.size()) + static_cast<uint32_t>(DUEL_CHAIN_LENGTH);
			report.Check(m_Failures.empty() && outcomes == 0 && swings == expected,
				std::format("scripted duel with broken hitboxes: {} outcomes (none expected), {} swings started ({} expected){}", outcomes, swings, expected, failed));
		}
		else
		{
			const bool passed = m_Failures.empty() && stats.Hits >= 1 && stats.Blocks >= 1 && stats.Parries >= 1 && stats.Dodges >= 1
				&& stats.RiposteLanded && stats.BestChain >= DUEL_CHAIN_LENGTH && stats.AllInRange;
			report.Check(passed, std::format("scripted duel: {} hits, {} blocks, {} parries, {} dodges, {}, chain of {} landed{}{}", stats.Hits, stats.Blocks,
				stats.Parries, stats.Dodges, stats.RiposteLanded ? "riposte landed" : "riposte missed", stats.BestChain,
				stats.AllInRange ? "" : ", an outcome fell outside its event range", failed));
		}
		m_Phase = Phase::Done;
	}

}
