#include "Checks.h"
#include "CheckReport.h"
#include "CheckTuning.h"
#include "Combat.h"
#include "Fighter.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "HitGeometry.h"
#include "Locomotion.h"
#include "MoveTravel.h"
#include "Moveset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{
	using namespace Dingo;

	struct DerivedWindows
	{
		ClipRange Windup;
		ClipRange Hitbox;
		ClipRange Combo;
		bool Valid = false;
	};

	struct DerivedDodge
	{
		ClipRange Dash;
		float DashTravel = 0.0f;
		bool Valid = false;
	};

	float TimeAt(size_t index, float duration)
	{
		return std::min(static_cast<float>(index) * CHECK_SAMPLE_STEP, duration);
	}

	std::vector<glm::vec3> SamplePath(const Skeleton& skeleton, const AnimationClip& clip, int32_t joint)
	{
		Animator animator(&skeleton);
		animator.Play(&clip);

		const float duration = clip.GetDuration();
		const size_t count = std::max<size_t>(1, static_cast<size_t>(std::lround(duration / CHECK_SAMPLE_STEP)));
		std::vector<glm::vec3> path;
		path.reserve(count + 1);
		for (size_t i = 0; i <= count; ++i)
		{
			animator.SetTime(std::min(TimeAt(i, duration), duration - 1.0e-4f));
			animator.Evaluate();
			const glm::vec4 point = animator.GetJointTransform(joint)[3];
			path.emplace_back(point.x, point.y, point.z);
		}
		return path;
	}

	// The speed over a window of DERIVE_SPEED_SPAN samples either side, along `project` of the displacement.
	std::vector<float> Speeds(const std::vector<glm::vec3>& path, const std::function<float(const glm::vec3&)>& project)
	{
		std::vector<float> speeds(path.size(), 0.0f);
		for (size_t i = 0; i < path.size(); ++i)
		{
			const size_t a = i >= static_cast<size_t>(DERIVE_SPEED_SPAN) ? i - DERIVE_SPEED_SPAN : 0;
			const size_t b = std::min(i + DERIVE_SPEED_SPAN, path.size() - 1);
			if (b > a)
				speeds[i] = project(path[b] - path[a]) / (static_cast<float>(b - a) * CHECK_SAMPLE_STEP);
		}
		return speeds;
	}

	// The contiguous run of samples around the peak that stays above `fraction` of it.
	std::pair<size_t, size_t> SpanAroundPeak(const std::vector<float>& values, float fraction)
	{
		const size_t peak = static_cast<size_t>(std::max_element(values.begin(), values.end()) - values.begin());
		const float threshold = fraction * values[peak];
		size_t low = peak;
		size_t high = peak;
		while (low > 0 && values[low - 1] > threshold)
			--low;
		while (high + 1 < values.size() && values[high + 1] > threshold)
			++high;
		return { low, high };
	}

	DerivedWindows DeriveAttack(const AnimationClip& clip, float fadeOut)
	{
		DerivedWindows derived;
		const Skeleton* skeleton = clip.GetSourceSkeleton();
		if (!skeleton)
			return derived;

		const int32_t hand = skeleton->FindJoint(Joints::HAND_RIGHT);
		if (hand == Skeleton::k_InvalidJoint)
			return derived;

		const float duration = clip.GetDuration();
		const std::vector<glm::vec3> handPath = SamplePath(*skeleton, clip, hand);
		const std::vector<float> handSpeed = Speeds(handPath, [](const glm::vec3& d) { return glm::length(d); });
		const auto [hitLow, hitHigh] = SpanAroundPeak(handSpeed, DERIVE_HITBOX_FRACTION);
		const float peak = *std::max_element(handSpeed.begin(), handSpeed.end());
		derived.Hitbox = { TimeAt(hitLow, duration), TimeAt(hitHigh, duration) };

		size_t first = 0;
		while (first < handSpeed.size() && !(handSpeed[first] > DERIVE_WINDUP_FLOOR * peak))
			++first;
		first = std::min(first, handSpeed.size() - 1);
		float windupBegin = TimeAt(first, duration);
		if (derived.Hitbox.Begin - windupBegin < DERIVE_WINDUP_MIN)
			windupBegin = std::max(0.0f, derived.Hitbox.Begin - DERIVE_WINDUP_MIN);
		derived.Windup = { windupBegin, derived.Hitbox.Begin };
		derived.Combo = { derived.Hitbox.End, duration - fadeOut - DERIVE_COMBO_MARGIN };
		derived.Valid = true;
		return derived;
	}

	DerivedDodge DeriveDodge(const AnimationClip& clip)
	{
		DerivedDodge derived;
		const Skeleton* skeleton = clip.GetSourceSkeleton();
		const int32_t hips = skeleton ? skeleton->FindJoint(Joints::HIPS) : Skeleton::k_InvalidJoint;
		if (hips == Skeleton::k_InvalidJoint)
			return derived;

		const float duration = clip.GetDuration();
		const std::vector<glm::vec3> path = SamplePath(*skeleton, clip, hips);
		const std::vector<float> speed = Speeds(path, [](const glm::vec3& d) { return std::hypot(d.x, d.z); });
		const auto [low, high] = SpanAroundPeak(speed, DERIVE_DASH_FRACTION);
		derived.Dash = { TimeAt(low, duration), TimeAt(high, duration) };
		derived.DashTravel = std::hypot(path[high].x - path[low].x, path[high].z - path[low].z);
		derived.Valid = true;
		return derived;
	}

	ClipRange TunedIframes(float duration)
	{
		return { DODGE_IFRAMES_BEGIN * duration, DODGE_IFRAMES_END * duration };
	}

	bool Matches(const std::optional<ClipRange>& authored, const ClipRange& derived)
	{
		return authored && std::abs(authored->Begin - derived.Begin) <= CHECK_WINDOW_TOLERANCE && std::abs(authored->End - derived.End) <= CHECK_WINDOW_TOLERANCE;
	}

	std::string Describe(const std::optional<ClipRange>& range)
	{
		return range ? std::format("[{:.2f},{:.2f}]", range->Begin, range->End) : std::string("none");
	}

	std::string Describe(const ClipRange& range)
	{
		return std::format("[{:.2f},{:.2f}]", range.Begin, range.End);
	}

	bool EndsInside(const std::optional<ClipRange>& range, float limit)
	{
		return !range || range->End <= limit + 1.0e-4f;
	}

	void CheckAttackWindows(CheckReport& report, const GameAssets& assets, const Skeleton* fighter)
	{
		for (const MoveDef& move : GetMoves())
		{
			const AnimationClip* clip = assets.GetClip(move.Clip);
			const DerivedWindows derived = clip ? DeriveAttack(*clip, move.FadeOut) : DerivedWindows();
			if (!derived.Valid)
			{
				report.Check(false, std::format("{}: windows can be derived from the clip", move.Clip));
				report.Check(false, std::format("{}: windows are ordered and end before the one-shot returns", move.Clip));
				continue;
			}

			const std::optional<ClipRange> windup = FindRange(*clip, Events::WINDUP);
			const std::optional<ClipRange> hitbox = FindRange(*clip, Events::HITBOX);
			const std::optional<ClipRange> combo = FindRange(*clip, Events::COMBO);
			const bool wantsCombo = IsChainLink(move.Clip);
			const glm::vec2 net = fighter ? NetHipsTravel(*fighter, *clip, move.FadeOut) : glm::vec2(0.0f);

			DE_INFO("[INFO] {:<34} {:.2f} s, fade-out {:.2f}: windup {} / {}  hitbox {} / {}  combo {} / {}  (authored / derived)  net hips ({:+.3f}, {:+.3f}) m",
				move.Clip, clip->GetDuration(), move.FadeOut, Describe(windup), Describe(derived.Windup), Describe(hitbox), Describe(derived.Hitbox),
				Describe(combo), wantsCombo ? Describe(derived.Combo) : std::string("none"), net.x, net.y);

			const bool comboOk = wantsCombo ? Matches(combo, derived.Combo) : !combo;
			report.Check(Matches(windup, derived.Windup) && Matches(hitbox, derived.Hitbox) && comboOk,
				std::format("{}: the authored windup, hitbox{} match the ones derived from the clip within {:.2f} s",
					move.Clip, wantsCombo ? " and combo" : "", CHECK_WINDOW_TOLERANCE));

			const float limit = clip->GetDuration() - move.FadeOut;
			const bool ordered = windup && hitbox && windup->Begin < hitbox->Begin && windup->End <= hitbox->Begin + CHECK_WINDOW_TOLERANCE
				&& (!combo || combo->Begin >= hitbox->End - CHECK_WINDOW_TOLERANCE);
			report.Check(ordered && EndsInside(windup, limit) && EndsInside(hitbox, limit) && EndsInside(combo, limit),
				std::format("{}: windup before hitbox, combo after it, every window ends before {:.2f} s (the {:.2f} s clip less its {:.2f} s fade-out)",
					move.Clip, limit, clip->GetDuration(), move.FadeOut));
		}
	}

	void CheckDodgeWindows(CheckReport& report, const GameAssets& assets, const Skeleton* fighter)
	{
		for (const char* name : { Clips::DODGE_FORWARD, Clips::DODGE_BACKWARD, Clips::DODGE_LEFT, Clips::DODGE_RIGHT })
		{
			const AnimationClip* clip = assets.GetClip(name);
			const DerivedDodge derived = clip ? DeriveDodge(*clip) : DerivedDodge();
			if (!derived.Valid)
			{
				report.Check(false, std::format("{}: the dash can be derived from the clip", name));
				report.Check(false, std::format("tuning: {}: the iframes can be compared with the tuning", name));
				report.Check(false, std::format("{}: the windows lie inside the clip", name));
				continue;
			}

			const float duration = clip->GetDuration();
			const std::optional<ClipRange> dash = FindRange(*clip, Events::DASH);
			const std::optional<ClipRange> iframes = FindRange(*clip, Events::IFRAMES);
			const ClipRange tuned = TunedIframes(duration);
			const glm::vec2 net = fighter ? NetHipsTravel(*fighter, *clip, DODGE_FADE_OUT) : glm::vec2(0.0f);
			const float dashTravel = dash && fighter ? glm::length(PoseHipsTravel(*fighter, *clip, dash->Begin, dash->End)) : 0.0f;
			DE_INFO("[INFO] {:<34} {:.2f} s, fade-out {:.2f}: dash {} / {} ({:.2f} m, hips in it {:.2f} m)  (authored / derived)  iframes {} / {}  (authored / tuned)  net hips ({:+.3f}, {:+.3f}) m, extra {:.2f} m",
				name, duration, DODGE_FADE_OUT, Describe(dash), Describe(derived.Dash), derived.DashTravel, dashTravel, Describe(iframes), Describe(tuned),
				net.x, net.y, DODGE_EXTRA_DISTANCE);

			report.Check(Matches(dash, derived.Dash) && dashTravel >= CHECK_DASH_MIN_TRAVEL,
				std::format("{}: the authored dash matches the one derived from the clip within {:.2f} s, and the hips move {:.2f} m in it (the direction of the extra distance)",
					name, CHECK_WINDOW_TOLERANCE, dashTravel));

			report.Check(Matches(iframes, tuned),
				std::format("tuning: {}: the authored iframes match {:.0f}% to {:.0f}% of the clip within {:.2f} s (DODGE_IFRAMES_BEGIN and END, not derived)",
					name, 100.0f * DODGE_IFRAMES_BEGIN, 100.0f * DODGE_IFRAMES_END, CHECK_WINDOW_TOLERANCE));

			const float limit = duration - DODGE_FADE_OUT;
			report.Check(dash && iframes && EndsInside(iframes, limit) && dash->End <= duration + 1.0e-4f && iframes->Begin > dash->Begin - CHECK_WINDOW_TOLERANCE,
				std::format("{}: the iframes end before {:.2f} s (the {:.2f} s clip less its {:.2f} s fade-out) and the dash, read from the clip, lies inside it", name, limit, duration, DODGE_FADE_OUT));
		}
	}

	void CheckBlockWindows(CheckReport& report, const GameAssets& assets)
	{
		const AnimationClip* raise = assets.GetClip(Clips::BLOCK_RAISE);
		const std::optional<ClipRange> parry = raise ? FindRange(*raise, Events::PARRY) : std::nullopt;
		report.Check(parry && std::abs(parry->Begin) <= CHECK_WINDOW_TOLERANCE && std::abs(parry->End - PARRY_WINDOW_END) <= CHECK_WINDOW_TOLERANCE,
			std::format("tuning: {}: the parry window is {} (PARRY_WINDOW_END {:.2f} s from the start of the raise, not derived)", Clips::BLOCK_RAISE, Describe(parry), PARRY_WINDOW_END));
	}

	struct Tally
	{
		int ParryBegin = 0;
		int ParryEnd = 0;
		int HitboxBegin = 0;
		int HitboxEnd = 0;
	};

	Animator MakeAnimator(const Skeleton& skeleton, const LocomotionStates& states, float move)
	{
		Animator animator(&skeleton);
		animator.SetLayer(BLOCK_LAYER, AnimationLayer().SetMask(Joints::SPINE).SetWeight(1.0f));
		animator.SetFloat(MOVE_PARAMETER, move);
		animator.Play(states.Blend);
		animator.Update(CHECK_ANIMATOR_STEP);
		return animator;
	}

	void Run(Animator& animator, float seconds, Tally& tally, const std::function<void()>& watch = {})
	{
		const int steps = std::max(1, static_cast<int>(std::lround(seconds / CHECK_ANIMATOR_STEP)));
		for (int i = 0; i < steps; ++i)
		{
			animator.Update(CHECK_ANIMATOR_STEP);
			for (const AnimationEvent& event : animator.GetEventsThisFrame())
			{
				const bool begin = event.Type == AnimationEventType::RangeBegin;
				const bool end = event.Type == AnimationEventType::RangeEnd;
				if (event.Name == Events::PARRY)
				{
					tally.ParryBegin += begin;
					tally.ParryEnd += end;
				}
				else if (event.Name == Events::HITBOX)
				{
					tally.HitboxBegin += begin;
					tally.HitboxEnd += end;
				}
			}
			if (watch)
				watch();
		}
	}

	float MaxPoseDifference(std::span<const JointPose> a, std::span<const JointPose> b)
	{
		float worst = 0.0f;
		for (size_t i = 0; i < a.size() && i < b.size(); ++i)
		{
			const glm::quat other = glm::dot(a[i].Rotation, b[i].Rotation) < 0.0f ? -b[i].Rotation : b[i].Rotation;
			worst = std::max({ worst, std::abs(a[i].Rotation.x - other.x), std::abs(a[i].Rotation.y - other.y), std::abs(a[i].Rotation.z - other.z),
				std::abs(a[i].Rotation.w - other.w) });
			const glm::vec3 gap = glm::abs(a[i].Translation - b[i].Translation);
			worst = std::max({ worst, gap.x, gap.y, gap.z });
		}
		return worst;
	}

	bool FinishRaise(Animator& animator, Tally& tally)
	{
		for (int i = 0; i < CHECK_RAISE_FINISH_STEPS && !animator.IsFinished(BLOCK_LAYER); ++i)
			Run(animator, CHECK_ANIMATOR_STEP, tally);
		return animator.IsFinished(BLOCK_LAYER);
	}

	void CheckParry(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states, const GameAssets& assets)
	{
		const AnimationClip* raise = assets.GetClip(Clips::BLOCK_RAISE);
		const AnimationClip* blocking = assets.GetClip(Clips::BLOCKING);
		const AnimationClip* blockHit = assets.GetClip(Clips::BLOCK_HIT);

		Animator animator = MakeAnimator(skeleton, states, WALK_SPEED);
		bool every = true;
		int raises = 0;
		for (int i = 0; i < CHECK_PARRY_RAISES; ++i)
		{
			Tally walk;
			Run(animator, CHECK_PARRY_RUN_IN + CHECK_PARRY_RUN_IN_STEP * static_cast<float>(i), walk);

			Tally tally;
			animator.Play(AnimationState::Clip(raise).SetLoop(false), BLOCK_RAISE_FADE, BLOCK_LAYER);
			Run(animator, CHECK_PARRY_WATCH, tally);
			every = every && tally.ParryBegin == 1 && tally.ParryEnd == 1 && !animator.IsEventActive(Events::PARRY);
			++raises;

			if (i % 2 == 1)
			{
				FinishRaise(animator, tally);
				animator.Play(blocking, 0.0f, BLOCK_LAYER);
				Run(animator, CHECK_BLOCK_SETTLE, tally);
			}
			animator.Stop(BLOCK_LOWER_FADE, BLOCK_LAYER);
			Run(animator, CHECK_BLOCK_LOWER_WATCH, tally);
			every = every && tally.ParryBegin == 1 && tally.ParryEnd == 1;
		}
		report.Check(every, std::format("parry opens and closes once on each of {} block raises, from a walk and after a hold, on the {} layer", raises, BLOCK_LAYER));

		Tally held;
		animator.Play(AnimationState::Clip(raise).SetLoop(false), BLOCK_RAISE_FADE, BLOCK_LAYER);
		Run(animator, CHECK_RAISE_WATCH, held);
		const int afterRaise = held.ParryBegin;
		const bool finished = FinishRaise(animator, held);
		animator.Play(blocking, 0.0f, BLOCK_LAYER);
		Run(animator, CHECK_BLOCK_HOLD, held);
		animator.PlayOneShot(blockHit, BLOCK_HIT_FADE_IN, BLOCK_HIT_FADE_OUT, BLOCK_LAYER);
		Run(animator, CHECK_BLOCK_HIT_WATCH, held);
		report.Check(finished && afterRaise == 1 && held.ParryBegin == 1,
			std::format("holding the block ({}) and being hit on it ({}) opens no further parry window ({} in all)", Clips::BLOCKING, Clips::BLOCK_HIT, held.ParryBegin));
	}

	void CheckHitReaction(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states, const GameAssets& assets)
	{
		const AnimationClip* attack = assets.GetClip(GetPlayerDef().LightChain[0]);
		const AnimationClip* react = assets.GetClip(Clips::HIT_REACT);
		const AnimationClip* idle = assets.GetClip(Clips::IDLE);
		const std::optional<ClipRange> hitbox = attack ? FindRange(*attack, Events::HITBOX) : std::nullopt;

		for (const bool inside : { false, true })
		{
			Animator animator = MakeAnimator(skeleton, states, 0.0f);
			Tally tally;
			animator.PlayOneShot(attack, ATTACK_FADE_IN, ATTACK_FADE_OUT, 0);

			const float at = hitbox ? (inside ? 0.5f * (hitbox->Begin + hitbox->End) : 0.5f * hitbox->Begin) : 0.0f;
			Run(animator, at, tally);
			const bool attacking = animator.GetCurrentClip(0) == attack;
			const int opened = tally.HitboxBegin;

			animator.PlayOneShot(react, HIT_REACT_FADE_IN, HIT_REACT_FADE_OUT, 0);
			bool backToAttack = false;
			for (int i = 0; i < CHECK_ONE_SHOT_STEPS; ++i)
			{
				Run(animator, CHECK_ANIMATOR_STEP, tally);
				backToAttack = backToAttack || animator.GetCurrentClip(0) == attack;
				if (!animator.IsOneShotPlaying(0))
					break;
			}
			const bool returned = !animator.IsOneShotPlaying(0);
			Run(animator, CHECK_RETURN_WATCH, tally, [&] { backToAttack = backToAttack || animator.GetCurrentClip(0) == attack; });

			report.Check(attacking && opened == (inside ? 1 : 0) && returned && !backToAttack && animator.GetCurrentClip(0) == idle
					&& tally.HitboxBegin == opened && tally.HitboxEnd == opened && !animator.IsEventActive(Events::HITBOX),
				std::format("a {} one-shot over {} {} returns to {} and never to the attack; its hitbox {} (opened {}, closed {})",
					Clips::HIT_REACT, GetPlayerDef().LightChain[0], inside ? "inside its hitbox" : "during its windup", Clips::IDLE,
					inside ? "closes within its fade-in and stays shut" : "never opens", tally.HitboxBegin, tally.HitboxEnd));
		}
	}

	void CheckDeath(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states, const GameAssets& assets)
	{
		const AnimationClip* raise = assets.GetClip(Clips::BLOCK_RAISE);
		for (const FighterDef& fighter : GetFighterDefs())
		{
			if (fighter.Id == FighterId::Warrior)
				continue;

			const AnimationClip* death = assets.GetClip(fighter.Death);
			Animator animator = MakeAnimator(skeleton, states, WALK_SPEED);
			Tally tally;
			Run(animator, CHECK_DEATH_WALK, tally);
			animator.Play(AnimationState::Clip(raise).SetLoop(false), BLOCK_RAISE_FADE, BLOCK_LAYER);
			Run(animator, CHECK_DEATH_RAISE, tally);
			animator.Stop(0.0f, BLOCK_LAYER);
			animator.Play(AnimationState::Clip(death).SetLoop(false), DEATH_FADE);

			bool onlyDeath = true;
			const float settle = death ? death->GetDuration() + CHECK_DEATH_MARGIN : CHECK_DEATH_SETTLE;
			Run(animator, settle, tally);
			const std::vector<JointPose> earlier(animator.GetLocalPoses().begin(), animator.GetLocalPoses().end());
			Run(animator, CHECK_DEATH_SETTLE, tally, [&] { onlyDeath = onlyDeath && animator.GetCurrentClip(0) == death; });
			const float drift = MaxPoseDifference(earlier, animator.GetLocalPoses());

			report.Check(death && drift <= CHECK_POSE_HOLD_TOLERANCE && onlyDeath && animator.IsFinished(0) && !animator.IsOneShotPlaying(0)
					&& animator.GetCurrentClip(BLOCK_LAYER) == nullptr,
				std::format("{} ({}) holds its last pose: it moves {:.1e} in the next {:.0f} s, stays the current clip, and the block layer is clear",
					fighter.Death, fighter.Name, drift, CHECK_DEATH_SETTLE));
		}
	}

	void CheckRiposte(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states, const GameAssets& assets)
	{
		const MoveDef& riposte = GetRiposteMove();
		const AnimationClip* attack = assets.GetClip(riposte.Clip);
		const AnimationClip* raise = assets.GetClip(Clips::BLOCK_RAISE);
		const AnimationClip* blocking = assets.GetClip(Clips::BLOCKING);

		Animator animator = MakeAnimator(skeleton, states, 0.0f);
		Tally tally;
		animator.Play(AnimationState::Clip(raise).SetLoop(false), BLOCK_RAISE_FADE, BLOCK_LAYER);
		const bool finished = FinishRaise(animator, tally);
		animator.Play(blocking, 0.0f, BLOCK_LAYER);
		Run(animator, CHECK_RIPOSTE_HOLD, tally);

		const int parries = tally.ParryBegin;
		animator.Stop(0.0f, BLOCK_LAYER);
		animator.PlayOneShot(attack, riposte.FadeIn, riposte.FadeOut, 0);
		const bool clear = animator.GetCurrentClip(BLOCK_LAYER) == nullptr;
		Run(animator, riposte.FadeIn + attack->GetDuration() - riposte.FadeOut, tally);
		report.Check(finished && clear && tally.HitboxBegin == 1 && tally.HitboxEnd == 1 && tally.ParryBegin == parries,
			std::format("a {} one-shot after a block (layer {} stopped first) opens its hitbox once, closes it, and opens no parry", riposte.Clip, BLOCK_LAYER));
	}

	void CheckBlockHit(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states, const GameAssets& assets)
	{
		const AnimationClip* raise = assets.GetClip(Clips::BLOCK_RAISE);
		const AnimationClip* blocking = assets.GetClip(Clips::BLOCKING);
		const AnimationClip* hit = assets.GetClip(Clips::BLOCK_HIT);
		const AnimationClip* idle = assets.GetClip(Clips::IDLE);

		Animator animator = MakeAnimator(skeleton, states, 0.0f);
		Tally tally;
		animator.Play(AnimationState::Clip(raise).SetLoop(false), BLOCK_RAISE_FADE, BLOCK_LAYER);
		FinishRaise(animator, tally);
		animator.Play(blocking, 0.0f, BLOCK_LAYER);
		Run(animator, CHECK_BLOCK_SETTLE, tally);

		animator.PlayOneShot(hit, BLOCK_HIT_FADE_IN, BLOCK_HIT_FADE_OUT, BLOCK_LAYER);
		const bool playing = animator.GetCurrentClip(BLOCK_LAYER) == hit && animator.IsOneShotPlaying(BLOCK_LAYER);
		bool baseKeepsPlaying = true;
		for (int i = 0; i < CHECK_ONE_SHOT_STEPS && animator.IsOneShotPlaying(BLOCK_LAYER); ++i)
			Run(animator, CHECK_ANIMATOR_STEP, tally, [&] { baseKeepsPlaying = baseKeepsPlaying && animator.GetCurrentClip(0) == idle; });
		const bool returned = !animator.IsOneShotPlaying(BLOCK_LAYER) && animator.GetCurrentClip(BLOCK_LAYER) == blocking;
		Run(animator, CHECK_BLOCK_HIT_AFTER, tally);
		report.Check(playing && returned && animator.GetCurrentClip(BLOCK_LAYER) == blocking && baseKeepsPlaying,
			std::format("{} played on layer {} returns to {} (and layer 0 keeps its own clip meanwhile)", Clips::BLOCK_HIT, BLOCK_LAYER, Clips::BLOCKING));
	}

	constexpr float k_Unmeasured = std::numeric_limits<float>::max();
	constexpr float k_ProbeDamage = 1.0f;

	struct CarryRun
	{
		float MaxJerk = 0.0f;
		float Owed = 0.0f;
		float Unpaid = 0.0f;
		bool Ran = false;
	};

	// A dodge cut by a hit at the middle of its dash, in the order a fighter does it: the capsule pays its share,
	// the combat pass interrupts, the animator updates. It owes no extra distance, so the pose and the carry alone
	// move the hips. Follows the hips' position in the world (the capsule plus the pose) from the frame of the hit
	// on and reports the largest change in its step from one frame to the next: a pop shows as a step that reverses
	// at once, while the dodge's own speed does not. `carry` false is the control, with nothing owed and the
	// reaction cutting in at once. MoveTravel is driven by hand here; Fighter's use of it is CheckFighterCarry's.
	CarryRun RunCarry(const Skeleton& skeleton, const LocomotionStates& states, const AnimationClip& dodge, const AnimationClip& react, bool carry)
	{
		CarryRun run;
		const int32_t hips = skeleton.FindJoint(Joints::HIPS);
		const std::optional<ClipRange> dash = FindRange(dodge, Events::DASH);
		if (hips == Skeleton::k_InvalidJoint || !dash)
			return run;

		const float fadeIn = carry ? HIT_REACT_FADE_IN : 0.0f;
		const float cutAt = 0.5f * (dash->Begin + dash->End);
		Animator animator = MakeAnimator(skeleton, states, 0.0f);
		MoveTravel travel;
		animator.PlayOneShot(&dodge, DODGE_FADE_IN, DODGE_FADE_OUT, 0);
		travel.Begin(&skeleton, dodge, 1.0f, DODGE_FADE_OUT, 0.0f);

		glm::vec2 capsule(0.0f);
		glm::vec2 previous(0.0f);
		glm::vec2 lastStep(0.0f);
		bool cut = false;
		int framesAfterCut = 0;
		for (int i = 0; i < CHECK_ONE_SHOT_STEPS && framesAfterCut < CHECK_CARRY_FRAMES; ++i)
		{
			capsule += travel.Step(CHECK_ANIMATOR_STEP);
			if (!cut && animator.GetTime(0) >= cutAt)
			{
				cut = true;
				if (carry)
				{
					travel.Release(animator.GetTime(0), fadeIn);
					run.Owed = glm::length(travel.GetCarry());
					capsule += travel.Step(CHECK_ANIMATOR_STEP);
				}
				else
				{
					travel = MoveTravel();
				}
				animator.PlayOneShot(&react, fadeIn, HIT_REACT_FADE_OUT, 0);
			}
			animator.Update(CHECK_ANIMATOR_STEP);

			const glm::vec4 hip = animator.GetJointTransform(hips)[3];
			const glm::vec2 world = capsule + glm::vec2(hip.x, hip.z);
			const glm::vec2 step = world - previous;
			if (cut)
			{
				run.MaxJerk = std::max(run.MaxJerk, glm::length(step - lastStep));
				++framesAfterCut;
			}
			lastStep = step;
			previous = world;
		}
		run.Unpaid = glm::length(travel.GetCarry());
		run.Ran = cut;
		return run;
	}

	void CheckTravelCarry(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states, const GameAssets& assets)
	{
		const AnimationClip* react = assets.GetClip(Clips::HIT_REACT);
		bool carried = react != nullptr;
		bool detected = react != nullptr;
		float worst = 0.0f;
		float weakestControl = k_Unmeasured;
		float owedLeast = k_Unmeasured;
		for (const char* name : { Clips::DODGE_FORWARD, Clips::DODGE_BACKWARD, Clips::DODGE_LEFT, Clips::DODGE_RIGHT })
		{
			const AnimationClip* dodge = assets.GetClip(name);
			if (!dodge || !react)
			{
				carried = false;
				continue;
			}

			const CarryRun with = RunCarry(skeleton, states, *dodge, *react, true);
			const CarryRun without = RunCarry(skeleton, states, *dodge, *react, false);
			DE_INFO("[INFO] {:<16} cut at mid-dash owes {:.3f} m: the hips' step changes by at most {:.3f} m carried, {:.3f} m without the carry",
				name, with.Owed, with.MaxJerk, without.MaxJerk);
			carried = carried && with.Ran && with.MaxJerk <= CHECK_CARRY_JERK_MAX && with.Unpaid < CHECK_GEOMETRY_TOLERANCE;
			detected = detected && without.Ran && without.MaxJerk > CHECK_CARRY_JERK_MAX;
			worst = std::max(worst, with.MaxJerk);
			weakestControl = std::min(weakestControl, without.MaxJerk);
			owedLeast = std::min(owedLeast, with.Owed);
		}
		report.Check(carried && detected,
			std::format("a dodge cut by a hit at mid-dash keeps the hips continuous: their step changes by at most {:.3f} m from one frame to the next over the {} frames after it (limit {:.2f} m) "
				"and the {:.2f} m or more it owed is paid in full; without the carry, with the reaction cutting in at once, it changes by at least {:.3f} m, so the check sees a snap",
				worst, CHECK_CARRY_FRAMES, CHECK_CARRY_JERK_MAX, owedLeast, weakestControl));
	}

	struct TravelErrors
	{
		float Owed = 0.0f;
		float Paid = 0.0f;
	};

	glm::vec2 PayFor(MoveTravel& travel, int steps)
	{
		glm::vec2 paid(0.0f);
		for (int i = 0; i < steps; ++i)
			paid += travel.Step(CHECK_ANIMATOR_STEP);
		return paid;
	}

	int StepsFor(float seconds)
	{
		return std::max(1, static_cast<int>(std::lround(seconds / CHECK_ANIMATOR_STEP)));
	}

	// Starts `next` on `travel`, which holds a carry that HIT_REACT_FADE_IN pays off, and steps past it. `own` is what
	// the same steps pay on a travel that owes nothing, so the difference is the carry.
	glm::vec2 PayNext(MoveTravel& travel, const Skeleton& skeleton, const AnimationClip& next, glm::vec2& own)
	{
		MoveTravel alone;
		for (MoveTravel* target : { &travel, &alone })
			target->Begin(&skeleton, next, CHECK_TRAVEL_SCALE, DODGE_FADE_OUT, DODGE_EXTRA_DISTANCE);

		const int steps = StepsFor(HIT_REACT_FADE_IN) + 1;
		own = PayFor(alone, steps);
		return PayFor(travel, steps);
	}

	std::optional<TravelErrors> ProbeBeforeReturn(const Skeleton& skeleton, const AnimationClip& dodge, const AnimationClip& next)
	{
		const std::optional<ClipRange> dash = FindRange(dodge, Events::DASH);
		if (!dash || !(dash->End > dash->Begin))
			return std::nullopt;

		MoveTravel travel;
		travel.Begin(&skeleton, dodge, CHECK_TRAVEL_SCALE, DODGE_FADE_OUT, DODGE_EXTRA_DISTANCE);
		const glm::vec2 paidBefore = PayFor(travel, StepsFor(0.5f * (dash->Begin + dash->End)));
		const float elapsed = travel.GetClock();
		if (elapsed >= dodge.GetDuration() - DODGE_FADE_OUT || !(glm::length(travel.GetDashStep()) > CHECK_TRAVEL_TOLERANCE))
			return std::nullopt;

		TravelErrors errors;
		const float covered = std::clamp((elapsed - dash->Begin) / (dash->End - dash->Begin), 0.0f, 1.0f);
		const float dashError = glm::length(paidBefore - travel.GetDashStep() * covered);

		const glm::vec2 owed = PoseHipsTravel(skeleton, dodge, 0.0f, elapsed) * CHECK_TRAVEL_SCALE;
		travel.Release(elapsed, HIT_REACT_FADE_IN);
		const glm::vec2 carry = travel.GetCarry();
		errors.Owed = glm::length(carry - owed);

		glm::vec2 own;
		const glm::vec2 paid = PayNext(travel, skeleton, next, own);
		errors.Paid = std::max(dashError, glm::length(paid - (own + carry)) + glm::length(travel.GetCarry()));
		return errors;
	}

	std::optional<TravelErrors> ProbeInReturn(const Skeleton& skeleton, const AnimationClip& dodge, const AnimationClip& next)
	{
		const float returnBegin = std::max(dodge.GetDuration() - DODGE_FADE_OUT, 0.0f);
		MoveTravel travel;
		travel.Begin(&skeleton, dodge, CHECK_TRAVEL_SCALE, DODGE_FADE_OUT, DODGE_EXTRA_DISTANCE);
		const glm::vec2 paidBefore = PayFor(travel, StepsFor(returnBegin + 0.5f * DODGE_FADE_OUT));
		const float elapsed = travel.GetClock();
		const float fraction = (elapsed - returnBegin) / DODGE_FADE_OUT;
		if (!(fraction > 0.0f && fraction < 1.0f) || !(glm::length(travel.GetReturnStep()) > CHECK_TRAVEL_TOLERANCE))
			return std::nullopt;

		TravelErrors errors;
		const glm::vec2 whole = travel.GetDashStep() + travel.GetReturnStep();
		travel.Release(elapsed, HIT_REACT_FADE_IN);
		const glm::vec2 carry = travel.GetCarry();
		errors.Owed = glm::length(carry - travel.GetReturnStep() * (1.0f - fraction));

		glm::vec2 own;
		const glm::vec2 paid = PayNext(travel, skeleton, next, own);
		errors.Paid = std::max(glm::length(paidBefore + carry - whole), glm::length(paid - (own + carry)) + glm::length(travel.GetCarry()));
		return errors;
	}

	std::optional<TravelErrors> ProbeChained(const Skeleton& skeleton, const AnimationClip& dodge, const MoveDef& move, const AnimationClip& attack, const AnimationClip& next)
	{
		const std::optional<ClipRange> dash = FindRange(dodge, Events::DASH);
		if (!dash)
			return std::nullopt;

		MoveTravel travel;
		travel.Begin(&skeleton, dodge, CHECK_TRAVEL_SCALE, DODGE_FADE_OUT, DODGE_EXTRA_DISTANCE);
		PayFor(travel, StepsFor(0.5f * (dash->Begin + dash->End)));
		travel.Release(travel.GetClock(), CHECK_TRAVEL_SPREAD);
		const glm::vec2 first = travel.GetCarry();
		glm::vec2 paid = PayFor(travel, 1);

		travel.Begin(&skeleton, attack, CHECK_TRAVEL_SCALE, move.FadeOut, 0.0f);
		paid += PayFor(travel, CHECK_TRAVEL_SPREAD_STEPS);
		const glm::vec2 inFlight = travel.GetCarry();
		const float elapsed = travel.GetClock();
		if (!(glm::length(inFlight) > CHECK_TRAVEL_TOLERANCE) || elapsed >= attack.GetDuration() - move.FadeOut)
			return std::nullopt;

		TravelErrors errors;
		const glm::vec2 attackPose = PoseHipsTravel(skeleton, attack, 0.0f, elapsed) * CHECK_TRAVEL_SCALE;
		travel.Release(elapsed, HIT_REACT_FADE_IN);
		errors.Owed = glm::length(travel.GetCarry() - (inFlight + attackPose));

		glm::vec2 own;
		paid += PayNext(travel, skeleton, next, own);
		errors.Paid = glm::length(paid - (first + attackPose + own)) + glm::length(travel.GetCarry());
		return errors;
	}

	void CheckTravelConservation(CheckReport& report, const Skeleton& skeleton, const GameAssets& assets)
	{
		const MoveDef* move = FindMove(GetPlayerDef().LightChain[0]);
		const AnimationClip* attack = move ? assets.GetClip(move->Clip) : nullptr;
		const AnimationClip* next = assets.GetClip(Clips::DODGE_FORWARD);

		std::array<bool, 3> ran = { attack && next, attack && next, attack && next };
		std::array<TravelErrors, 3> worst;
		for (const char* name : { Clips::DODGE_FORWARD, Clips::DODGE_BACKWARD, Clips::DODGE_LEFT, Clips::DODGE_RIGHT })
		{
			const AnimationClip* dodge = assets.GetClip(name);
			if (!dodge || !attack || !next)
			{
				ran = { false, false, false };
				break;
			}

			const std::array<std::optional<TravelErrors>, 3> probes = { ProbeBeforeReturn(skeleton, *dodge, *next), ProbeInReturn(skeleton, *dodge, *next),
				ProbeChained(skeleton, *dodge, *move, *attack, *next) };
			for (size_t i = 0; i < probes.size(); ++i)
			{
				ran[i] = ran[i] && probes[i].has_value();
				if (probes[i])
				{
					worst[i].Owed = std::max(worst[i].Owed, probes[i]->Owed);
					worst[i].Paid = std::max(worst[i].Paid, probes[i]->Paid);
				}
			}
		}

		const std::array<const char*, 3> subjects = {
			"a dodge released at mid-dash owes the pose travel so far, has paid its dash share as it went, and the next move pays the carry on top of its own",
			"a dodge released inside its return owes the rest of the return, so what it paid plus the carry is the dash share plus the whole return, and the next move pays the carry on top of its own",
			"a move released while an earlier carry is still in flight owes what was left plus its own pose travel, and the whole chain pays the sum once"
		};
		for (size_t i = 0; i < subjects.size(); ++i)
		{
			report.Check(ran[i] && worst[i].Owed <= CHECK_TRAVEL_TOLERANCE && worst[i].Paid <= CHECK_TRAVEL_TOLERANCE,
				std::format("MoveTravel: {} (worst error {:.1e} m owed, {:.1e} m paid, on the four dodges at scale {:.2f})", subjects[i], worst[i].Owed, worst[i].Paid, CHECK_TRAVEL_SCALE));
		}
	}

	void StepFighter(Fighter& fighter, const FighterDef& def)
	{
		fighter.Update(CHECK_ANIMATOR_STEP, nullptr);
		if (Animator* animator = fighter.GetAnimator())
			animator->Update(CHECK_ANIMATOR_STEP * def.Pace);
	}

	void CheckFighterCarry(CheckReport& report, const GameAssets& assets, const Skeleton* skeleton)
	{
		struct Interruption
		{
			void (*Apply)(Fighter&);
			FighterState State;
		};
		const Interruption interruptions[] = {
			{ [](Fighter& fighter) { fighter.TakeHit(k_ProbeDamage); }, FighterState::HitReact },
			{ [](Fighter& fighter) { fighter.Stagger(); }, FighterState::Stagger }
		};

		const FighterDef& def = GetPlayerDef();
		const GameAudio audio{ GameSounds() };
		Scene scene("Marionette carry check");
		const double now = 0.0;
		const FighterContext context{ scene, assets, audio, now };
		FighterSpawn spawn;
		spawn.Controlled = false;

		bool banked = skeleton != nullptr;
		float worst = 0.0f;
		float least = k_Unmeasured;
		for (const Interruption& interruption : interruptions)
		{
			Fighter fighter(context, def, spawn);
			fighter.StartLocomotion(0.0f);
			FighterIntent dodge;
			dodge.Dodge = true;
			fighter.SetIntent(dodge);
			StepFighter(fighter, def);
			fighter.SetIntent(FighterIntent());

			const AnimationClip* clip = fighter.GetLayerClip(0);
			const std::optional<ClipRange> dash = clip ? FindRange(*clip, Events::DASH) : std::nullopt;
			if (!skeleton || !dash || fighter.GetState() != FighterState::Dodge)
			{
				banked = false;
				continue;
			}

			const float cutAt = 0.5f * (dash->Begin + dash->End);
			for (int i = 0; i < CHECK_ONE_SHOT_STEPS && fighter.GetLayerTime(0) < cutAt; ++i)
				StepFighter(fighter, def);

			// Apply never runs here, so none of the carry is paid yet and all of it shows.
			const float clipTime = fighter.GetLayerTime(0);
			const glm::vec2 before = fighter.GetTravelCarry();
			interruption.Apply(fighter);
			const glm::vec2 owed = PoseHipsTravel(*skeleton, *clip, 0.0f, std::min(clipTime, clip->GetDuration() - DODGE_FADE_OUT)) * def.Scale;
			const float error = glm::length(fighter.GetTravelCarry() - owed);

			banked = banked && fighter.GetState() == interruption.State && glm::length(before) <= CHECK_TRAVEL_TOLERANCE && glm::length(owed) >= CHECK_FIGHTER_CARRY_MIN
				&& error <= CHECK_TRAVEL_TOLERANCE;
			worst = std::max(worst, error);
			least = std::min(least, glm::length(owed));
		}
		report.Check(banked,
			std::format("a Fighter dodging at mid-dash that takes a hit or is staggered banks the {:.2f} m or more of pose travel it owed as its carry (worst error {:.1e} m, none before) and takes the reaction's state",
				least, worst));
	}

	void CheckParryCooldown(CheckReport& report, const GameAssets& assets)
	{
		const FighterDef& def = GetPlayerDef();
		const GameAudio audio{ GameSounds() };
		Scene scene("Marionette parry cooldown check");
		const double now = 0.0;
		const FighterContext context{ scene, assets, audio, now };
		FighterSpawn spawn;
		spawn.Controlled = false;
		Fighter fighter(context, def, spawn);
		fighter.StartLocomotion(0.0f);

		FighterIntent raise;
		raise.Block = true;
		const FighterIntent lower;
		auto step = [&](const FighterIntent& intent, int frames, const std::function<void()>& watch = {})
		{
			fighter.SetIntent(intent);
			for (int i = 0; i < frames; ++i)
			{
				StepFighter(fighter, def);
				if (watch)
					watch();
			}
		};

		step(raise, 1);
		const bool firstOpen = fighter.GetState() == FighterState::Block && fighter.IsParryOpen() && !fighter.IsParryDenied();

		const int held = StepsFor(CHECK_PARRY_FIRST_HOLD);
		step(raise, held - 1);
		step(lower, 1);
		const bool lowered = fighter.GetState() == FighterState::Locomotion;
		step(lower, std::max(StepsFor(CHECK_PARRY_RERAISE_GAP) - held - 1, 0));

		int frames = 0;
		int windowFrames = 0;
		bool denied = true;
		step(raise, StepsFor(CHECK_PARRY_WATCH), [&]
		{
			++frames;
			windowFrames += fighter.IsWindowActive(Events::PARRY) ? 1 : 0;
			denied = denied && fighter.GetState() == FighterState::Block && fighter.IsParryDenied() && !fighter.IsParryOpen();
		});

		step(lower, StepsFor(BLOCK_PARRY_COOLDOWN + CHECK_PARRY_COOLED_MARGIN));
		step(raise, 1);
		const bool cooled = fighter.GetState() == FighterState::Block && fighter.IsParryOpen() && !fighter.IsParryDenied();

		report.Check(firstOpen && lowered && denied && windowFrames > 0 && cooled,
			std::format("a Fighter that raises its block {:.1f} s after the previous raise (lowered in between) gets no parry window: none of {} frames reports one, {} of them inside the clip's own {} range; "
				"a raise {:.1f} s after lowering parries again (BLOCK_PARRY_COOLDOWN {:.1f} s)",
				CHECK_PARRY_RERAISE_GAP, frames, windowFrames, Events::PARRY, BLOCK_PARRY_COOLDOWN + CHECK_PARRY_COOLED_MARGIN, BLOCK_PARRY_COOLDOWN));
	}

	void CheckGeometry(CheckReport& report, const GameAssets& assets)
	{
		const glm::vec3 a(-1.0f, 0.0f, 0.0f);
		const glm::vec3 b(1.0f, 0.0f, 0.0f);
		const float beside = DistanceToSegment(glm::vec3(0.0f, 1.0f, 0.0f), a, b);
		const float beyond = DistanceToSegment(glm::vec3(3.0f, 0.0f, 0.0f), a, b);
		const float degenerate = DistanceToSegment(glm::vec3(0.0f, 2.0f, 0.0f), a, a);
		report.Check(std::abs(beside - 1.0f) < CHECK_GEOMETRY_TOLERANCE && std::abs(beyond - 2.0f) < CHECK_GEOMETRY_TOLERANCE && std::abs(degenerate - std::sqrt(5.0f)) < CHECK_GEOMETRY_TOLERANCE,
			std::format("segment distance: {:.3f} beside the segment, {:.3f} past its end, {:.3f} from a zero-length one", beside, beyond, degenerate));

		const WorldSphere target{ glm::vec3(0.0f, 0.5f, 0.0f), 0.3f };
		const SweptSphere through{ glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f), 0.25f };
		const SweptSphere before{ glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(-1.5f, 0.0f, 0.0f), 0.25f };
		const SweptSphere atStart{ through.From, through.From, through.Radius };
		const SweptSphere atEnd{ through.To, through.To, through.Radius };
		report.Check(Touches(through, target) && !Touches(before, target) && !Touches(atStart, target) && !Touches(atEnd, target),
			"a sphere that jumps across a target in one frame touches it, though neither end of the sweep does");

		std::string weapons;
		bool sensible = true;
		for (const FighterDef& fighter : GetFighterDefs())
		{
			const Model* weapon = fighter.RightWeapon ? assets.GetModel(fighter.RightWeapon) : nullptr;
			const BladeAxis blade = weapon ? MeasureBlade(*weapon) : BladeAxis();
			sensible = sensible && blade.Length > CHECK_BLADE_MIN_LENGTH && blade.Length < CHECK_BLADE_MAX_LENGTH && blade.HalfWidth > 0.0f
				&& std::abs(glm::length(blade.Direction) - 1.0f) < CHECK_AXIS_TOLERANCE;
			weapons += std::format("{}{} {:.2f} m, half-width {:.3f} m, sphere radius {:.3f} m", weapons.empty() ? "" : "; ",
				fighter.RightWeapon ? fighter.RightWeapon : "none", blade.Length, blade.HalfWidth, WeaponSphereRadius(blade));
		}
		report.Check(sensible, std::format("every right-hand weapon has a blade axis of a sensible length and a width from its grip: {}; spheres at {:.2f}, {:.2f}, {:.2f} of the axis",
			weapons, WEAPON_SPHERE_FRACTIONS[0], WEAPON_SPHERE_FRACTIONS[1], WEAPON_SPHERE_FRACTIONS[2]));
	}
}

