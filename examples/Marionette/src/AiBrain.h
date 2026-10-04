#pragma once
#include "Fighter.h"
#include "FighterIntent.h"
#include "GameTuning.h"
#include "Moveset.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <deque>
#include <limits>
#include <random>

namespace Dingo
{

	class ReachTable;

	// What one fighter shows to the other, and all the AI ever reads of it: its state, what its animation is
	// doing, where it stands. Nothing here comes from the other brain's intent.
	struct AiView
	{
		const FighterDef* Def = nullptr;
		glm::vec2 Position{ 0.0f };
		glm::vec2 Facing{ 0.0f, 1.0f };
		FighterState State = FighterState::Locomotion;
		bool Windup = false;
		bool Hitbox = false;
		bool ComboOpen = false;
		bool CanRiposte = false;
		bool SwingResolved = false;
		// Real seconds until the hitbox opens; negative once it has, infinite off an attack.
		float SecondsToHitbox = std::numeric_limits<float>::infinity();
		const MoveDef* Move = nullptr;
		int ChainIndex = -1;
		uint32_t SwingId = 0;
		float Health = 0.0f;
	};

	AiView MakeAiView(const Fighter& fighter);

	// The opponent as it stood a while ago: a person reacts to what they saw a moment back, and so does the AI.
	class Perception
	{
	public:
		struct Seen
		{
			const AiView* View = nullptr;
			double Time = 0.0;
		};

		void Clear() { m_Entries.clear(); }
		void Push(double now, const AiView& view);
		// The newest snapshot at least `delay` old, or the oldest there is. Valid until the next call.
		Seen Perceive(double now, float delay);

	private:
		struct Entry
		{
			double Time = 0.0;
			AiView View;
		};

		std::deque<Entry> m_Entries;
	};

	// One brain for all three tiers; the tier is a row of AI_TIERS. It decides from a delayed view of the
	// opponent, with AI decision timers of its own; the combat windows it reads (windup, hitbox) are the
	// opponent's clip events, as a person would read them off the animation.
	class AiBrain : public Brain
	{
	public:
		AiBrain(const AiTierParams& params, uint32_t seed, const ReachTable* reach);

		FighterIntent Think(float deltaTime, const Fighter& self, const Fighter& opponent) override;
		void OnEventsChanged() override;
		FighterIntent Decide(float deltaTime, const AiView& self, const AiView& opponent);

		const AiTierParams& GetParams() const { return m_Params; }

	private:
		enum class PlanKind : uint8_t
		{
			None,
			Parry,
			Dodge,
			Block,
			StepBack
		};

		enum class Stance : uint8_t
		{
			Engage,
			Retreat,
			Bait
		};

		struct Plan
		{
			PlanKind Kind = PlanKind::None;
			uint32_t Swing = 0;
			float Side = 1.0f;
			bool Fired = false;
		};

		struct Reaction
		{
			bool Busy = false;
			bool Block = false;
			bool Dodge = false;
			glm::vec2 Move{ 0.0f };
		};

		float Next01();
		bool Chance(float probability) { return Next01() < probability; }
		float Range(float low, float high) { return low + (high - low) * Next01(); }

		void Begin(const AiView& self);
		void TrackState(const AiView& self);
		const MoveDef* ChooseMove(const AiView& self);
		const MoveDef* FirstLight(const AiView& self) const;

		float ReachOf(const AiView& attacker, const MoveDef& move, const AiView& target) const;
		float AttackDistance(const AiView& self, const AiView& opponent, const MoveDef* move) const;
		float OpponentReach(const AiView& opponent, const AiView& self) const;
		bool Threatens(const AiView& self, const AiView& seen) const;
		float PredictedHit(const AiView& seen, double seenTime) const;
		float ContactDelay(const AiView& seen, const AiView& self) const;
		float PredictedContact(const AiView& seen, double seenTime, const AiView& self) const;

		void DecideFree(float deltaTime, const AiView& self, const AiView& opponent, const AiView& seen, double seenTime, float distance,
			const glm::vec2& toward, FighterIntent& intent);
		void DecideAttack(const AiView& self, const AiView& opponent, float distance, FighterIntent& intent);
		Reaction React(const AiView& self, const AiView& seen, double seenTime, const glm::vec2& toward);
		void RollPlan(const AiView& self, const AiView& seen, float predicted);
		void DecideOffence(const AiView& self, const AiView& opponent, const AiView& seen, float distance, const glm::vec2& toward, FighterIntent& intent);
		glm::vec2 HoldGap(float distance, const glm::vec2& toward, float gap, bool strafe) const;
		float ApproachIntent(float distance, float attackDistance) const;
		static glm::vec2 AvoidWall(const glm::vec2& position, const glm::vec2& move);

	private:
		AiTierParams m_Params;
		std::mt19937 m_Rng;
		const ReachTable* m_Reach;
		Perception m_Perception;

		bool m_Started = false;
		double m_Clock = 0.0;
		FighterState m_PrevState = FighterState::Locomotion;

		const MoveDef* m_NextMove = nullptr;
		float m_AttackTimer = 0.0f;
		float m_PunishTimer = 0.0f;
		uint32_t m_PunishedSwing = 0;
		Stance m_Stance = Stance::Engage;
		float m_StanceTimer = 0.0f;
		float m_StrafeSign = 1.0f;
		float m_StrafeTimer = 0.0f;

		Plan m_Plan;
		float m_PlanAge = 0.0f;
		float m_LingerTimer = 0.0f;
		uint32_t m_HandledSwing = 0;

		uint32_t m_ChainSwing = 0;
		bool m_RiposteArmed = false;
		bool m_RipostePlanned = false;
		float m_RiposteWait = 0.0f;
	};

}
