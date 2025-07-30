#include "depch.h"
#include "EmptyLayer.h"
#include "DingoEngine/Core/Application.h"

namespace Dingo
{

	void EmptyLayer::OnUpdate(float deltaTime)
	{
		Renderer& renderer = Application::Get().GetRenderer();

		renderer.Begin();
		renderer.Clear({ 1.0f, 0.0f, 1.0f, 1.0f });
		renderer.End();
	}

}
