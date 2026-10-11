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

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Dingo
{

	namespace Internal
	{

		// The 2D colliders' shapes live on the body, so they are tracked with it.
		struct RigidBody2DRuntime
		{
			PhysicsBodyId2D Body = 0;
			PhysicsShapeId2D BoxShape = 0;
			PhysicsShapeId2D CircleShape = 0;
		};

		struct RigidBody3DRuntime
		{
			PhysicsBodyId3D Body = k_InvalidBody3D;
		};

		// Slot in PhysicsSync's controller store.
		struct CharacterController3DRuntime
		{
			std::uint32_t Index = 0;
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
