#pragma once

// Engine-internal: drives each entity's Animator and answers where its joints are, for rendering
// and for children parented to a joint. Lives under src/ so EnTT stays private.

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <span>
#include <string_view>

namespace Dingo
{

	class Animator;
	class Skeleton;

	namespace Internal
	{

		namespace AnimationSystem
		{

			// Registers the hook that frees an animator with its AnimatorComponent.
			void Connect(entt::registry& registry);

			// The animate pass: gives every AnimatorComponent on a skinned model its animator (playing
			// DefaultClip when PlayOnStart is set), rebinds one whose model changed, and advances the
			// enabled ones by deltaTime x Speed.
			void Update(entt::registry& registry, float deltaTime);

			// Created on first use; nullptr without an AnimatorComponent and a skinned model.
			Animator* GetAnimator(entt::registry& registry, entt::entity handle);

			// The palette to draw `skeleton` with: the entity's animator's while it is bound to that
			// skeleton, else the rest palette.
			std::span<const glm::mat4> Palette(const entt::registry& registry, entt::entity handle, const Skeleton& skeleton);

			// The joint's frame in the entity's model space, from its animator's pose (the rest pose
			// without one): the joint's position and rotation, its scale left out. False when the entity
			// has no skinned model or the model no such joint.
			bool JointFrame(const entt::registry& registry, entt::entity handle, std::string_view joint, glm::mat4& frame);

		}

	}

}
