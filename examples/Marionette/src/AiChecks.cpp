#include "Checks.h"
#include "AiBrain.h"
#include "CheckReport.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Moveset.h"
#include "ReachTable.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace
{
	using namespace Dingo;

	constexpr float k_LeadIn = 1.0f;
	constexpr float k_OwnAttackSeconds = 1.0f;
	constexpr float k_StepBackMove = 0.5f;

	// A move of the scripted opponent, as its clip times it.
	struct SwingSpec
	{
		const FighterDef* Attacker = nullptr;
		const MoveDef* Move = nullptr;
		float HitboxBegin = 0.0f;
		float HitboxEnd = 0.0f;
		float OneShot = 0.0f;

		// Real seconds from the start of the swing to the hitbox opening.
		float HitSeconds() const { return HitboxBegin / Attacker->Pace; }
	};

	std::optional<SwingSpec> MakeSpec(const GameAssets& assets, const FighterDef& attacker, const MoveDef& move)
	{
		const AnimationClip* clip = assets.GetClip(move.Clip);
		const std::optional<ClipRange> hitbox = clip ? FindRange(*clip, Events::HITBOX) : std::nullopt;
		if (!hitbox)
			return std::nullopt;
		return SwingSpec{ &attacker, &move, hitbox->Begin, hitbox->End, clip->GetDuration() - move.FadeOut };
	}

	// The scripted opponent stands at the origin facing the AI, which stands CHECK_AI_DISTANCE away.
	AiView SwingView(const SwingSpec& spec, float since, uint32_t swingId)
	{
		AiView view;
		view.Def = spec.Attacker;
		view.Position = glm::vec2(0.0f);
		view.Facing = glm::vec2(0.0f, 1.0f);
		view.SwingId = swingId;

		const float pace = spec.Attacker->Pace;
		const float clipTime = since * pace;
		if (since >= 0.0f && clipTime < spec.OneShot)
		{
			view.State = FighterState::Attack;
			view.Move = spec.Move;
			view.Windup = clipTime < spec.HitboxBegin;
			view.Hitbox = clipTime >= spec.HitboxBegin && clipTime <= spec.HitboxEnd;
			view.SecondsToHitbox = (spec.HitboxBegin - clipTime) / pace;
		}
		return view;
	}

	AiView SelfView(const FighterDef& def, FighterState state)
	{
		AiView view;
		view.Def = &def;
		view.Position = glm::vec2(0.0f, CHECK_AI_DISTANCE);
		view.Facing = glm::vec2(0.0f, -1.0f);
		view.State = state;
		return view;
	}

	struct SwingLog
	{
		// Seconds into the swing at the first frame the block went up, and the dodge started; negative if never.
		float Raise = -1.0f;
		float Dodge = -1.0f;
		bool SteppedBack = false;
	};

	// Plays a brain against a scripted opponent that repeats one swing every CHECK_AI_SWING_GAP seconds. The
	// brain's own state follows its intents crudely: a block stays up, a dodge and an attack run their time.
	class Harness
	{
	public:
		Harness(const FighterDef& self, const SwingSpec& spec, bool followAttacks)
			: m_Self(self), m_Spec(spec), m_FollowAttacks(followAttacks)
		{
			m_SelfMove = self.LightChain.empty() ? nullptr : FindMove(self.LightChain[0]);
		}

		template<typename Visit>
		void Run(AiBrain& brain, int swings, Visit&& visit)
		{
			m_Swings.assign(static_cast<size_t>(swings), SwingLog());
			FighterState state = FighterState::Locomotion;
			float dodgeLeft = 0.0f;
			float attackLeft = 0.0f;
			uint32_t selfSwing = 0;

			for (int frame = 1;; ++frame)
			{
				const double time = static_cast<double>(frame) * CHECK_AI_STEP;
				const double sinceLead = time - k_LeadIn;
				const int index = sinceLead < 0.0 ? -1 : static_cast<int>(sinceLead / CHECK_AI_SWING_GAP);
				if (index >= swings)
					break;

				const float since = index < 0 ? -1.0f : static_cast<float>(sinceLead - static_cast<double>(index) * CHECK_AI_SWING_GAP);
				const AiView opponent = SwingView(m_Spec, since, index < 0 ? 0u : static_cast<uint32_t>(index) + 1u);
				AiView self = SelfView(m_Self, state);
				if (state == FighterState::Attack)
				{
					self.Move = m_SelfMove;
					self.SwingId = selfSwing;
					self.ChainIndex = 0;
				}

				const FighterIntent intent = brain.Decide(CHECK_AI_STEP, self, opponent);
				if (index >= 0)
				{
					SwingLog& log = m_Swings[static_cast<size_t>(index)];
					if (intent.Block && log.Raise < 0.0f)
						log.Raise = since;
					if (intent.Dodge && log.Dodge < 0.0f)
						log.Dodge = since;
					log.SteppedBack = log.SteppedBack || intent.Move.y > k_StepBackMove;
				}
				if (intent.Block)
					++m_BlockFrames;
				if (intent.Dodge)
					++m_DodgeFrames;
				visit(intent);

				const bool attacks = m_FollowAttacks && state == FighterState::Locomotion && (intent.Light || intent.Heavy);
				if (attacks)
				{
					m_AttackStarts.push_back(time);
					attackLeft = k_OwnAttackSeconds;
					++selfSwing;
					state = FighterState::Attack;
				}
				else if (attackLeft > 0.0f)
				{
					attackLeft -= CHECK_AI_STEP;
					state = attackLeft > 0.0f ? FighterState::Attack : FighterState::Locomotion;
				}
				else if (intent.Dodge)
				{
					dodgeLeft = CHECK_AI_DODGE_SECONDS;
					state = FighterState::Dodge;
				}
				else if (dodgeLeft > 0.0f)
				{
					dodgeLeft -= CHECK_AI_STEP;
					state = dodgeLeft > 0.0f ? FighterState::Dodge : FighterState::Locomotion;
				}
				else
				{
					state = intent.Block ? FighterState::Block : FighterState::Locomotion;
				}
			}
		}

		const std::vector<SwingLog>& GetSwings() const { return m_Swings; }
		const std::vector<double>& GetAttackStarts() const { return m_AttackStarts; }
		int GetBlockFrames() const { return m_BlockFrames; }
		int GetDodgeFrames() const { return m_DodgeFrames; }

	private:
		const FighterDef& m_Self;
		SwingSpec m_Spec;
		bool m_FollowAttacks;
		const MoveDef* m_SelfMove = nullptr;
		std::vector<SwingLog> m_Swings;
		std::vector<double> m_AttackStarts;
		int m_BlockFrames = 0;
		int m_DodgeFrames = 0;
	};

	std::vector<SwingSpec> KnightSpecs(const GameAssets& assets)
	{
		const FighterDef& knight = GetPlayerDef();
		std::vector<SwingSpec> specs;
		for (const char* name : knight.LightChain)
		{
			if (const MoveDef* move = FindMove(name))
			{
				if (const std::optional<SwingSpec> spec = MakeSpec(assets, knight, *move))
					specs.push_back(*spec);
			}
		}
		if (const MoveDef* heavy = GetHeavyMove(knight))
		{
			if (const std::optional<SwingSpec> spec = MakeSpec(assets, knight, *heavy))
				specs.push_back(*spec);
		}
		return specs;
	}

	void CheckTiers(CheckReport& report)
	{
		report.Check(AI_TIERS.size() == 3 && AI_TIERS[0].ReactionTime == 0.45f && AI_TIERS[1].ReactionTime == 0.30f && AI_TIERS[2].ReactionTime == 0.18f,
			std::format("AI tiers: the Recruit, Veteran and Champion react after {:.2f}, {:.2f} and {:.2f} s", AI_TIERS[0].ReactionTime, AI_TIERS[1].ReactionTime,
				AI_TIERS[2].ReactionTime));
	}

	struct LagResult
	{
		float Worst = 0.0f;
		float Best = 1.0e9f;
		bool Identified = true;
		int Frames = 0;
	};

	LagResult MeasureLag(float delay, const std::vector<float>& steps)
	{
		Perception perception;
		LagResult result;
		double now = 0.0;
		std::vector<double> stamps;
		for (int frame = 0; frame < 900; ++frame)
		{
			now += static_cast<double>(steps[static_cast<size_t>(frame) % steps.size()]);
			AiView view;
			view.SwingId = static_cast<uint32_t>(frame);
			stamps.push_back(now);
			perception.Push(now, view);

			const Perception::Seen seen = perception.Perceive(now, delay);
			const float lag = static_cast<float>(now - seen.Time);
			result.Identified = result.Identified && seen.View && stamps[seen.View->SwingId] == seen.Time;
			if (now - stamps.front() >= static_cast<double>(delay))
			{
				result.Worst = std::max(result.Worst, lag);
				result.Best = std::min(result.Best, lag);
				++result.Frames;
			}
		}
		return result;
	}

	void CheckPerception(CheckReport& report)
	{
		const std::vector<float> fixed = { CHECK_AI_STEP };
		const std::vector<float> irregular = { 1.0f / 60.0f, 1.0f / 30.0f, 1.0f / 120.0f, 1.0f / 45.0f };
		const float irregularMax = 1.0f / 30.0f;

		for (const bool steady : { true, false })
		{
			bool ok = true;
			std::string detail;
			for (const AiTierParams& tier : AI_TIERS)
			{
				const LagResult lag = MeasureLag(tier.ReactionTime, steady ? fixed : irregular);
				const float room = steady ? CHECK_AI_STEP : irregularMax;
				ok = ok && lag.Identified && lag.Best >= tier.ReactionTime - CHECK_AI_LAG_SLACK && lag.Worst < tier.ReactionTime + room + CHECK_AI_LAG_SLACK;
				detail += std::format("{}{} {:.3f} to {:.3f} s", detail.empty() ? "" : "; ", tier.Name, lag.Best, lag.Worst);
			}
			report.Check(ok, std::format("what the AI sees is its tier's REACTION_TIME old at every frame, and at most one frame older ({}): lag per tier {}",
				steady ? "steady 60 Hz" : "irregular 30, 45, 60 and 120 Hz", detail));
		}
	}

	void CheckRecruit(CheckReport& report, const GameAssets& assets)
	{
		const std::vector<SwingSpec> specs = KnightSpecs(assets);
		if (specs.empty())
		{
			report.Check(false, "Recruit: the Knight's moves are there to swing at it");
			return;
		}

		const FighterDef& recruit = GetOpponentDef(1);
		int blocks = 0;
		int dodges = 0;
		int steps = 0;
		float shortest = 1.0e9f;
		float longest = 0.0f;
		size_t starts = 0;
		for (int seed = 1; seed <= CHECK_AI_SEEDS; ++seed)
		{
			for (const SwingSpec& spec : specs)
			{
				AiBrain brain(AI_TIERS[0], static_cast<uint32_t>(seed), nullptr);
				Harness harness(recruit, spec, true);
				harness.Run(brain, 20, [&](const FighterIntent& intent) { steps += (intent.Move.y > k_StepBackMove) ? 1 : 0; });
				blocks += harness.GetBlockFrames();
				dodges += harness.GetDodgeFrames();

				const std::vector<double>& attacks = harness.GetAttackStarts();
				starts += attacks.size();
				for (size_t i = 1; i < attacks.size(); ++i)
				{
					shortest = std::min(shortest, static_cast<float>(attacks[i] - attacks[i - 1]));
					longest = std::max(longest, static_cast<float>(attacks[i] - attacks[i - 1]));
				}
			}
		}

		report.Check(blocks == 0 && dodges == 0 && steps == 0,
			std::format("Recruit never blocks, dodges or steps back: {} blocking frames, {} dodges, {} step-back frames over {} seeds of {} scripted swings each", blocks, dodges, steps,
				CHECK_AI_SEEDS, specs.size()));

		const AiTierParams& tier = AI_TIERS[0];
		report.Check(starts > 0 && shortest >= tier.AttackIntervalMin - CHECK_AI_STEP && longest <= tier.AttackIntervalMax + 3.0f * CHECK_AI_STEP,
			std::format("Recruit attacks on a slow rhythm: {} attacks, {:.2f} to {:.2f} s apart (tuned {:.1f} to {:.1f} s)", starts, shortest, longest,
				tier.AttackIntervalMin, tier.AttackIntervalMax));
	}

	void CheckVeteran(CheckReport& report, const GameAssets& assets)
	{
		const std::vector<SwingSpec> specs = KnightSpecs(assets);
		if (specs.empty())
		{
			report.Check(false, "Veteran: the Knight's moves are there to swing at it");
			return;
		}

		const FighterDef& veteran = GetOpponentDef(2);
		const AiTierParams& tier = AI_TIERS[1];

		// It blocks early, on first sight, so only a swing slow enough to land past the parry window is blocked;
		// a quicker one it would parry by accident, and it steps back from that instead.
		const SwingSpec* slow = &specs.front();
		const SwingSpec* quick = nullptr;
		for (const SwingSpec& spec : specs)
		{
			if (spec.HitSeconds() > slow->HitSeconds())
				slow = &spec;
			if (spec.HitSeconds() >= tier.ReactionTime + CHECK_AI_STEP && (!quick || spec.HitSeconds() < quick->HitSeconds()))
				quick = &spec;
		}

		AiBrain brain(tier, 1, nullptr);
		Harness harness(veteran, *slow, false);
		harness.Run(brain, CHECK_AI_SWINGS, [](const FighterIntent&) {});

		int blocked = 0;
		int backed = 0;
		int neither = 0;
		float earliest = 1.0e9f;
		for (const SwingLog& log : harness.GetSwings())
		{
			blocked += log.Raise >= 0.0f ? 1 : 0;
			backed += log.Raise < 0.0f && log.SteppedBack ? 1 : 0;
			neither += log.Raise < 0.0f && !log.SteppedBack ? 1 : 0;
			if (log.Raise >= 0.0f)
				earliest = std::min(earliest, log.Raise);
		}

		int quickBlocks = 0;
		int quickSteps = 0;
		if (quick)
		{
			AiBrain quickBrain(tier, 1, nullptr);
			Harness quickRun(veteran, *quick, false);
			quickRun.Run(quickBrain, CHECK_AI_SWINGS, [](const FighterIntent&) {});
			for (const SwingLog& log : quickRun.GetSwings())
			{
				quickBlocks += log.Raise >= 0.0f ? 1 : 0;
				quickSteps += log.SteppedBack ? 1 : 0;
			}
		}

		const float fraction = static_cast<float>(blocked) / static_cast<float>(CHECK_AI_SWINGS);
		report.Check(fraction >= CHECK_AI_CHANCE_LOW && fraction <= CHECK_AI_CHANCE_HIGH && neither == 0 && earliest >= tier.ReactionTime - CHECK_AI_STEP
				&& quickBlocks == 0 && (!quick || quickSteps == CHECK_AI_SWINGS),
			std::format("Veteran blocks {:.0f}% of {} slow swings ({}, hit at {:.2f} s; tuned {:.0f}%) and steps back from the rest ({} blocked, {} stepped back, {} did neither); the first block went up "
				"{:.2f} s into a swing (reaction {:.2f} s); against the quick {} it never blocks and steps back every time ({} blocks, {} step-backs)",
				100.0f * fraction, CHECK_AI_SWINGS, slow->Move->Clip, slow->HitSeconds(), 100.0f * tier.BlockChance, blocked, backed, neither, earliest, tier.ReactionTime,
				quick ? quick->Move->Clip : "swing", quickBlocks, quickSteps));
	}

	void CheckChampion(CheckReport& report, const GameAssets& assets)
	{
		const std::vector<SwingSpec> specs = KnightSpecs(assets);
		const FighterDef& champion = GetOpponentDef(3);
		const AiTierParams& tier = AI_TIERS[2];

		const AnimationClip* leftDodge = assets.GetClip(Clips::DODGE_LEFT);
		const std::optional<ClipRange> iframes = leftDodge ? FindRange(*leftDodge, Events::IFRAMES) : std::nullopt;
		const float parryWindow = PARRY_WINDOW_END / champion.Pace;
		if (specs.empty() || !iframes)
		{
			report.Check(false, "Champion: the Knight's moves and the dodge's iframes are there to time against");
			report.Check(false, "Champion: the dodge can be timed");
			return;
		}

		bool parryOk = true;
		bool dodgeOk = true;
		int parried = 0;
		int dodged = 0;
		int unreadable = 0;
		float tightest = 1.0e9f;
		float widest = 0.0f;
		std::string parryDetail;
		for (const SwingSpec& spec : specs)
		{
			AiTierParams parrying = tier;
			parrying.ParryChanceLight = 1.0f;
			parrying.ParryChanceHeavy = 1.0f;
			AiBrain parryBrain(parrying, 7, nullptr);
			Harness parryRun(champion, spec, false);
			parryRun.Run(parryBrain, 12, [](const FighterIntent&) {});

			AiTierParams dodging = tier;
			dodging.ParryChanceLight = 0.0f;
			dodging.ParryChanceHeavy = 0.0f;
			dodging.DodgeChance = 1.0f;
			AiBrain dodgeBrain(dodging, 7, nullptr);
			Harness dodgeRun(champion, spec, false);
			dodgeRun.Run(dodgeBrain, 12, [](const FighterIntent&) {});

			const float hit = spec.HitSeconds();
			const bool readable = hit - tier.ReactionTime >= CHECK_AI_STEP;
			const bool dodgeable = hit - tier.ReactionTime >= AI_DODGE_LEAD + CHECK_AI_STEP;
			if (!readable)
			{
				++unreadable;
				parryDetail += std::format("{}{} (hit {:.2f} s: too quick to read)", parryDetail.empty() ? "" : "; ", spec.Move->Clip, hit);
				continue;
			}

			for (const SwingLog& log : parryRun.GetSwings())
			{
				const float lead = hit - log.Raise;
				const bool inside = log.Raise >= tier.ReactionTime - CHECK_AI_STEP && lead >= 0.0f && lead <= parryWindow - CHECK_AI_STEP;
				parryOk = parryOk && inside;
				parried += inside ? 1 : 0;
				tightest = std::min(tightest, lead);
				widest = std::max(widest, lead);
			}
			parryDetail += std::format("{}{} (hit {:.2f} s)", parryDetail.empty() ? "" : "; ", spec.Move->Clip, hit);

			if (!dodgeable)
				continue;
			for (const SwingLog& log : dodgeRun.GetSwings())
			{
				const float lead = hit - log.Dodge;
				const bool inside = log.Dodge >= tier.ReactionTime - CHECK_AI_STEP && lead >= iframes->Begin / champion.Pace - CHECK_AI_STEP
					&& lead <= iframes->End / champion.Pace;
				dodgeOk = dodgeOk && inside;
				dodged += inside ? 1 : 0;
			}
		}

		report.Check(parryOk && parried > 0 && tightest >= 0.0f && widest <= parryWindow,
			std::format("Champion raises its block so the hit lands inside the parry window: {} parries timed, the block up {:.3f} to {:.3f} s before the hit (window {:.3f} s, "
				"never before it could have seen the windup); {}; {} too quick to read", parried, tightest, widest, parryWindow, parryDetail, unreadable));
		report.Check(dodgeOk && dodged > 0,
			std::format("Champion starts its dodge so the hit lands inside the iframes ({:.2f} to {:.2f} s of the dodge): {} dodges timed", iframes->Begin / champion.Pace,
				iframes->End / champion.Pace, dodged));
	}

	uint64_t HashRun(uint32_t seed, const std::vector<SwingSpec>& specs)
	{
		uint64_t hash = 1469598103934665603ull;
		auto mix = [&](int64_t value) { hash = (hash ^ static_cast<uint64_t>(value)) * 1099511628211ull; };

		const FighterDef& champion = GetOpponentDef(3);
		AiBrain brain(AI_TIERS[2], seed, nullptr);
		Harness harness(champion, specs.front(), true);
		harness.Run(brain, 30, [&](const FighterIntent& intent)
		{
			mix(std::llround(intent.Move.x * 1000.0f));
			mix(std::llround(intent.Move.y * 1000.0f));
			mix((intent.Light ? 1 : 0) | (intent.Heavy ? 2 : 0) | (intent.Dodge ? 4 : 0) | (intent.Block ? 8 : 0));
		});
		return hash;
	}

	void CheckDeterminism(CheckReport& report, const GameAssets& assets)
	{
		const std::vector<SwingSpec> specs = KnightSpecs(assets);
		if (specs.empty())
		{
			report.Check(false, "AI: the Knight's moves are there to run a seeded bout against");
			return;
		}

		const uint64_t first = HashRun(5, specs);
		const uint64_t again = HashRun(5, specs);
		const uint64_t other = HashRun(6, specs);
		report.Check(first == again && first != other,
			std::format("the AI repeats exactly for one seed and differs between seeds ({:016x} and {:016x} for seed 5, {:016x} for seed 6)", first, again, other));
	}

	void CheckReach(CheckReport& report, const ReachTable& reach)
	{
		std::string worst;
		bool sane = true;
		int moves = 0;
		for (const FighterDef& attacker : GetFighterDefs())
		{
			for (const FighterDef& target : GetFighterDefs())
			{
				if (&attacker == &target)
					continue;

				std::vector<const MoveDef*> list;
				for (const char* name : attacker.LightChain)
					list.push_back(FindMove(name));
				list.push_back(GetHeavyMove(attacker));
				list.push_back(&GetRiposteMove());
				for (const MoveDef* move : list)
				{
					if (!move)
						continue;

					const MoveReach& found = reach.Get(attacker, *move, target);
					++moves;
					const bool ok = found.Valid && found.Max >= REACH_SANE_MIN && found.Max <= REACH_SANE_MAX && found.DelayAt(found.Max) <= REACH_SANE_DELAY;
					sane = sane && ok;
					if (!ok)
						worst += std::format("{}{} -> {} {} ({})", worst.empty() ? "" : "; ", attacker.Name, target.Name, move->Clip, found.Valid ? std::format("{:.2f} m", found.Max) : "no touch");
				}
			}
		}
		report.Check(sane && moves > 0, std::format("every move of every fighter touches a standing opponent between {:.1f} and {:.1f} m apart in the sweep the AI learns its reach from ({} swept){}{}",
			REACH_SANE_MIN, REACH_SANE_MAX, moves, worst.empty() ? "" : "; not: ", worst));

		// The scripted duel lands these swings with the pair 1.1 m apart.
		const FighterDef& knight = GetPlayerDef();
		const FighterDef& minion = GetOpponentDef(1);
		bool agrees = true;
		std::string detail;
		auto compare = [&](const FighterDef& attacker, const FighterDef& target, const char* clip)
		{
			const MoveDef* move = FindMove(clip);
			const MoveReach found = move ? reach.Get(attacker, *move, target) : MoveReach();
			const bool inside = found.Valid && found.Min <= DUEL_GAP && found.Max >= DUEL_GAP;
			agrees = agrees && inside;
			detail += std::format("{}{} {}: {:.2f} to {:.2f} m", detail.empty() ? "" : "; ", attacker.Name, clip, found.Min, found.Max);
		};
		compare(minion, knight, "Melee_1H_Attack_Slice_Diagonal");
		for (const char* clip : knight.LightChain)
			compare(knight, minion, clip);
		report.Check(agrees, std::format("the reach table agrees with the scripted duel: each swing it lands at a {:.1f} m gap touches there ({})", DUEL_GAP, detail));
	}
}

namespace Dingo
{

	bool RunAiChecks(const GameAssets& assets, const ReachTable& reach)
	{
		CheckReport report;
		CheckTiers(report);
		CheckPerception(report);
		CheckRecruit(report, assets);
		CheckVeteran(report, assets);
		CheckChampion(report, assets);
		CheckDeterminism(report, assets);
		CheckReach(report, reach);

		if (report.GetFailed() == 0)
			DE_INFO("AI checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		else
			DE_ERROR("AI checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		return report.GetFailed() == 0;
	}

}
