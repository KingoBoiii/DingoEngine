#pragma once

// Engine-internal: builds each RagdollComponent's bodies and joints from its skeleton, drives them
// toward the animated pose every physics step, and hands the simulated bones back to the animator.
// Owned by PhysicsSync, which calls it only while the 3D world is live.

#include "DingoEngine/Physics/3D/Physics3D.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class Animator;
	class Skeleton;

	namespace Internal
	{

		class RagdollSystem
		{
		public:
			// Drops ragdolls whose skeleton changed or lost a body, builds the pending ones from the current
			// pose, then sets every motor and root velocity for a step of deltaTime (0 builds only).
			void Sync(entt::registry& registry, Physics3D& physics, float deltaTime);

			// After the frame's animation and physics: blends the simulated bones into each animator by
			// RagdollComponent::Blend and moves a following entity onto its root body.
			void Pose(entt::registry& registry, Physics3D& physics);

			void Drop(entt::registry& registry, Physics3D* physics, entt::entity handle);
			// The world is going: forgets every ragdoll, its bodies left to the world's shutdown.
			void Clear(entt::registry& registry);

			bool OwnsBody(const entt::registry& registry, entt::entity handle, PhysicsBodyId3D body) const;
			// The joint's body, or its nearest simulated ancestor's; k_InvalidBody3D without one.
			PhysicsBodyId3D Body(const entt::registry& registry, entt::entity handle, std::string_view joint) const;

			static constexpr uint32_t k_MaxBodies = 20;

		private:
			void Build(entt::registry& registry, Physics3D& physics, entt::entity handle);
			void Drive(entt::registry& registry, Physics3D& physics, entt::entity handle, float deltaTime);

			// The animator posing the ragdoll's skeleton: the AnimatorComponent's, else the ragdoll's own.
			Animator* PosingAnimator(entt::registry& registry, entt::entity handle, const Skeleton& skeleton);
			// The world frame of every joint in the animated pose (no blend), into m_JointWorlds.
			bool AnimatedJointWorlds(entt::registry& registry, entt::entity handle, const Skeleton& skeleton, glm::mat4& entityWorld);

			// Entities whose ragdoll can't be built for this skeleton id and revision (logged once).
			struct Rejection
			{
				uint64_t SkeletonId = 0;
				uint32_t Revision = 0;
			};
			std::unordered_map<entt::entity, Rejection> m_Rejected;

			std::vector<entt::entity> m_Scratch;
			std::vector<glm::mat4> m_Globals;
			std::vector<glm::mat4> m_JointWorlds;
			std::vector<float> m_Weights;
		};

	}

}
