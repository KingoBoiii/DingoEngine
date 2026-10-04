#include "AiBrain.h"
#include "ArenaWorld.h"
#include "ReachTable.h"

#include <algorithm>
#include <cmath>

namespace
{
	using namespace Dingo;

	constexpr float k_Infinity = std::numeric_limits<float>::infinity();

	glm::vec2 Perpendicular(const glm::vec2& v)
	{
		return glm::vec2(v.y, -v.x);
	}

	glm::vec2 ClampLength(const glm::vec2& v, float limit)
	{
		const float length = glm::length(v);
		return length > limit && length > 0.0f ? v * (limit / length) : v;
	}
}

namespace Dingo
{

	AiView MakeAiView(const Fighter& fighter)
	{
		AiView view;
		view.Def = &fighter.GetDef();
		view.Position = fighter.GetGroundPosition();
		view.Facing = fighter.GetFacing();
		view.State = fighter.GetState();
		view.Health = fighter.GetHealth();
		view.Move = fighter.GetMove();
		view.ChainIndex = fighter.GetChainIndex();
		view.SwingId = fighter.GetSwingId();
		view.SwingResolved = fighter.IsSwingResolved();
		view.CanRiposte = fighter.CanRiposte();
		if (view.State == FighterState::Attack)
		{
			view.Windup = fighter.IsWindowActive(Events::WINDUP);
			view.Hitbox = fighter.IsHitboxLive();
			view.ComboOpen = fighter.IsWindowActive(Events::COMBO);
			view.SecondsToHitbox = fighter.GetSecondsToHitbox();
		}
		return view;
	}

	void Perception::Push(double now, const AiView& view)
	{
		m_Entries.push_back({ now, view });
	}

	Perception::Seen Perception::Perceive(double now, float delay)
	{
		if (m_Entries.empty())
			return {};

		const double wanted = now - static_cast<double>(delay);
		while (m_Entries.size() > 1 && m_Entries[1].Time <= wanted)
			m_Entries.pop_front();
		return { &m_Entries.front().View, m_Entries.front().Time };
	}

	AiBrain::AiBrain(const AiTierParams& params, uint32_t seed, const ReachTable* reach)
		: m_Params(params), m_Rng(seed), m_Reach(reach)
	{}

	float AiBrain::Next01()
	{
		return static_cast<float>(m_Rng() >> 8) * (1.0f / 16777216.0f);
	}

	FighterIntent AiBrain::Think(float deltaTime, const Fighter& self, const Fighter& opponent)
	{
		return Decide(deltaTime, MakeAiView(self), MakeAiView(opponent));
	}

	void AiBrain::Begin(const AiView& self)
	{
		m_Started = true;
		m_PrevState = self.State;
		m_NextMove = ChooseMove(self);
		m_AttackTimer = Range(m_Params.AttackIntervalMin, m_Params.AttackIntervalMax);
		m_StrafeSign = Chance(0.5f) ? 1.0f : -1.0f;
		m_StrafeTimer = Range(AI_STRAFE_TURN_MIN, AI_STRAFE_TURN_MAX);
	}

	const MoveDef* AiBrain::FirstLight(const AiView& self) const
	{
		return self.Def->LightChain.empty() ? nullptr : FindMove(self.Def->LightChain[0]);
	}

	const MoveDef* AiBrain::ChooseMove(const AiView& self)
	{
		const MoveDef* heavy = GetHeavyMove(*self.Def);
		if (heavy && Chance(m_Params.HeavyChance))
			return heavy;
		const MoveDef* light = FirstLight(self);
		return light ? light : heavy;
	}

	float AiBrain::ReachOf(const AiView& attacker, const MoveDef& move, const AiView& target) const
	{
		if (m_Reach && attacker.Def && target.Def)
		{
			const MoveReach& reach = m_Reach->Get(*attacker.Def, move, *target.Def);
			if (reach.Valid && reach.Max >= REACH_SANE_MIN && reach.Max <= REACH_SANE_MAX)
				return reach.Max;
		}
		return AI_FALLBACK_REACH;
	}

	float AiBrain::AttackDistance(const AiView& self, const AiView& opponent, const MoveDef* move) const
	{
		if (!move)
			return AI_MIN_ATTACK_DISTANCE;
		return std::max(ReachOf(self, *move, opponent) - AI_REACH_MARGIN, AI_MIN_ATTACK_DISTANCE);
	}

