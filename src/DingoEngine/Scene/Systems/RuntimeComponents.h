#pragma once

// Engine-internal: the live backend handles behind the public settings components,
// stored in the same registry but nameable only from src/. Keeping them out of
// Components.h is what makes copying or assigning a settings component safe — it can
// never carry another entity's body, controller or sound along with it.
//
// Each exists exactly while the thing it names is alive: emplaced when the body,
// controller or sound is created, removed when it is destroyed or its world stops.

#include "DingoEngine/Physics/2D/PhysicsTypes2D.h"
#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"
#include "DingoEngine/Audio/AudioTypes.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Graphics/Particles.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	namespace Internal
	{

		// The 2D colliders' shapes live on the body, so they are tracked with it.
		// Type is the body's, as built: the settings component may change after.
		struct RigidBody2DRuntime
		{
			PhysicsBodyId2D Body = 0;
			BodyType2D Type = BodyType2D::Static;
			PhysicsShapeId2D BoxShape = 0;
			PhysicsShapeId2D CircleShape = 0;
		};

		struct RigidBody3DRuntime
		{
			PhysicsBodyId3D Body = k_InvalidBody3D;
			BodyType3D Type = BodyType3D::Static;
		};

		// The Constraint3DComponent's built joint; Other is the entity it joins to, null for the world.
		struct Constraint3DRuntime
		{
			ConstraintId3D Constraint = k_InvalidConstraint3D;
			entt::entity Other = entt::null;
		};

		// Slot in PhysicsSync's controller store.
		struct CharacterController3DRuntime
		{
			std::uint32_t Index = 0;
		};

		// The local pose physics wrote at the last two fixed ticks (SceneParams::FixedTick), which
		// PhysicsInterpolation draws between. Dropped when the body or controller goes.
		struct InterpolatedPose3D
		{
			glm::vec3 PreviousPosition{ 0.0f };
			glm::vec3 Position{ 0.0f };
			glm::quat PreviousRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
			glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		};

		struct InterpolatedPose2D
		{
			glm::vec2 PreviousPosition{ 0.0f };
			glm::vec2 Position{ 0.0f };
			float PreviousRotation = 0.0f; // degrees, as TransformComponent
			float Rotation = 0.0f;
		};

		struct AudioSourceRuntime
		{
			AudioSoundId Sound = k_InvalidSound;
		};

		// Unlike the physics handles it survives OnStop/OnStart: animation needs no world. Freed with
		// the AnimatorComponent (AnimationSystem::Connect).
		struct AnimatorRuntime
		{
			std::unique_ptr<Animator> Instance;
			// Skeleton::GetId() of the model it was bound for, so a swapped or reloaded model rebinds
			// even at a freed one's address; 0 = none.
			uint64_t SkeletonId = 0;
			// Model::GetGeneration() it last posed for, so a paused animator shows a reload's keys.
			uint32_t ModelGeneration = 0;
			// Root motion set the entity's controller velocity last pass, so it is stopped once it no
			// longer does.
			bool DroveController = false;
			// Its entity wasn't drawn last frame (AnimatorComponent::CullingMode), and the updates since.
			bool Culled = false;
			uint32_t CulledUpdates = 0;
		};

		// A RagdollComponent's bodies and joints (RagdollSystem), dropped with the component, the entity's
		// deactivation or the 3D world, and rebuilt when the skeleton's id or revision or the recipe changes.
		struct RagdollRuntime
		{
			// Rigid: the scale a joint carries is the animated one, never simulated.
			struct Part
			{
				int32_t Joint = -1;
				int32_t Parent = -1; // index into Parts, below its own; -1 for the root body
				PhysicsBodyId3D Body = k_InvalidBody3D;
				ConstraintId3D Constraint = k_InvalidConstraint3D; // to Parent's body
				// The body's frame in its joint's world frame (joint position, rotation without scale).
				glm::vec3 OffsetPosition{ 0.0f };
				glm::quat OffsetRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
				float Mass = 0.0f;
				// The subtree's moment of inertia about the joint, which the motor's gains scale with.
				float Inertia = 0.0f;
				bool Hinge = false; // else swing-twist
				// The motors' torque cap at the last step; 0 while limp.
				float MaxTorque = 0.0f;
				// The body's world pose after the last two steps (RagdollRuntime::Recorded).
				glm::vec3 PreviousPosition{ 0.0f };
				glm::vec3 Position{ 0.0f };
				glm::quat PreviousRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
				glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
			};

			std::vector<Part> Parts; // parents first
			// Per skeleton joint, its part or its nearest simulated ancestor's; -1 above the root body.
			std::vector<int32_t> PartOfJoint;
			// The rest-pose animator posed in place of an AnimatorComponent's, for an entity without one.
			std::unique_ptr<Animator> Instance;
			uint64_t SkeletonId = 0;
			uint32_t SkeletonRevision = 0;
			// The RagdollComponent::Recipe and Model::GetRagdollRecipeRevision it was built with.
			std::string Recipe;
			uint32_t RecipeRevision = 0;
			std::uint32_t Group = k_NoCollisionGroup;
			// The entity's position followed the root body at the last step, so clip root motion stays
			// off and PhysicsInterpolation draws the entity between steps like a body.
			bool FollowsRoot = false;
			// The entity was carried down by a limp ragdoll. Its height above the ground under the root body
			// when it went limp, so a ragdoll driven again is put back that high above the ground it now lies on.
			bool Carried = false;
			bool HasGroundHeight = false;
			float GroundHeight = 0.0f;
			// A step has run since the build, so the parts' recorded poses are set.
			bool Recorded = false;
			// The animated root pose the last driven step aimed for, so the drive can match its velocity.
			glm::vec3 TargetPosition{ 0.0f };
			glm::quat TargetRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
			bool HasTarget = false;
		};

		// A ParticleEmitterComponent's emitter, made by the renderer that first draws it (ParticleSync).
		// Freed with the component; its particles vanish with it.
		struct ParticleEmitterRuntime
		{
			struct Burst
			{
				glm::vec3 Position{ 0.0f };
				bool AtPosition = false;
				uint32_t Count = 0;
				uint64_t Serial = 0;
			};

			// One emitter per Renderer3D that draws the entity (a minimap's beside the main view's),
			// each simulating on its own, since a renderer can only draw its own pool.
			struct Instance
			{
				std::shared_ptr<ParticleEmitter> Emitter;
				float PendingTime = 0.0f;  // the scene's time since it last stepped
				uint64_t BurstsTaken = 0;  // the serial of the last burst it was handed
				uint64_t LastFrame = 0;    // the frame it was last submitted in
				uint64_t RetryFrame = 0;   // without a ring (the pool was full), when to ask for one again
			};

			std::vector<Instance> Instances; // the first made first
			const ParticleEffect* Effect = nullptr;
			std::vector<Burst> Bursts;       // until every instance has taken them
			uint64_t NextBurst = 1;
		};

	}

}
