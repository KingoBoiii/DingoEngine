#include "Locomotion.h"
#include "GameMath.h"
#include "GameTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
	using namespace Dingo;

	struct FootPhases
	{
		float Left = -1.0f;
		float Right = -1.0f;

		bool IsValid() const { return Left >= 0.0f && Right >= 0.0f; }
	};

	FootPhases GetFootPhases(const AnimationClip* clip)
	{
		return { FootfallPhase(clip, Events::STEP_LEFT), FootfallPhase(clip, Events::STEP_RIGHT) };
	}

	float Fraction(float value)
	{
		return value - std::floor(value);
	}

	float MapPhase(float phase, const FootPhases& from, const FootPhases& to)
	{
		const float armFrom = Fraction(from.Right - from.Left);
		const float armTo = Fraction(to.Right - to.Left);
		const float along = Fraction(phase - from.Left);

		float mapped = 0.0f;
		if (along < armFrom)
			mapped = armTo * along / armFrom;
		else if (armFrom < 1.0f)
			mapped = armTo + (1.0f - armTo) * (along - armFrom) / (1.0f - armFrom);
		return Fraction(to.Left + mapped);
	}

	float CycleSeconds(const Animator& animator)
	{
		Animator probe(animator);
		probe.SetNormalizedTime(0.5f);
		return 2.0f * probe.GetTime();
	}
}

namespace Dingo
{

	const char* ToString(LocomotionZone zone)
	{
		switch (zone)
		{
			case LocomotionZone::StrafeLeft:  return "StrafeLeft";
			case LocomotionZone::StrafeRight: return "StrafeRight";
			case LocomotionZone::Backward:    return "Backward";
			default:                          return "Forward";
		}
	}

	LocomotionZone PickZone(LocomotionZone current, float travel)
	{
		travel = GameMath::WrapAngle(travel);
		const float angle = std::abs(travel);
		const float hysteresis = glm::radians(ZONE_HYSTERESIS_DEG);
		const float forwardEdge = glm::radians(ZONE_FORWARD_MAX_DEG) + (current == LocomotionZone::Forward ? hysteresis : -hysteresis);
		const float backEdge = glm::radians(ZONE_BACK_MIN_DEG) + (current == LocomotionZone::Backward ? -hysteresis : hysteresis);

		if (angle < forwardEdge)
			return LocomotionZone::Forward;
		if (angle >= backEdge)
			return LocomotionZone::Backward;
		return travel > 0.0f ? LocomotionZone::StrafeLeft : LocomotionZone::StrafeRight;
	}

	float ZoneAngle(LocomotionZone zone, float travel)
	{
		switch (zone)
		{
			case LocomotionZone::StrafeLeft:  return glm::radians(STRAFE_CLIP_ANGLE_DEG);
			case LocomotionZone::StrafeRight: return -glm::radians(STRAFE_CLIP_ANGLE_DEG);
			case LocomotionZone::Backward:    return travel >= 0.0f ? std::numbers::pi_v<float> : -std::numbers::pi_v<float>;
			default:                          return 0.0f;
		}
	}

	const AnimationState& LocomotionStates::For(LocomotionZone zone) const
	{
		switch (zone)
		{
			case LocomotionZone::StrafeLeft:  return StrafeLeft;
			case LocomotionZone::StrafeRight: return StrafeRight;
			case LocomotionZone::Backward:    return Backward;
			default:                          return Blend;
		}
	}

