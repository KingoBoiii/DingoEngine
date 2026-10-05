#include "Checks.h"
#include "CheckReport.h"
#include "CheckTuning.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Locomotion.h"
#include "Moveset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <numbers>
#include <string>
#include <vector>

namespace
{
	using namespace Dingo;

	struct FootContact
	{
		uint32_t Stances = 0;
		float Touchdown = -1.0f;
		float Liftoff = -1.0f;
		float Lowest = -1.0f;
		glm::dvec2 Travel{ 0.0 };
		bool Valid = false;

		float GetSpeed() const { return static_cast<float>(glm::length(Travel)); }
	};

	struct ClipContact
	{
		const AnimationClip* Clip = nullptr;
		FootContact Left;
		FootContact Right;

		float GetSpeed() const { return 0.5f * (Left.GetSpeed() + Right.GetSpeed()); }
		float GetDirectionDegrees() const
		{
			const glm::dvec2 sum = Left.Travel + Right.Travel;
			return static_cast<float>(std::atan2(sum.x, sum.y)) * 180.0f / std::numbers::pi_v<float>;
		}
	};

	FootContact AnalyseFoot(const std::vector<glm::dvec3>& path, int cycle)
	{
		FootContact foot;
		const int total = static_cast<int>(path.size());

		double lowest = path[0].y;
		int lowestIndex = 0;
		for (int i = 1; i < cycle; ++i)
		{
			if (path[i].y < lowest)
			{
				lowest = path[i].y;
				lowestIndex = i;
			}
		}

		const double limit = lowest + CHECK_CONTACT_BAND;
		int bestBegin = -1;
		int bestEnd = -1;
		for (int i = 0; i < total;)
		{
			if (path[i].y > limit)
			{
				++i;
				continue;
			}

			int end = i;
			while (end + 1 < total && path[end + 1].y <= limit)
				++end;

			if (i > 0 && end < total - 1 && i >= cycle && i < 2 * cycle)
			{
				++foot.Stances;
				if (bestBegin < 0 || end - i > bestEnd - bestBegin)
				{
					bestBegin = i;
					bestEnd = end;
				}
			}
			i = end + 1;
		}

		foot.Lowest = static_cast<float>(lowestIndex) * CHECK_SAMPLE_STEP;
		const int count = bestEnd - bestBegin + 1;
		if (bestBegin < 0 || count < 3)
			return foot;

		double sumT = 0.0, sumX = 0.0, sumZ = 0.0, sumTT = 0.0, sumTX = 0.0, sumTZ = 0.0;
		for (int i = bestBegin; i <= bestEnd; ++i)
		{
			const double t = static_cast<double>(i - bestBegin) * CHECK_SAMPLE_STEP;
			sumT += t;
			sumX += path[i].x;
			sumZ += path[i].z;
			sumTT += t * t;
			sumTX += t * path[i].x;
			sumTZ += t * path[i].z;
		}
		const double denominator = count * sumTT - sumT * sumT;
		foot.Travel = glm::dvec2(-(count * sumTX - sumT * sumX) / denominator, -(count * sumTZ - sumT * sumZ) / denominator);
		foot.Touchdown = static_cast<float>(bestBegin - cycle) * CHECK_SAMPLE_STEP;
		foot.Liftoff = static_cast<float>((bestEnd - cycle) % cycle) * CHECK_SAMPLE_STEP;
		foot.Valid = true;
		return foot;
	}

	ClipContact AnalyseClip(const AnimationClip& clip)
	{
		ClipContact result;
		result.Clip = &clip;

		const Skeleton* skeleton = clip.GetSourceSkeleton();
		const float duration = clip.GetDuration();
		if (!skeleton || !(duration > 0.0f))
			return result;

		const int32_t left = skeleton->FindJoint(Joints::FOOT_LEFT);
		const int32_t right = skeleton->FindJoint(Joints::FOOT_RIGHT);
		if (left == Skeleton::k_InvalidJoint || right == Skeleton::k_InvalidJoint)
			return result;

		Animator animator(skeleton);
		animator.Play(&clip);

		const int cycle = std::max(1, static_cast<int>(std::lround(duration / CHECK_SAMPLE_STEP)));
		std::vector<glm::dvec3> leftPath(static_cast<size_t>(3 * cycle));
		std::vector<glm::dvec3> rightPath(leftPath.size());
		for (int i = 0; i < 3 * cycle; ++i)
		{
			animator.SetTime(static_cast<float>(i % cycle) * CHECK_SAMPLE_STEP);
			animator.Evaluate();
			const glm::vec4 leftPoint = animator.GetJointTransform(left)[3];
			const glm::vec4 rightPoint = animator.GetJointTransform(right)[3];
			leftPath[static_cast<size_t>(i)] = glm::dvec3(leftPoint.x, leftPoint.y, leftPoint.z);
			rightPath[static_cast<size_t>(i)] = glm::dvec3(rightPoint.x, rightPoint.y, rightPoint.z);
		}

		result.Left = AnalyseFoot(leftPath, cycle);
		result.Right = AnalyseFoot(rightPath, cycle);
		return result;
	}

	struct Footfalls
	{
		int Left = 0;
		int Right = 0;
		float LeftTime = -1.0f;
		float RightTime = -1.0f;
	};

	Footfalls ReadFootfalls(const AnimationClip& clip)
	{
		Footfalls footfalls;
		for (const AnimationClipEvent& event : clip.GetEvents())
		{
			if (event.Range)
				continue;
			if (event.Name == Events::STEP_LEFT)
			{
				++footfalls.Left;
				footfalls.LeftTime = event.Time;
			}
			else if (event.Name == Events::STEP_RIGHT)
			{
				++footfalls.Right;
				footfalls.RightTime = event.Time;
			}
		}
		return footfalls;
	}