	float AiBrain::OpponentReach(const AiView& opponent, const AiView& self) const
	{
		float reach = 0.0f;
		if (const MoveDef* light = FirstLight(opponent))
			reach = std::max(reach, ReachOf(opponent, *light, self));
		if (const MoveDef* heavy = GetHeavyMove(*opponent.Def))
			reach = std::max(reach, ReachOf(opponent, *heavy, self));
		return reach > 0.0f ? reach : AI_FALLBACK_REACH;
	}

	bool AiBrain::Threatens(const AiView& self, const AiView& seen) const
	{
		const glm::vec2 offset = self.Position - seen.Position;
		const float gap = glm::length(offset);
		if (gap > ReachOf(seen, *seen.Move, self) + AI_THREAT_MARGIN)
			return false;
		if (!(gap > 1.0e-3f))
			return true;
		return glm::dot(seen.Facing, offset / gap) >= std::cos(glm::radians(AI_THREAT_ARC_DEG));
	}

	float AiBrain::PredictedHit(const AiView& seen, double seenTime) const
	{
		if (!std::isfinite(seen.SecondsToHitbox))
			return k_Infinity;
		return seen.SecondsToHitbox - static_cast<float>(m_Clock - seenTime);
	}

	float AiBrain::ContactDelay(const AiView& seen, const AiView& self) const
	{
		if (!m_Reach || !seen.Def || !self.Def || !seen.Move)
			return 0.0f;

		const MoveReach& reach = m_Reach->Get(*seen.Def, *seen.Move, *self.Def);
		return reach.Valid ? reach.DelayAt(glm::length(seen.Position - self.Position)) / seen.Def->Pace : 0.0f;
	}

	float AiBrain::PredictedContact(const AiView& seen, double seenTime, const AiView& self) const
	{
		const float opens = PredictedHit(seen, seenTime);
		return std::isfinite(opens) ? opens + ContactDelay(seen, self) : opens;
	}

	void AiBrain::TrackState(const AiView& self)
	{
		if (self.State != m_PrevState)
		{
			if (self.State == FighterState::Attack)
			{
				m_AttackTimer = Range(m_Params.AttackIntervalMin, m_Params.AttackIntervalMax);
				m_NextMove = ChooseMove(self);
				m_Plan = Plan();
				m_LingerTimer = 0.0f;
			}
			else if (m_PrevState == FighterState::Attack && m_Params.RetreatMax > 0.0f)
			{
				m_Stance = Stance::Retreat;
				m_StanceTimer = Range(m_Params.RetreatMin, m_Params.RetreatMax);
			}
		}
		m_PrevState = self.State;
	}

	FighterIntent AiBrain::Decide(float deltaTime, const AiView& self, const AiView& opponent)
	{
		FighterIntent intent;
		if (!self.Def || !opponent.Def)
			return intent;
		if (!m_Started)
			Begin(self);

		m_Clock += deltaTime;
		m_Perception.Push(m_Clock, opponent);
		const Perception::Seen seen = m_Perception.Perceive(m_Clock, m_Params.ReactionTime);

		m_AttackTimer -= deltaTime;
		m_PunishTimer -= deltaTime;
		m_StrafeTimer -= deltaTime;
		m_StanceTimer -= deltaTime;
		m_LingerTimer -= deltaTime;
		m_PlanAge += deltaTime;

		if (self.State == FighterState::Dead || opponent.State == FighterState::Dead)
		{
			m_Plan = Plan();
			m_PrevState = self.State;
			return intent;
		}

		TrackState(self);

		const glm::vec2 offset = opponent.Position - self.Position;
		const float distance = glm::length(offset);
		const glm::vec2 toward = distance > 1.0e-3f ? offset / distance : self.Facing;

		switch (self.State)
		{
			case FighterState::Locomotion:
			case FighterState::Block:
				DecideFree(deltaTime, self, opponent, *seen.View, seen.Time, distance, toward, intent);
				break;
			case FighterState::Attack:
				DecideAttack(self, opponent, distance, intent);
				break;
			default:
				break;
		}
		return intent;
	}

	void AiBrain::DecideAttack(const AiView& self, const AiView& opponent, float distance, FighterIntent& intent)
	{
		if (!self.Move || self.Move->Kind != MoveKind::Light || !self.ComboOpen || m_ChainSwing == self.SwingId)
			return;

		// One look per swing, as the combo window opens: the swing landed if the opponent is reeling from it.
		m_ChainSwing = self.SwingId;
		const int links = std::min(m_Params.MaxChain, static_cast<int>(self.Def->LightChain.size()));
		const MoveDef* next = GetNextInChain(*self.Def, self.Move->Clip);
		const bool landed = self.SwingResolved && opponent.State == FighterState::HitReact;
		if (next && self.ChainIndex + 1 < links && landed && distance <= ReachOf(self, *next, opponent) && Chance(m_Params.ChainChance))
			intent.Light = true;
	}