	LocomotionStates MakeLocomotionStates(const ClipSet& clips)
	{
		LocomotionStates states;
		const AnimationClip* idle = clips.Find(Clips::IDLE);
		const AnimationClip* walk = clips.Find(Clips::WALK);
		const AnimationClip* run = clips.Find(Clips::RUN);
		const AnimationClip* backwards = clips.Find(Clips::BACKWARDS);
		const AnimationClip* strafeLeft = clips.Find(Clips::STRAFE_LEFT);
		const AnimationClip* strafeRight = clips.Find(Clips::STRAFE_RIGHT);
		if (!idle || !walk || !run || !backwards || !strafeLeft || !strafeRight)
			return states;

		states.Blend = AnimationState::Blend1D(MOVE_PARAMETER, { { 0.0f, idle }, { WALK_SPEED, walk }, { RUN_SPEED, run } });
		states.Backward = AnimationState::Blend1D(MOVE_PARAMETER, { { 0.0f, idle }, { BACKPEDAL_SPEED, backwards } }).SetSpeed(BACKPEDAL_PLAYBACK);
		states.StrafeLeft = AnimationState::Blend1D(MOVE_PARAMETER, { { 0.0f, idle }, { STRAFE_SPEED, strafeLeft } }).SetSpeed(STRAFE_PLAYBACK);
		states.StrafeRight = AnimationState::Blend1D(MOVE_PARAMETER, { { 0.0f, idle }, { STRAFE_SPEED, strafeRight } }).SetSpeed(STRAFE_PLAYBACK);
		states.Complete = true;
		return states;
	}

	float FootfallPhase(const AnimationClip* clip, std::string_view event)
	{
		if (!clip || !(clip->GetDuration() > 0.0f))
			return -1.0f;

		for (const AnimationClipEvent& candidate : clip->GetEvents())
		{
			if (!candidate.Range && candidate.Name == event)
				return candidate.Time / clip->GetDuration();
		}
		return -1.0f;
	}

	void SwitchZone(Animator& animator, const AnimationState& from, const AnimationState& to, float step)
	{
		const FootPhases before = GetFootPhases(animator.GetCurrentClip());
		const float phase = animator.GetNormalizedTime();
		const float cycleBefore = CycleSeconds(animator);

		animator.Play(to, ZONE_FADE);
		const FootPhases after = GetFootPhases(to.IsBlend() ? to.GetPoints().back().Clip : to.GetClip());
		const float cycleAfter = CycleSeconds(animator);
		if (!before.IsValid() || !after.IsValid() || !(cycleBefore > 0.0f) || !(cycleAfter > 0.0f))
			return;

		// Only the clip leading the cross-fade fires footfalls, and the new state leads from the first frame
		// whose fade weight reaches a half. Aligning both clips' footfalls at the start of that frame, not
		// now, keeps one from repeating or dropping at the hand-over. The new state's own clip is used
		// though its Move may still read idle, and the Move is taken as constant.
		const float oldLeads = step > 0.0f ? std::max(std::ceil(0.5f * ZONE_FADE / step) - 1.0f, 0.0f) * step : 0.0f;
		const float handover = MapPhase(phase + from.GetSpeed() / cycleBefore * oldLeads, before, after);
		animator.SetNormalizedTime(Fraction(handover - to.GetSpeed() / cycleAfter * oldLeads));
	}

	void ResolveSeparation(SeparationBody& a, SeparationBody& b, float deltaTime)
	{
		const glm::vec2 delta = b.Position - a.Position;
		const float distance = glm::length(delta);
		const glm::vec2 normal = distance > 1.0e-4f ? delta / distance : glm::vec2(1.0f, 0.0f);

		const float approachA = glm::dot(a.Velocity, normal);
		const float approachB = -glm::dot(b.Velocity, normal);
		const float closing = approachA + approachB;
		const float allowed = std::max((distance - (a.Radius + b.Radius)) / std::max(deltaTime, 1.0e-4f), -SEPARATION_PUSH_SPEED);
		if (closing <= allowed)
			return;

		const float excess = closing - allowed;
		const float pushing = std::max(approachA, 0.0f) + std::max(approachB, 0.0f);
		const float shareA = pushing > 1.0e-4f ? std::max(approachA, 0.0f) / pushing : 0.5f;
		a.Velocity -= normal * (excess * shareA);
		b.Velocity += normal * (excess * (1.0f - shareA));
	}

}