	float CycleGap(float a, float b, float cycle)
	{
		const float gap = std::abs(a - b);
		return std::min(gap, std::abs(cycle - gap));
	}

	float Fraction(float value)
	{
		return value - std::floor(value);
	}

	float SignedGap(float value, float cycle)
	{
		value = std::fmod(value, cycle);
		if (value >= 0.5f * cycle)
			return value - cycle;
		if (value < -0.5f * cycle)
			return value + cycle;
		return value;
	}

	bool Within(float derived, float tuned)
	{
		return std::abs(derived - tuned) <= CHECK_SPEED_TOLERANCE * tuned;
	}

	void CheckSharedFootfalls(CheckReport& report, const GameAssets& assets, const std::array<ClipContact, 5>& contacts)
	{
		const AnimationClip* walk = assets.GetClip(Clips::WALK);
		const AnimationClip* run = assets.GetClip(Clips::RUN);
		if (!walk || !run)
			return;

		const ClipContact& walkContact = contacts[0];
		const ClipContact& runContact = contacts[1];
		const Footfalls walkSteps = ReadFootfalls(*walk);
		const Footfalls runSteps = ReadFootfalls(*run);
		const float walkCycle = walk->GetDuration();
		const float runCycle = run->GetDuration();

		const float sharedLeft = Fraction(runContact.Left.Touchdown / runCycle - FOOTSTEP_SHARED_LEAD);
		const float sharedRight = Fraction(runContact.Right.Touchdown / runCycle - FOOTSTEP_SHARED_LEAD);
		const std::array<float, 2> runOffset = {
			SignedGap(runSteps.LeftTime - runContact.Left.Touchdown, runCycle) * 1000.0f,
			SignedGap(runSteps.RightTime - runContact.Right.Touchdown, runCycle) * 1000.0f };
		const std::array<float, 2> walkOffset = {
			SignedGap(walkSteps.LeftTime - walkContact.Left.Touchdown, walkCycle) * 1000.0f,
			SignedGap(walkSteps.RightTime - walkContact.Right.Touchdown, walkCycle) * 1000.0f };

		DE_INFO("[INFO] shared footfall phase: foot.l {:.4f}, foot.r {:.4f} of the cycle (the run's touchdowns at {:.4f} and {:.4f} of its cycle, less {:.2f})",
			sharedLeft, sharedRight, runContact.Left.Touchdown / runCycle, runContact.Right.Touchdown / runCycle, FOOTSTEP_SHARED_LEAD);
		DE_INFO("[INFO]   {:<10} step_l at {:.4f} s, step_r at {:.4f} s; true touchdowns {:.3f} s and {:.3f} s; the sound is {:+.0f} and {:+.0f} ms from them",
			Clips::RUN, runSteps.LeftTime, runSteps.RightTime, runContact.Left.Touchdown, runContact.Right.Touchdown, runOffset[0], runOffset[1]);
		DE_INFO("[INFO]   {:<10} step_l at {:.4f} s, step_r at {:.4f} s; true touchdowns {:.3f} s and {:.3f} s; the sound is {:+.0f} and {:+.0f} ms from them",
			Clips::WALK, walkSteps.LeftTime, walkSteps.RightTime, walkContact.Left.Touchdown, walkContact.Right.Touchdown, walkOffset[0], walkOffset[1]);

		const bool measured = walkContact.Left.Valid && walkContact.Right.Valid && runContact.Left.Valid && runContact.Right.Valid;
		const bool single = walkSteps.Left == 1 && walkSteps.Right == 1 && runSteps.Left == 1 && runSteps.Right == 1;
		const bool shared = measured && single
			&& CycleGap(walkSteps.LeftTime / walkCycle, sharedLeft, 1.0f) <= CHECK_PHASE_TOLERANCE
			&& CycleGap(walkSteps.RightTime / walkCycle, sharedRight, 1.0f) <= CHECK_PHASE_TOLERANCE
			&& CycleGap(runSteps.LeftTime / runCycle, sharedLeft, 1.0f) <= CHECK_PHASE_TOLERANCE
			&& CycleGap(runSteps.RightTime / runCycle, sharedRight, 1.0f) <= CHECK_PHASE_TOLERANCE
			&& CycleGap(walkSteps.LeftTime / walkCycle, runSteps.LeftTime / runCycle, 1.0f) <= CHECK_PHASE_MATCH
			&& CycleGap(walkSteps.RightTime / walkCycle, runSteps.RightTime / runCycle, 1.0f) <= CHECK_PHASE_MATCH;
		bool bounded = measured && single;
		for (const float offset : { runOffset[0], runOffset[1], walkOffset[0], walkOffset[1] })
			bounded = bounded && offset >= -CHECK_STEP_EARLY_MAX * 1000.0f && offset <= CHECK_STEP_LATE_MAX * 1000.0f;

		report.Check(shared && bounded,
			std::format("{} and {} fire {} and {} at one shared cycle fraction (foot.l {:.4f}, foot.r {:.4f}), at most {:.0f} ms early to {:.0f} ms late of their true touchdowns (run {:+.0f} and {:+.0f} ms, walk {:+.0f} and {:+.0f} ms)",
				Clips::WALK, Clips::RUN, Events::STEP_LEFT, Events::STEP_RIGHT, sharedLeft, sharedRight, CHECK_STEP_EARLY_MAX * 1000.0f, CHECK_STEP_LATE_MAX * 1000.0f,
				runOffset[0], runOffset[1], walkOffset[0], walkOffset[1]));
	}

