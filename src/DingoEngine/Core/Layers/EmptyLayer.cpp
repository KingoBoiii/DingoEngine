#include "depch.h"
#include "EmptyLayer.h"
#include "DingoEngine/Graphics/Renderer.h"

namespace Dingo
{

	void EmptyLayer::OnUpdate(float deltaTime)
	{
		Renderer::Clear({ 1.0f, 0.0f, 1.0f, 1.0f });
	}

}