namespace Dingo
{

	bool RunCombatChecks(const GameAssets& assets)
	{
		CheckReport report;

		report.Check(ValidateMoveset(assets.GetClips()) == 0, "the Moveset's windows all end before their one-shots return and every rule has its windows (see the warnings above if not)");
		const Model* knightModel = assets.GetCharacter(GetPlayerDef());
		const Skeleton* knightSkeleton = knightModel ? knightModel->GetSkeleton() : nullptr;
		CheckAttackWindows(report, assets, knightSkeleton);
		CheckDodgeWindows(report, assets, knightSkeleton);
		CheckBlockWindows(report, assets);
		CheckGeometry(report, assets);

		const AnimationClip* idle = assets.GetClip(Clips::IDLE);
		const Skeleton* skeleton = idle ? idle->GetSourceSkeleton() : nullptr;
		const LocomotionStates states = MakeLocomotionStates(assets.GetClips());
		report.Check(knightModel && skeleton && states.Complete, "the library skeleton and the locomotion states are there to run the animator checks on");
		if (skeleton && states.Complete)
		{
			CheckParry(report, *skeleton, states, assets);
			CheckHitReaction(report, *skeleton, states, assets);
			CheckDeath(report, *skeleton, states, assets);
			CheckRiposte(report, *skeleton, states, assets);
			CheckBlockHit(report, *skeleton, states, assets);
			CheckTravelCarry(report, *skeleton, states, assets);
			CheckTravelConservation(report, *skeleton, assets);
			CheckFighterCarry(report, assets, knightSkeleton);
			CheckParryCooldown(report, assets);
		}

		if (report.GetFailed() == 0)
			DE_INFO("Combat checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		else
			DE_ERROR("Combat checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		return report.GetFailed() == 0;
	}

}