	void CheckFootContacts(CheckReport& report, const GameAssets& assets, std::array<ClipContact, 5>& contacts)
	{
		const std::array<const char*, 5> names = { Clips::WALK, Clips::RUN, Clips::BACKWARDS, Clips::STRAFE_LEFT, Clips::STRAFE_RIGHT };
		for (size_t i = 0; i < names.size(); ++i)
		{
			const AnimationClip* clip = assets.GetClip(names[i]);
			if (!clip)
			{
				report.Check(false, std::format("{} is available to analyse", names[i]));
				continue;
			}

			contacts[i] = AnalyseClip(*clip);
			const ClipContact& contact = contacts[i];
			const float duration = clip->GetDuration();
			DE_INFO("[INFO] {:<22} {:.3f} s  foot.l down {:.3f} up {:.3f} (ankle lowest {:.3f})  foot.r down {:.3f} up {:.3f} (lowest {:.3f})  travels {:.3f} m/s at {:.1f} deg (l {:.3f}, r {:.3f})",
				names[i], duration, contact.Left.Touchdown, contact.Left.Liftoff, contact.Left.Lowest, contact.Right.Touchdown, contact.Right.Liftoff, contact.Right.Lowest,
				contact.GetSpeed(), contact.GetDirectionDegrees(), contact.Left.GetSpeed(), contact.Right.GetSpeed());

			report.Check(contact.Left.Valid && contact.Right.Valid && contact.Left.Stances == 1 && contact.Right.Stances == 1,
				std::format("{}: each foot is planted once per {:.3f} s cycle (foot.l {} stance at {:.3f} s, foot.r {} at {:.3f} s)", names[i], duration,
					contact.Left.Stances, contact.Left.Touchdown, contact.Right.Stances, contact.Right.Touchdown));

			if (i < 2)
				continue;

			const Footfalls footfalls = ReadFootfalls(*clip);
			const bool placed = footfalls.Left == 1 && footfalls.Right == 1 && contact.Left.Valid && contact.Right.Valid
				&& CycleGap(footfalls.LeftTime, contact.Left.Touchdown, duration) <= CHECK_EVENT_TOLERANCE
				&& CycleGap(footfalls.RightTime, contact.Right.Touchdown, duration) <= CHECK_EVENT_TOLERANCE;
			report.Check(placed, std::format("{}: one {} at {:.3f} s and one {} at {:.3f} s in its .events, on the derived touchdowns {:.3f} s and {:.3f} s (within {:.2f} s)",
				names[i], Events::STEP_LEFT, footfalls.LeftTime, Events::STEP_RIGHT, footfalls.RightTime, contact.Left.Touchdown, contact.Right.Touchdown, CHECK_EVENT_TOLERANCE));
		}

		CheckSharedFootfalls(report, assets, contacts);
	}

	void CheckSpeeds(CheckReport& report, const std::array<ClipContact, 5>& contacts)
	{
		const float walk = contacts[0].GetSpeed();
		const float run = contacts[1].GetSpeed();
		const float backwards = contacts[2].GetSpeed();
		const float strafe = 0.5f * (contacts[3].GetSpeed() + contacts[4].GetSpeed());
		DE_INFO("[INFO] derived speeds at scale 1 and pace 1: walk {:.3f}, run {:.3f}, backpedal {:.3f}, strafe clips {:.3f} m/s ({:.3f} and {:.3f})",
			walk, run, backwards, strafe, contacts[3].GetSpeed(), contacts[4].GetSpeed());

		report.Check(Within(walk, WALK_SPEED), std::format("derived walk speed {:.3f} m/s is within {:.0f}% of the tuned WALK_SPEED {:.3f}", walk, CHECK_SPEED_TOLERANCE * 100.0f, WALK_SPEED));
		report.Check(Within(run, RUN_SPEED), std::format("derived run speed {:.3f} m/s is within {:.0f}% of the tuned RUN_SPEED {:.3f}", run, CHECK_SPEED_TOLERANCE * 100.0f, RUN_SPEED));
		report.Check(Within(backwards * BACKPEDAL_PLAYBACK, BACKPEDAL_SPEED),
			std::format("derived backpedal speed {:.3f} m/s (playing at {:.2f}x) is within {:.0f}% of the tuned BACKPEDAL_SPEED {:.3f}", backwards * BACKPEDAL_PLAYBACK, BACKPEDAL_PLAYBACK, CHECK_SPEED_TOLERANCE * 100.0f, BACKPEDAL_SPEED));
		report.Check(Within(strafe, STRAFE_CLIP_SPEED) && Within(strafe * STRAFE_PLAYBACK, STRAFE_SPEED),
			std::format("derived strafe speed {:.3f} m/s (playing at {:.2f}x, {:.3f} m/s) is within {:.0f}% of the tuned STRAFE_CLIP_SPEED {:.3f} and STRAFE_SPEED {:.3f}",
				strafe, STRAFE_PLAYBACK, strafe * STRAFE_PLAYBACK, CHECK_SPEED_TOLERANCE * 100.0f, STRAFE_CLIP_SPEED, STRAFE_SPEED));

		const float tolerance = CHECK_ANGLE_TOLERANCE_DEG;
		const float forward = contacts[0].GetDirectionDegrees();
		const float forwardRun = contacts[1].GetDirectionDegrees();
		const float back = contacts[2].GetDirectionDegrees();
		const float left = contacts[3].GetDirectionDegrees();
		const float right = contacts[4].GetDirectionDegrees();
		report.Check(std::abs(forward) <= tolerance && std::abs(forwardRun) <= tolerance && 180.0f - std::abs(back) <= tolerance
				&& std::abs(left - STRAFE_CLIP_ANGLE_DEG) <= tolerance && std::abs(right + STRAFE_CLIP_ANGLE_DEG) <= tolerance,
			std::format("the clips travel where the zones assume: walk {:.1f}, run {:.1f}, backpedal {:.1f}, strafe left {:.1f} and right {:.1f} degrees from the facing (left is positive; strafe expected at +-{:.0f})",
				forward, forwardRun, back, left, right, STRAFE_CLIP_ANGLE_DEG));
	}