	void AiBrain::DecideFree(float deltaTime, const AiView& self, const AiView& opponent, const AiView& seen, double seenTime, float distance,
		const glm::vec2& toward, FighterIntent& intent)
	{
		if (self.State == FighterState::Block && self.CanRiposte)
		{
			if (!m_RiposteArmed)
			{
				m_RiposteArmed = true;
				m_RiposteWait = 0.0f;
				m_RipostePlanned = Chance(m_Params.RiposteChance);
			}
			m_RiposteWait += deltaTime;

			intent.Block = true;
			const MoveDef& riposte = GetRiposteMove();
			if (m_RipostePlanned && m_RiposteWait >= m_Params.RiposteDelay && distance <= AttackDistance(self, opponent, &riposte))
				intent.Light = true;
			return;
		}
		m_RiposteArmed = false;

		if (m_StrafeTimer <= 0.0f)
		{
			if (Chance(AI_STRAFE_FLIP_CHANCE))
				m_StrafeSign = -m_StrafeSign;
			m_StrafeTimer = Range(AI_STRAFE_TURN_MIN, AI_STRAFE_TURN_MAX);
		}

		const Reaction reaction = React(self, seen, seenTime, toward);
		if (reaction.Busy)
		{
			intent.Block = reaction.Block;
			intent.Dodge = reaction.Dodge;
			intent.Move = AvoidWall(self.Position, reaction.Move);
			return;
		}

		DecideOffence(self, opponent, seen, distance, toward, intent);
	}

	void AiBrain::RollPlan(const AiView& self, const AiView& seen, float predicted)
	{
		m_Plan = Plan();
		m_Plan.Swing = seen.SwingId;

		// A tier that does not parry blocks early, which is only a block if the hit is past the parry window.
		const bool parries = m_Params.ParryChanceLight > 0.0f || m_Params.ParryChanceHeavy > 0.0f;
		const bool early = predicted >= PARRY_WINDOW_END / self.Def->Pace;
		const bool heavy = seen.Move->Kind == MoveKind::Heavy;
		if (Chance(heavy ? m_Params.ParryChanceHeavy : m_Params.ParryChanceLight))
		{
			m_Plan.Kind = PlanKind::Parry;
		}
		else if (Chance(m_Params.DodgeChance) && predicted >= AI_DODGE_MIN_LEAD)
		{
			m_Plan.Kind = PlanKind::Dodge;
			m_Plan.Side = Chance(0.5f) ? 1.0f : -1.0f;
		}
		else if (Chance(m_Params.BlockChance) && (parries || early))
		{
			m_Plan.Kind = PlanKind::Block;
		}
		else if (Chance(m_Params.StepBackChance))
		{
			m_Plan.Kind = PlanKind::StepBack;
		}
	}

	AiBrain::Reaction AiBrain::React(const AiView& self, const AiView& seen, double seenTime, const glm::vec2& toward)
	{
		Reaction reaction;

		const bool swinging = seen.State == FighterState::Attack && seen.Move != nullptr;
		const float predicted = swinging ? PredictedContact(seen, seenTime, self) : k_Infinity;
		if (swinging && seen.SwingId != m_HandledSwing && predicted > AI_LATE_SECONDS)
		{
			m_HandledSwing = seen.SwingId;
			m_Plan = Plan();
			m_PlanAge = 0.0f;
			if (Threatens(self, seen))
				RollPlan(self, seen, predicted);
		}

		if (m_Plan.Kind != PlanKind::None)
		{
			const bool same = swinging && seen.SwingId == m_Plan.Swing;
			const bool whiffed = same && !seen.Hitbox && seen.SecondsToHitbox < -AI_PUNISH_PAST_HITBOX;
			if (!same || whiffed || m_PlanAge > AI_PLAN_MAX_SECONDS)
			{
				if (!same && (m_Plan.Kind == PlanKind::Parry || m_Plan.Kind == PlanKind::Block))
					m_LingerTimer = AI_BLOCK_LINGER;
				m_Plan = Plan();
			}
			else
			{
				reaction.Busy = true;
				switch (m_Plan.Kind)
				{
					case PlanKind::Parry:
						reaction.Block = predicted <= AI_PARRY_LEAD;
						break;
					case PlanKind::Block:
						reaction.Block = true;
						break;
					case PlanKind::Dodge:
						if (!m_Plan.Fired && predicted <= AI_DODGE_LEAD)
						{
							reaction.Dodge = true;
							reaction.Move = Perpendicular(toward) * m_Plan.Side;
							m_Plan.Fired = true;
						}
						break;
					case PlanKind::StepBack:
						reaction.Move = -toward;
						break;
					default:
						break;
				}
			}
		}

		if (!reaction.Busy && m_LingerTimer > 0.0f)
		{
			reaction.Busy = true;
			reaction.Block = true;
		}
		return reaction;
	}

