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

#include <cstdint>

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

	}

}