	void CheckZones(CheckReport& report)
	{
		auto sweep = [](float from, float to, float step, float sign, LocomotionZone start, std::vector<float>& changes)
		{
			LocomotionZone zone = start;
			const int steps = static_cast<int>(std::lround(std::abs(to - from) / step));
			for (int i = 0; i <= steps; ++i)
			{
				const float degrees = from + (to > from ? 1.0f : -1.0f) * step * static_cast<float>(i);
				const LocomotionZone next = PickZone(zone, sign * glm::radians(degrees));
				if (next != zone)
					changes.push_back(degrees);
				zone = next;
			}
			return zone;
		};

		std::vector<float> outward, inward, mirrored;
		const LocomotionZone outermost = sweep(0.0f, 180.0f, 0.5f, 1.0f, LocomotionZone::Forward, outward);
		const LocomotionZone home = sweep(180.0f, 0.0f, 0.5f, 1.0f, outermost, inward);
		const LocomotionZone right = sweep(0.0f, 100.0f, 0.5f, -1.0f, LocomotionZone::Forward, mirrored);

		const float forwardOut = ZONE_FORWARD_MAX_DEG + ZONE_HYSTERESIS_DEG;
		const float backOut = ZONE_BACK_MIN_DEG + ZONE_HYSTERESIS_DEG;
		const float backIn = ZONE_BACK_MIN_DEG - ZONE_HYSTERESIS_DEG;
		const float forwardIn = ZONE_FORWARD_MAX_DEG - ZONE_HYSTERESIS_DEG;
		const bool outwardOk = outward.size() == 2 && std::abs(outward[0] - forwardOut) <= 0.5f && std::abs(outward[1] - backOut) <= 0.5f && outermost == LocomotionZone::Backward;
		const bool inwardOk = inward.size() == 2 && std::abs(inward[0] - backIn) <= 0.5f && std::abs(inward[1] - forwardIn) <= 0.5f && home == LocomotionZone::Forward;
		report.Check(outwardOk && inwardOk && mirrored.size() == 1 && right == LocomotionZone::StrafeRight,
			std::format("the zones switch Forward to Strafe at {:.0f} degrees and on to Backward at {:.0f} going out, back at {:.0f} and {:.0f} coming in (measured {:.1f}, {:.1f} out; {:.1f}, {:.1f} in), and mirror to the right",
				forwardOut, backOut, backIn, forwardIn, outward.size() > 0 ? outward[0] : -1.0f, outward.size() > 1 ? outward[1] : -1.0f,
				inward.size() > 0 ? inward[0] : -1.0f, inward.size() > 1 ? inward[1] : -1.0f));

		auto jitter = [](LocomotionZone start, float centerDegrees, float amplitudeDegrees)
		{
			LocomotionZone zone = start;
			int flips = 0;
			for (int i = 0; i < 200; ++i)
			{
				const float degrees = centerDegrees + (i % 2 == 0 ? amplitudeDegrees : -amplitudeDegrees);
				const LocomotionZone next = PickZone(zone, glm::radians(degrees));
				if (next != zone)
					++flips;
				zone = next;
			}
			return flips;
		};
		const int atForward = jitter(LocomotionZone::Forward, ZONE_FORWARD_MAX_DEG, 0.5f * ZONE_HYSTERESIS_DEG);
		const int atStrafe = jitter(LocomotionZone::StrafeLeft, ZONE_FORWARD_MAX_DEG, 0.5f * ZONE_HYSTERESIS_DEG);
		const int atBack = jitter(LocomotionZone::Backward, ZONE_BACK_MIN_DEG, 0.5f * ZONE_HYSTERESIS_DEG);
		report.Check(atForward == 0 && atStrafe == 0 && atBack == 0,
			std::format("travel directions jittering +-{:.0f} degrees around a zone edge never flip the zone (flips {}, {}, {})", 0.5f * ZONE_HYSTERESIS_DEG, atForward, atStrafe, atBack));
	}

	void CheckSeparation(CheckReport& report)
	{
		const float step = 1.0f / 60.0f;
		const float radius = FIGHTER_RADIUS;

		SeparationBody a{ glm::vec2(0.0f), glm::vec2(3.0f, 0.0f), radius };
		SeparationBody b{ glm::vec2(0.65f, 0.0f), glm::vec2(-3.0f, 0.0f), radius };
		ResolveSeparation(a, b, step);
		const float gap = (b.Position.x + b.Velocity.x * step) - (a.Position.x + a.Velocity.x * step);
		const bool closing = std::abs(gap - 2.0f * radius) <= 1.0e-4f && std::abs(a.Velocity.x + b.Velocity.x) <= 1.0e-4f;

		SeparationBody still{ glm::vec2(0.65f, 0.0f), glm::vec2(0.0f), radius };
		SeparationBody pusher{ glm::vec2(0.0f), glm::vec2(5.0f, 0.0f), radius };
		ResolveSeparation(pusher, still, step);
		const bool oneSided = still.Velocity == glm::vec2(0.0f) && pusher.Velocity.x > 0.0f && pusher.Velocity.x < 5.0f;

		SeparationBody apartA{ glm::vec2(0.0f), glm::vec2(-3.0f, 0.0f), radius };
		SeparationBody apartB{ glm::vec2(0.65f, 0.0f), glm::vec2(3.0f, 0.0f), radius };
		ResolveSeparation(apartA, apartB, step);
		const bool leaving = apartA.Velocity == glm::vec2(-3.0f, 0.0f) && apartB.Velocity == glm::vec2(3.0f, 0.0f);

		SeparationBody overlapA{ glm::vec2(0.0f), glm::vec2(0.0f), radius };
		SeparationBody overlapB{ glm::vec2(0.4f, 0.0f), glm::vec2(0.0f), radius };
		ResolveSeparation(overlapA, overlapB, step);
		const float opening = overlapB.Velocity.x - overlapA.Velocity.x;
		const bool eased = std::abs(opening - SEPARATION_PUSH_SPEED) <= 1.0e-4f && overlapA.Velocity.x < 0.0f && overlapB.Velocity.x > 0.0f;

		report.Check(closing && oneSided && leaving && eased,
			std::format("separation lets two fighters close to the sum of their radii and no further (gap after a step {:.4f} of {:.2f}), takes the speed off the one that approaches, leaves parting ones alone and eases overlapping ones apart at {:.1f} m/s (opening {:.2f})",
				gap, 2.0f * radius, SEPARATION_PUSH_SPEED, opening));
	}

