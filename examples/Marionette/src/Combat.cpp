#include "Combat.h"
#include "GameTuning.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	using namespace Dingo;

	constexpr float k_RangeSlack = 1.0e-3f;

	bool Inside(const std::optional<ClipRange>& range, float time)
	{
		return range && time >= range->Begin - k_RangeSlack && time <= range->End + k_RangeSlack;
	}

	std::string Describe(const std::optional<ClipRange>& range)
	{
		return range ? std::format("[{:.2f},{:.2f}]", range->Begin, range->End) : std::string("[none]");
	}

	bool IsInsideArc(const Fighter& attacker, const Fighter& target)
	{
		const glm::vec2 toAttacker = attacker.GetGroundPosition() - target.GetGroundPosition();
		const float distance = glm::length(toAttacker);
		if (!(distance > 1.0e-4f))
			return true;
		return glm::dot(target.GetFacing(), toAttacker / distance) >= std::cos(glm::radians(BLOCK_ARC_DEG));
	}
}

namespace Dingo
{

	const char* ToString(Outcome outcome)
	{
		switch (outcome)
		{
			case Outcome::Blocked: return "Blocked";
			case Outcome::Parry:   return "Parry";
			case Outcome::Dodged:  return "Dodged";
			default:               return "Hit";
		}
	}

	Combat::Combat(const GameAudio& audio, bool log)
		: m_Audio(audio), m_Log(log)
	{}

	void Combat::Update(Fighter& a, Fighter& b)
	{
		a.AdvanceWeaponTrail();
		b.AdvanceWeaponTrail();

		// Both swings are judged before either lands, so a trade is a trade.
		std::optional<Decision> first = Judge(a, b);
		std::optional<Decision> second = Judge(b, a);
		if (first)
			Commit(*first);
		if (second)
			Commit(*second);

		a.ClearHitboxPulse();
		b.ClearHitboxPulse();
		UpdateDebug(a, b);
	}

	void Combat::UpdateDebug(Fighter& a, Fighter& b)
	{
		a.UpdateDebugTint();
		b.UpdateDebugTint();
	}

	std::optional<Combat::Decision> Combat::Judge(Fighter& attacker, Fighter& target)
	{
		if (attacker.IsDead() || target.IsDead() || attacker.IsSwingResolved() || !attacker.IsHitboxLive())
			return std::nullopt;

		const MoveDef* move = attacker.GetMove();
		const AnimationClip* attackClip = attacker.GetLayerClip(0);
		if (!move || !attackClip)
			return std::nullopt;

		m_SwingScratch.clear();
		m_HurtScratch.clear();
		attacker.CollectSwingSpheres(m_SwingScratch);
		target.CollectHurtSpheres(m_HurtScratch);
		const bool touching = std::any_of(m_SwingScratch.begin(), m_SwingScratch.end(), [&](const SweptSphere& swept)
		{
			return std::any_of(m_HurtScratch.begin(), m_HurtScratch.end(), [&](const WorldSphere& hurt) { return Touches(swept, hurt); });
		});
		if (!touching)
			return std::nullopt;

		Decision decision;
		decision.Attacker = &attacker;
		decision.Target = &target;
		OutcomeRecord& record = decision.Record;
		record.Attacker = attacker.GetDef().Name;
		record.Target = target.GetDef().Name;
		record.Move = move->Clip;
		record.Riposte = move->Kind == MoveKind::Riposte;
		record.ChainIndex = attacker.GetChainIndex();
		record.AttackTime = attacker.GetHitboxTime();
		record.Hitbox = FindRange(*attackClip, Events::HITBOX);
		record.InHitbox = Inside(record.Hitbox, record.AttackTime);
		record.Damage = move->Damage;

		if (target.GetState() == FighterState::Dodge && target.IsWindowActive(Events::IFRAMES))
		{
			record.Kind = Outcome::Dodged;
			record.Damage = 0.0f;
			if (const AnimationClip* dodge = target.GetLayerClip(0))
			{
				record.DefenceClip = dodge->GetName();
				record.Window = FindRange(*dodge, Events::IFRAMES);
			}
			record.DefenceTime = target.GetLayerTime(0);
			record.InWindow = Inside(record.Window, record.DefenceTime);
		}
		else if (target.GetState() == FighterState::Block && IsInsideArc(attacker, target))
		{
			const AnimationClip* block = target.GetLayerClip(BLOCK_LAYER);
			if (block)
			{
				record.DefenceClip = block->GetName();
				record.Window = FindRange(*block, Events::PARRY);
			}
			record.DefenceTime = target.GetLayerTime(BLOCK_LAYER);

			if (target.IsParryOpen())
			{
				record.Kind = Outcome::Parry;
				record.Damage = 0.0f;
				record.InWindow = Inside(record.Window, record.DefenceTime);
				record.ParryPending = !target.IsWindowActive(Events::PARRY);
			}
			else
			{
				record.Kind = Outcome::Blocked;
				record.Damage = move->Damage * BLOCK_CHIP_FRACTION;
				record.ParryDenied = target.IsParryDenied();
				record.InWindow = record.ParryDenied || !Inside(record.Window, record.DefenceTime);
			}
		}
		return decision;
	}

