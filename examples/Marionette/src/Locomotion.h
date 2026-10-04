#pragma once
#include "Moveset.h"

#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>

namespace Dingo
{

	enum class LocomotionZone : uint8_t
	{
		Forward,
		StrafeLeft,
		StrafeRight,
		Backward
	};

	const char* ToString(LocomotionZone zone);

	// `travel` is the travel direction minus the facing, in radians, positive towards the fighter's
	// left. Each zone keeps its range until the direction is ZONE_HYSTERESIS_DEG past the edge.
	LocomotionZone PickZone(LocomotionZone current, float travel);

	// The direction the zone's clip carries the body in, relative to the body's facing.
	float ZoneAngle(LocomotionZone zone, float travel);

	struct LocomotionStates
	{
		AnimationState Blend;
		AnimationState StrafeLeft;
		AnimationState StrafeRight;
		AnimationState Backward;
		bool Complete = false;

		const AnimationState& For(LocomotionZone zone) const;
	};

	LocomotionStates MakeLocomotionStates(const ClipSet& clips);

	float FootfallPhase(const AnimationClip* clip, std::string_view event);

	void SwitchZone(Animator& animator, const AnimationState& from, const AnimationState& to, float step);

	struct SeparationBody
	{
		glm::vec2 Position{ 0.0f };
		glm::vec2 Velocity{ 0.0f };
		float Radius = 0.0f;
	};

	void ResolveSeparation(SeparationBody& a, SeparationBody& b, float deltaTime);

}