	struct StepEvent
	{
		float Time = 0.0f;
		int Foot = 0;
		int Segment = 0;
	};

	struct ZoneSwitch
	{
		float Time = 0.0f;
		LocomotionZone Zone = LocomotionZone::Forward;
	};

	struct GaitScript
	{
		std::function<float(float)> Move;
		std::vector<ZoneSwitch> Switches;
		float Seconds = 0.0f;
		float MaxCycles = 0.0f;
	};

	struct GaitLog
	{
		std::array<int, 2> Fired{ 0, 0 };
		std::array<int, 2> Expected{ 0, 0 };
		std::array<std::vector<float>, 2> Phases;
		std::array<std::vector<glm::vec3>, 2> FootPath;
		std::vector<float> Moves;
		std::vector<StepEvent> Steps;
		std::vector<size_t> SwitchSteps;
		float Cycles = 0.0f;
	};

	GaitLog RunGait(const Skeleton& skeleton, const LocomotionStates& states, const GaitScript& script)
	{
		GaitLog log;
		const std::array<int32_t, 2> joints = { skeleton.FindJoint(Joints::FOOT_LEFT), skeleton.FindJoint(Joints::FOOT_RIGHT) };

		Animator animator(&skeleton);
		animator.SetFloat(MOVE_PARAMETER, script.Move(0.0f));
		animator.Play(states.Blend);
		animator.Update(0.0f);

		auto record = [&](float move)
		{
			log.Moves.push_back(move);
			for (size_t foot = 0; foot < 2; ++foot)
				log.FootPath[foot].emplace_back(animator.GetJointTransform(joints[foot])[3]);
		};
		record(script.Move(0.0f));

		LocomotionZone zone = LocomotionZone::Forward;
		size_t nextSwitch = 0;
		int segment = 0;
		const int frames = static_cast<int>(std::lround(script.Seconds / CHECK_GAIT_STEP));
		for (int frame = 1; frame <= frames && !(script.MaxCycles > 0.0f && log.Cycles >= script.MaxCycles); ++frame)
		{
			const float time = static_cast<float>(frame) * CHECK_GAIT_STEP;
			const float move = script.Move(time);
			animator.SetFloat(MOVE_PARAMETER, move);
			if (nextSwitch < script.Switches.size() && time >= script.Switches[nextSwitch].Time)
			{
				log.SwitchSteps.push_back(log.Steps.size());
				SwitchZone(animator, states.For(zone), states.For(script.Switches[nextSwitch].Zone), CHECK_GAIT_STEP);
				zone = script.Switches[nextSwitch].Zone;
				++nextSwitch;
			}

			const float before = animator.GetNormalizedTime();
			const AnimationClip* dominant = animator.GetCurrentClip();
			animator.Update(CHECK_GAIT_STEP);
			const float after = animator.GetNormalizedTime();
			log.Cycles += after >= before ? after - before : after + 1.0f - before;

			if (script.Switches.empty())
			{
				if (FootfallPhase(dominant, Events::STEP_LEFT) < 0.0f)
					++segment;

				for (int foot = 0; foot < 2; ++foot)
				{
					const float mark = FootfallPhase(dominant, foot == 0 ? Events::STEP_LEFT : Events::STEP_RIGHT);
					if (mark >= 0.0f && (after >= before ? (mark > before && mark <= after) : (mark > before || mark <= after)))
						++log.Expected[static_cast<size_t>(foot)];
				}
			}

			for (const AnimationEvent& event : animator.GetEventsThisFrame())
			{
				if (event.Type != AnimationEventType::Instant)
					continue;

				const bool left = event.Name == Events::STEP_LEFT;
				if (!left && event.Name != Events::STEP_RIGHT)
					continue;

				const size_t foot = left ? 0 : 1;
				++log.Fired[foot];
				log.Phases[foot].push_back(log.Cycles);
				log.Steps.push_back({ time, static_cast<int>(foot), segment });
			}

			record(move);
		}
		return log;
	}

	GaitScript ConstantScript(float move)
	{
		return { .Move = [move](float) { return move; }, .Seconds = 20.0f, .MaxCycles = CHECK_GAIT_CYCLES };
	}

	GaitScript RampScript(float from, float to, float rampSeconds)
	{
		return { .Move = [=](float time) { return from + (to - from) * std::min(time / rampSeconds, 1.0f); }, .Seconds = rampSeconds + CHECK_RAMP_HOLD };
	}

	GaitScript UpDownUpScript(float legSeconds)
	{
		const auto move = [=](float time)
		{
			const float leg = std::min(time / legSeconds, 3.0f);
			if (leg < 1.0f)
				return RUN_SPEED * leg;
			if (leg < 2.0f)
				return RUN_SPEED * (2.0f - leg);
			return RUN_SPEED * (leg - 2.0f);
		};
		return { .Move = move, .Seconds = 3.0f * legSeconds + CHECK_RAMP_HOLD };
	}

	float RampTo(float from, float to, float elapsed)
	{
		const float rate = to > from ? MOVE_ACCEL : MOVE_DECEL;
		return from + std::copysign(std::min(rate * elapsed, std::abs(to - from)), to - from);
	}

