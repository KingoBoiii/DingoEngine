#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	/// The collider a ragdoll recipe gives one bone.
	enum class RagdollShape : uint8_t
	{
		Capsule, ///< Along the bone, from the joint to its main child (the default).
		Sphere,  ///< Centred halfway along the bone.
		Box,     ///< Along the bone, Radius across on both other axes.
	};

	/// The joint a ragdoll recipe asks for between a bone and its parent body.
	enum class RagdollJointType : uint8_t
	{
		Auto,       ///< Chosen from the joint's name, as without a recipe.
		Hinge,      ///< One axis, the way the bone bends.
		SwingTwist, ///< A cone around the bone and a twist about it.
	};

	/// One joint's line in a `.ragdoll` recipe. A field left at its default keeps the automatic value.
	struct RagdollJointRecipe
	{
		std::string Joint; ///< The skeleton joint's name, as it appears in the file.
		RagdollShape Shape = RagdollShape::Capsule; ///< The bone's collider.
		/// The collider's radius (a box's half thickness) in the model's units, the space of
		/// Mesh::GetVertices(); 0 measures it from the vertices the bone moves.
		float Radius = 0.0f;
		/// The bone's share of RagdollComponent::TotalMass, above 0 and at most 1; 0 shares the rest by bone size.
		float MassShare = 0.0f;
		RagdollJointType Type = RagdollJointType::Auto; ///< Hinge or SwingTwist, or by name.
		/// Hinge limits in radians, from the bone lying straight along its parent (positive bends it
		/// further; a bone built straight turns positive about its joint's X axis); used when HasHingeLimits.
		float HingeMin = 0.0f;
		float HingeMax = 0.0f; ///< See HingeMin.
		bool HasHingeLimits = false; ///< True when HingeMin and HingeMax were given.
		/// SwingTwist cone half-angles in radians, about the two axes across the bone; used when HasSwing.
		float SwingY = 0.0f;
		float SwingZ = 0.0f; ///< See SwingY.
		bool HasSwing = false; ///< True when SwingY and SwingZ were given.
		/// SwingTwist twist limits in radians about the bone, from the build pose, so TwistMin <= 0 <= TwistMax;
		/// used when HasTwist.
		float TwistMin = 0.0f;
		float TwistMax = 0.0f; ///< See TwistMin.
		bool HasTwist = false; ///< True when TwistMin and TwistMax were given.
	};

	/// A named set of overrides for the ragdoll a RagdollComponent builds from a Model's skeleton: which
	/// joints get no body, and each body's collider, mass share and joint limits. Model loads them from
	/// its `.ragdoll` sidecar (docs/animation.md, "Ragdoll recipes").
	struct RagdollRecipe
	{
		/// Empty for the file's default recipe (its lines before the first `[name]`).
		std::string Name;
		/// Joints that get no body, nor do the joints below them; they ride on the nearest body above.
		std::vector<std::string> Excluded;
		std::vector<RagdollJointRecipe> Joints; ///< Per-joint overrides, at most one per joint.

		/// The joint's overrides, or nullptr when the recipe has none for it.
		const RagdollJointRecipe* FindJoint(std::string_view joint) const;
		/// True when the recipe excludes the joint itself (not counting an excluded ancestor).
		bool IsExcluded(std::string_view joint) const;
	};

}
