#pragma once

#include <cstdint>

namespace Dingo
{

	// Whether an Animator takes its clips' root travel out of the pose (Animator::SetRootMotion,
	// AnimatorComponent::RootMotion).
	enum class RootMotionMode : uint8_t
	{
		Off,  // the pose carries the travel, as clips play in place
		XZ,   // horizontal travel and turning about the vertical; the root's height stays in the pose, so a jump still lifts the hips
		Full  // every axis of the root joint's travel and rotation
	};

}