	GaitScript ZoneScript(const std::array<LocomotionZone, 3>& zones, const std::array<float, 3>& speeds, float hold)
	{
		GaitScript script;
		script.Seconds = 4.0f * hold;
		for (size_t i = 0; i < zones.size(); ++i)
			script.Switches.push_back({ hold * static_cast<float>(i + 1), zones[i] });
		script.Move = [=](float time)
		{
			float value = RUN_SPEED;
			float target = RUN_SPEED;
			float start = 0.0f;
			for (size_t i = 0; i < speeds.size(); ++i)
			{
				const float at = hold * static_cast<float>(i + 1);
				if (time < at)
					break;

				value = RampTo(value, target, at - start);
				target = speeds[i];
				start = at;
			}
			return RampTo(value, target, time - start);
		};
		return script;
	}

	struct GapRange
	{
		float Min = 1.0e9f;
		float Max = 0.0f;
		int Count = 0;
	};

	GapRange Gaps(const std::vector<float>& phases)
	{
		GapRange range;
		for (size_t i = 1; i < phases.size(); ++i)
		{
			const float gap = phases[i] - phases[i - 1];
			range.Min = std::min(range.Min, gap);
			range.Max = std::max(range.Max, gap);
			++range.Count;
		}
		return range;
	}

	struct StepTiming
	{
		bool Alternating = true;
		float Shortest = 1.0e9f;
		float Longest = 0.0f;
		size_t Count = 0;

		bool IsInRange() const { return Alternating && Shortest >= CHECK_STEP_GAP_MIN && Longest <= CHECK_GAP_MAX; }
	};

	StepTiming Timing(const std::vector<StepEvent>& steps)
	{
		StepTiming timing;
		timing.Count = steps.size();
		for (size_t i = 1; i < steps.size(); ++i)
		{
			if (steps[i].Segment != steps[i - 1].Segment)
				continue;

			const float gap = steps[i].Time - steps[i - 1].Time;
			timing.Alternating = timing.Alternating && steps[i].Foot != steps[i - 1].Foot;
			timing.Shortest = std::min(timing.Shortest, gap);
			timing.Longest = std::max(timing.Longest, gap);
		}
		return timing;
	}

	float PeakAcceleration(const GaitLog& log)
	{
		float peak = 0.0f;
		for (const std::vector<glm::vec3>& path : log.FootPath)
		{
			for (size_t i = 1; i + 1 < path.size(); ++i)
				peak = std::max(peak, glm::length(path[i + 1] - 2.0f * path[i] + path[i - 1]));
		}
		return peak;
	}

	float WorstPopRatio(const GaitLog& ramp, const std::vector<float>& baseline)
	{
		float worst = 0.0f;
		for (const std::vector<glm::vec3>& path : ramp.FootPath)
		{
			for (size_t i = 1; i + 1 < path.size(); ++i)
			{
				const size_t low = std::min(static_cast<size_t>(ramp.Moves[i] / CHECK_SWEEP_STEP), baseline.size() - 2);
				const float reference = std::max({ baseline[low], baseline[low + 1], CHECK_POP_FLOOR });
				worst = std::max(worst, glm::length(path[i + 1] - 2.0f * path[i] + path[i - 1]) / reference);
			}
		}
		return worst;
	}