	void Combat::Commit(Decision& decision)
	{
		Fighter& attacker = *decision.Attacker;
		Fighter& target = *decision.Target;
		OutcomeRecord& record = decision.Record;

		const glm::vec3 where = target.GetPosition();
		switch (record.Kind)
		{
			case Outcome::Hit:
				target.TakeHit(record.Damage);
				m_Audio.PlayCombat(CombatSound::Hit, where, target.GetDef().Scale);
				StartHitStop(attacker, target);
				++m_Stats.Hits;
				m_Stats.RiposteLanded = m_Stats.RiposteLanded || record.Riposte;
				break;

			case Outcome::Blocked:
			{
				glm::vec2 away = target.GetGroundPosition() - attacker.GetGroundPosition();
				away = glm::length(away) > 1.0e-4f ? glm::normalize(away) : target.GetFacing() * -1.0f;
				target.TakeBlock(record.Damage, away);
				m_Audio.PlayCombat(CombatSound::Block, where, target.GetDef().Scale);
				StartHitStop(attacker, target);
				++m_Stats.Blocks;
				break;
			}

			case Outcome::Parry:
				attacker.Stagger();
				target.OpenRiposteWindow();
				m_Audio.PlayCombat(CombatSound::Parry, where, target.GetDef().Scale);
				StartHitStop(attacker, target);
				++m_Stats.Parries;
				break;

			case Outcome::Dodged:
				m_Audio.PlayCombat(CombatSound::Dodge, where, target.GetDef().Scale);
				++m_Stats.Dodges;
				break;
		}

		attacker.ResolveSwing();
		record.Health = target.GetHealth();
		m_Stats.AllInRange = m_Stats.AllInRange && record.IsValid();
		Track(decision);
		Log(record);
		m_Outcomes.push_back(record);
	}

	void Combat::StartHitStop(Fighter& a, Fighter& b)
	{
		a.StartHitStop(HITSTOP_SECONDS);
		b.StartHitStop(HITSTOP_SECONDS);
	}

	void Combat::Track(const Decision& decision)
	{
		int& run = m_ChainRuns[decision.Attacker];
		const int index = decision.Record.ChainIndex;
		if (decision.Record.Kind != Outcome::Hit)
			run = 0;
		else if (index == 0)
			run = 1;
		else if (index > 0 && index == run)
			++run;
		else
			run = 0;
		m_Stats.BestChain = std::max(m_Stats.BestChain, run);
	}

	void Combat::Log(const OutcomeRecord& record) const
	{
		if (!m_Log)
			return;

		std::string line = std::format("[{}] {} -> {} {} t={:.2f} hitbox={}", ToString(record.Kind), record.Attacker, record.Target,
			record.Move, record.AttackTime, Describe(record.Hitbox));
		switch (record.Kind)
		{
			case Outcome::Hit:
				line += std::format(" damage={:g} health={:g}", record.Damage, record.Health);
				break;
			case Outcome::Blocked:
				line += std::format(" chip={:g} health={:g} | {} t={:.2f} parry={} ({})", record.Damage, record.Health, record.DefenceClip,
					record.DefenceTime, Describe(record.Window), record.ParryDenied ? "denied: raised too soon after lowering" : "past it");
				break;
			case Outcome::Parry:
				line += std::format(" | {} t={:.2f} parry={}{}", record.DefenceClip, record.DefenceTime, Describe(record.Window),
					record.ParryPending ? " (raise just started, its parry event not begun yet)" : "");
				break;
			case Outcome::Dodged:
				line += std::format(" | {} t={:.2f} iframes={}", record.DefenceClip, record.DefenceTime, Describe(record.Window));
				break;
		}
		if (record.Riposte)
			line += " (riposte)";
		if (!record.IsValid())
			line += " OUTSIDE ITS EVENT RANGE";

		if (record.IsValid())
		{
			DE_INFO("{}", line);
		}
		else
		{
			DE_ERROR("{}", line);
		}
	}

}
