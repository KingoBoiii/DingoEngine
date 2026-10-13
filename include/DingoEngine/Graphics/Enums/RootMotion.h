#pragma once

#include <cstdint>

namespace Dingo
{

	/// Whether an Animator takes its clips' root travel out of the pose (Animator::SetRootMotion,
	/// AnimatorComponent::RootMotion).
	enum class RootMotionMode : uint8_t
	{
		/// The pose carries the travel, as clips play in place.
		Off,
		/// Horizontal travel and turning about the vertical; the root's height stays in the pose,
		/// so a jump still lifts the hips.
		XZ,
		/// Every axis of the root joint's travel and rotation.
		Full
	};

}