	void CheckGait(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states)
	{
		std::vector<float> baseline(static_cast<size_t>(std::lround(RUN_SPEED / CHECK_SWEEP_STEP)) + 2, 0.0f);
		bool allExpected = true;
		bool allEven = true;
		bool allCounted = true;
		std::string lines;
		for (size_t step = 1; static_cast<float>(step) * CHECK_SWEEP_STEP <= RUN_SPEED + 1.0e-4f; ++step)
		{
			const float move = static_cast<float>(step) * CHECK_SWEEP_STEP;
			const GaitLog log = RunGait(skeleton, states, ConstantScript(move));
			baseline[step] = PeakAcceleration(log);

			bool even = true;
			for (size_t foot = 0; foot < 2; ++foot)
			{
				const GapRange range = Gaps(log.Phases[foot]);
				even = even && (range.Count == 0 || (range.Min >= 1.0f - CHECK_EVEN_TOLERANCE && range.Max <= 1.0f + CHECK_EVEN_TOLERANCE));
			}
			const bool silent = move < 0.5f * WALK_SPEED;
			const int fewest = silent ? 0 : static_cast<int>(CHECK_GAIT_CYCLES) - 1;
			const bool counted = std::min(log.Fired[0], log.Fired[1]) >= fewest && (!silent || log.Fired[0] + log.Fired[1] == 0);
			allExpected = allExpected && log.Fired == log.Expected;
			allEven = allEven && even;
			allCounted = allCounted && counted;
			lines += std::format("{}{:.2f}: {}/{}", lines.empty() ? "" : ", ", move, log.Fired[0], log.Fired[1]);
		}
		baseline.front() = baseline[1];
		baseline.back() = baseline[baseline.size() - 2];
		DE_INFO("[INFO] steps (left/right) over {:.0f} cycles at Move m/s: {}", CHECK_GAIT_CYCLES, lines);
		report.Check(allExpected && allEven && allCounted,
			std::format("a constant Move from {:.2f} to {:.2f} m/s in steps of {:.2f}: every footfall the phase passes fires, once per cycle for each foot (no steps while the idle clip leads, below {:.2f} m/s)",
				CHECK_SWEEP_STEP, RUN_SPEED, CHECK_SWEEP_STEP, 0.5f * WALK_SPEED));

		const GaitLog primary = RunGait(skeleton, states, RampScript(0.0f, RUN_SPEED, CHECK_RAMP_SECONDS));
		const GapRange mainLeft = Gaps(primary.Phases[0]);
		const GapRange mainRight = Gaps(primary.Phases[1]);
		const StepTiming mainTiming = Timing(primary.Steps);
		const float mainMin = std::min(mainLeft.Min, mainRight.Min);
		const float mainMax = std::max(mainLeft.Max, mainRight.Max);
		report.Check(primary.Fired == primary.Expected && mainLeft.Count >= 4 && mainRight.Count >= 4
				&& mainMin >= CHECK_FOOT_GAP_MIN && mainMax <= CHECK_FOOT_GAP_MAX && mainTiming.IsInRange(),
			std::format("Move ramped 0 to {:.2f} m/s over {:.0f} s and held {:.0f} s: each foot fires once per cycle as the walk hands over to the run, with nothing filtered (steps {}/{}, same-foot gaps {:.2f} to {:.2f} cycles, alternating steps {:.2f} to {:.2f} s apart)",
				RUN_SPEED, CHECK_RAMP_SECONDS, CHECK_RAMP_HOLD, primary.Fired[0], primary.Fired[1], mainMin, mainMax, mainTiming.Shortest, mainTiming.Longest));

		bool variantsOk = true;
		float worstMin = 1.0e9f;
		float worstMax = 0.0f;
		float worstShortest = 1.0e9f;
		float worstLongest = 0.0f;
		for (const float length : CHECK_RAMP_LENGTHS)
		{
			const GaitLog log = RunGait(skeleton, states, RampScript(0.0f, RUN_SPEED, length));
			const GapRange left = Gaps(log.Phases[0]);
			const GapRange right = Gaps(log.Phases[1]);
			const StepTiming timing = Timing(log.Steps);
			worstMin = std::min({ worstMin, left.Min, right.Min });
			worstMax = std::max({ worstMax, left.Max, right.Max });
			worstShortest = std::min(worstShortest, timing.Shortest);
			worstLongest = std::max(worstLongest, timing.Longest);
			variantsOk = variantsOk && log.Fired == log.Expected && left.Count >= 4 && right.Count >= 4 && timing.IsInRange();
		}
		report.Check(variantsOk && worstMin >= CHECK_FOOT_GAP_MIN && worstMax <= CHECK_FOOT_GAP_MAX,
			std::format("ramps of {:.1f} to {:.1f} s: one footfall per foot per cycle every time, with nothing filtered (same-foot gaps {:.2f} to {:.2f} cycles, alternating steps {:.2f} to {:.2f} s apart)",
				CHECK_RAMP_LENGTHS.front(), CHECK_RAMP_LENGTHS.back(), worstMin, worstMax, worstShortest, worstLongest));

		bool downOk = true;
		float downShortest = 1.0e9f;
		float downLongest = 0.0f;
		size_t downFewest = 1000000;
		for (const float length : CHECK_RAMP_DOWN_LENGTHS)
		{
			const GaitLog log = RunGait(skeleton, states, RampScript(RUN_SPEED, 0.0f, length));
			const StepTiming timing = Timing(log.Steps);
			downShortest = std::min(downShortest, timing.Shortest);
			downLongest = std::max(downLongest, timing.Longest);
			downFewest = std::min(downFewest, timing.Count);
			downOk = downOk && log.Fired == log.Expected && timing.Count >= CHECK_RAMP_MIN_STEPS && timing.IsInRange();
		}
		report.Check(downOk,
			std::format("Move ramped {:.2f} m/s down to 0 over {:.0f} and {:.0f} s: every footfall the phase passes fires once as the run hands over to the walk and the idle (at least {} steps, alternating {:.2f} to {:.2f} s apart)",
				RUN_SPEED, CHECK_RAMP_DOWN_LENGTHS.front(), CHECK_RAMP_DOWN_LENGTHS.back(), downFewest, downShortest, downLongest));

		const GaitLog sweep = RunGait(skeleton, states, UpDownUpScript(CHECK_RAMP_SECONDS));
		const StepTiming sweepTiming = Timing(sweep.Steps);
		report.Check(sweep.Fired == sweep.Expected && sweepTiming.Count >= 3 * CHECK_RAMP_MIN_STEPS && sweepTiming.IsInRange(),
			std::format("Move swept up to {:.2f} m/s, down to 0 and up again over {:.0f} s legs: every footfall fires once, alternating within each run of steps ({} steps, {:.2f} to {:.2f} s apart)",
				RUN_SPEED, CHECK_RAMP_SECONDS, sweepTiming.Count, sweepTiming.Shortest, sweepTiming.Longest));

		const float ratio = WorstPopRatio(primary, baseline);
		GaitLog popped = primary;
		popped.FootPath[0][popped.FootPath[0].size() / 2].y += CHECK_POP_PROBE;
		const float probeRatio = WorstPopRatio(popped, baseline);
		DE_INFO("[INFO] foot acceleration over the {:.0f} s ramp reaches {:.2f}x the constant-Move peak at the same speed; a {:.0f} cm pop in one frame reads {:.1f}x", CHECK_RAMP_SECONDS, ratio, CHECK_POP_PROBE * 100.0f, probeRatio);
		report.Check(ratio <= CHECK_POP_RATIO && probeRatio > CHECK_POP_RATIO,
			std::format("no foot pops from idle to run: over the ramp at 60 Hz no frame's foot acceleration passes {:.1f}x the constant-Move peak at that speed (worst {:.2f}x), and the test does see a {:.0f} cm pop ({:.1f}x)",
				CHECK_POP_RATIO, ratio, CHECK_POP_PROBE * 100.0f, probeRatio));
	}

	int CountSteps(const Skeleton& skeleton, const AnimationState& state, float move)
	{
		Animator animator(&skeleton);
		animator.SetFloat(MOVE_PARAMETER, move);
		animator.Play(state);
		animator.Update(0.0f);

		int steps = 0;
		const int frames = static_cast<int>(std::lround(CHECK_ZONE_BLEND_SECONDS / CHECK_GAIT_STEP));
		for (int frame = 0; frame < frames; ++frame)
		{
			animator.Update(CHECK_GAIT_STEP);
			for (const AnimationEvent& event : animator.GetEventsThisFrame())
				steps += event.Type == AnimationEventType::Instant && (event.Name == Events::STEP_LEFT || event.Name == Events::STEP_RIGHT) ? 1 : 0;
		}
		return steps;
	}