	glm::vec2 AiBrain::HoldGap(float distance, const glm::vec2& toward, float gap, bool strafe) const
	{
		const float error = distance - gap;
		float radial = 0.0f;
		if (std::abs(error) > 0.5f * AI_SPACING_TOLERANCE)
			radial = std::clamp(error * AI_SPACING_GAIN, -AI_BACKOFF_INTENT, m_Params.ApproachIntent);

		glm::vec2 move = toward * radial;
		if (strafe)
			move += Perpendicular(toward) * (m_StrafeSign * AI_STRAFE_INTENT);
		return ClampLength(move, 1.0f);
	}

	float AiBrain::ApproachIntent(float distance, float attackDistance) const
	{
		const float excess = std::max(distance - attackDistance, 0.0f);
		const float slow = std::min(AI_CLOSE_INTENT, m_Params.ApproachIntent);
		return glm::mix(slow, m_Params.ApproachIntent, std::min(excess / AI_CLOSE_RANGE, 1.0f));
	}

	glm::vec2 AiBrain::AvoidWall(const glm::vec2& position, const glm::vec2& move)
	{
		const float edge = GetArenaApothem() - AI_WALL_MARGIN;
		const float radius = glm::length(position);
		if (!(radius > edge) || !(radius > 1.0e-3f))
			return move;

		const float depth = std::min((radius - edge) / AI_WALL_MARGIN, 1.0f);
		return ClampLength(move - position / radius * (AI_WALL_PUSH * depth), 1.0f);
	}

	void AiBrain::DecideOffence(const AiView& self, const AiView& opponent, const AiView& seen, float distance, const glm::vec2& toward, FighterIntent& intent)
	{
		if (m_Params.PunishChance > 0.0f && m_PunishTimer <= 0.0f && seen.SwingId != m_PunishedSwing)
		{
			const bool staggered = seen.State == FighterState::Stagger;
			const bool whiffed = seen.State == FighterState::Attack && !seen.Hitbox && seen.SecondsToHitbox < -AI_PUNISH_PAST_HITBOX;
			const MoveDef* quick = FirstLight(self);
			if ((staggered || whiffed) && quick)
			{
				const float quickDistance = AttackDistance(self, opponent, quick);
				if (distance <= quickDistance)
				{
					m_PunishedSwing = seen.SwingId;
					if (Chance(m_Params.PunishChance))
					{
						m_PunishTimer = AI_PUNISH_COOLDOWN;
						intent.Light = true;
						return;
					}
				}
				else
				{
					intent.Move = AvoidWall(self.Position, toward * ApproachIntent(distance, quickDistance));
					return;
				}
			}
		}

		if (m_Stance == Stance::Retreat && m_StanceTimer <= 0.0f)
		{
			m_Stance = Chance(m_Params.BaitChance) ? Stance::Bait : Stance::Engage;
			m_StanceTimer = AI_BAIT_SECONDS;
		}
		if (m_Stance == Stance::Bait && m_StanceTimer <= 0.0f)
			m_Stance = Stance::Engage;

		const float attackDistance = AttackDistance(self, opponent, m_NextMove);
		glm::vec2 move(0.0f);
		if (m_Stance == Stance::Retreat)
		{
			move = -toward * AI_RETREAT_INTENT;
			if (m_Params.Strafes)
				move += Perpendicular(toward) * (m_StrafeSign * AI_STRAFE_INTENT);
		}
		else if (m_Stance == Stance::Bait)
		{
			move = HoldGap(distance, toward, OpponentReach(opponent, self) + AI_BAIT_MARGIN, false);
		}
		else if (m_AttackTimer <= 0.0f && m_NextMove)
		{
			if (distance <= attackDistance)
			{
				if (m_NextMove->Kind == MoveKind::Heavy)
					intent.Heavy = true;
				else
					intent.Light = true;
				return;
			}
			move = toward * ApproachIntent(distance, attackDistance);
		}
		else if (m_Params.Strafes)
		{
			move = HoldGap(distance, toward, attackDistance + m_Params.GapSlack, true);
		}
		else if (distance > attackDistance)
		{
			move = toward * ApproachIntent(distance, attackDistance);
		}

		intent.Move = AvoidWall(self.Position, ClampLength(move, 1.0f));
	}

}
