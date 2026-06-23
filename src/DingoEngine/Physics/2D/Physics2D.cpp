#include "depch.h"
#include "DingoEngine/Physics/2D/Physics2D.h"

#include "DingoEngine/Physics/2D/Box2D/Box2DPhysics2D.h"

namespace Dingo
{

	Physics2D* Physics2D::Create()
	{
		// Box2D is currently the only 2D backend. Future backends would be selected
		// here, mirroring GraphicsContext::Create.
		return new Box2DPhysics2D();
	}

}