	void CheckZoneStates(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states)
	{
		struct ZoneRate
		{
			const char* Name;
			const AnimationState* State;
			float Speed;
		};
		const std::array<ZoneRate, 3> zones = { {
			{ "backpedal", &states.Backward, BACKPEDAL_SPEED },
			{ "strafe left", &states.StrafeLeft, STRAFE_SPEED },
			{ "strafe right", &states.StrafeRight, STRAFE_SPEED } } };

		bool ok = true;
		std::string lines;
		for (const ZoneRate& zone : zones)
		{
			const AnimationClip* clip = zone.State->GetPoints().back().Clip;
			const float expected = 2.0f * CHECK_ZONE_BLEND_SECONDS * zone.State->GetSpeed() / clip->GetDuration();
			const int still = CountSteps(skeleton, *zone.State, 0.0f);
			const int slow = CountSteps(skeleton, *zone.State, 0.25f * zone.Speed);
			const int full = CountSteps(skeleton, *zone.State, zone.Speed);
			ok = ok && still == 0 && slow == 0 && std::abs(static_cast<float>(full) - expected) <= 1.0f;
			lines += std::format("{}{} {}/{}/{} (expected {:.1f})", lines.empty() ? "" : ", ", zone.Name, still, slow, full, expected);
		}
		report.Check(ok,
			std::format("the backpedal and strafe states are silent at Move 0 and a quarter of their speed, so a blocked fighter's feet stop, and step at their playback rate at full speed; steps in {:.0f} s at 0, a quarter and full speed: {}",
				CHECK_ZONE_BLEND_SECONDS, lines));
	}

	void CheckZoneSwitches(CheckReport& report, const Skeleton& skeleton, const LocomotionStates& states)
	{
		std::vector<GaitLog> logs;
		for (int phase = 0; phase < CHECK_ZONE_PHASES; ++phase)
		{
			const float hold = CHECK_ZONE_HOLD + CHECK_ZONE_STAGGER * static_cast<float>(phase);
			logs.push_back(RunGait(skeleton, states, ZoneScript(
				{ LocomotionZone::StrafeLeft, LocomotionZone::Backward, LocomotionZone::Forward }, { STRAFE_SPEED, BACKPEDAL_SPEED, RUN_SPEED }, hold)));
			logs.push_back(RunGait(skeleton, states, ZoneScript(
				{ LocomotionZone::Backward, LocomotionZone::StrafeRight, LocomotionZone::Forward }, { BACKPEDAL_SPEED, STRAFE_SPEED, RUN_SPEED }, hold)));
		}

		bool timingOk = true;
		size_t fewest = 1000000;
		float shortest = 1.0e9f;
		float longest = 0.0f;
		bool handoverOk = true;
		size_t handovers = 0;
		float nearest = 1.0e9f;
		float farthest = 0.0f;
		for (const GaitLog& log : logs)
		{
			const StepTiming timing = Timing(log.Steps);
			timingOk = timingOk && timing.Count >= CHECK_ZONE_MIN_STEPS && timing.IsInRange();
			fewest = std::min(fewest, timing.Count);
			shortest = std::min(shortest, timing.Shortest);
			longest = std::max(longest, timing.Longest);

			for (const size_t index : log.SwitchSteps)
			{
				++handovers;
				if (index == 0 || index >= log.Steps.size())
				{
					handoverOk = false;
					continue;
				}

				const float gap = log.Steps[index].Time - log.Steps[index - 1].Time;
				nearest = std::min(nearest, gap);
				farthest = std::max(farthest, gap);
				handoverOk = handoverOk && log.Steps[index].Foot != log.Steps[index - 1].Foot && gap >= CHECK_STEP_GAP_MIN && gap <= CHECK_GAP_MAX;
			}
		}

		report.Check(timingOk,
			std::format("{} runs through forward, strafe, backpedal and forward (and forward, backpedal, strafe, forward) with the switch at {} phases: every foot fires once per step with nothing filtered (at least {} steps a run, alternating steps {:.2f} to {:.2f} s apart, limit {:.2f} to {:.2f} s)",
				logs.size(), CHECK_ZONE_PHASES, fewest, shortest, longest, CHECK_STEP_GAP_MIN, CHECK_GAP_MAX));
		report.Check(handoverOk && handovers == logs.size() * 3,
			std::format("at all {} zone switches the first step after is the opposite foot of the last one before and lands {:.2f} to {:.2f} s after it (limit {:.2f} to {:.2f} s)",
				handovers, nearest, farthest, CHECK_STEP_GAP_MIN, CHECK_GAP_MAX));
	}
}

namespace Dingo
{

	bool RunMovementChecks(const GameAssets& assets)
	{
		CheckReport report;

		std::array<ClipContact, 5> contacts{};
		CheckFootContacts(report, assets, contacts);
		CheckSpeeds(report, contacts);
		CheckZones(report);
		CheckSeparation(report);

		const Model* knight = assets.GetCharacter(GetPlayerDef());
		const Skeleton* skeleton = knight ? knight->GetSkeleton() : nullptr;
		const LocomotionStates states = MakeLocomotionStates(assets.GetClips());
		report.Check(states.Complete && skeleton, "the locomotion states (blend, backpedal, two strafes) are built from the clips and the player has a skeleton");
		if (states.Complete && skeleton)
		{
			CheckGait(report, *skeleton, states);
			CheckZoneStates(report, *skeleton, states);
			CheckZoneSwitches(report, *skeleton, states);
		}

		if (report.GetFailed() == 0)
			DE_INFO("Movement checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		else
			DE_ERROR("Movement checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		return report.GetFailed() == 0;
	}

}
