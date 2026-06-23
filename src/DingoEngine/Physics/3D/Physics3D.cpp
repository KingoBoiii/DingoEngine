#include "depch.h"
#include "DingoEngine/Physics/3D/Physics3D.h"

#include "DingoEngine/Physics/3D/JoltPhysics/JoltPhysics3D.h"

namespace Dingo
{

	Physics3D* Physics3D::Create()
	{
		// Jolt is currently the only 3D backend. Future backends would be selected
		// here, mirroring Physics2D::Create / GraphicsContext::Create.
		return new JoltPhysics3D();
	}

}
