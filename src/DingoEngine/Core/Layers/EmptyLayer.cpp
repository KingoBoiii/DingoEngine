#include "depch.h"
#include "EmptyLayer.h"
#include "DingoEngine/Core/Application.h"

namespace Dingo
{

	void EmptyLayer::OnUpdate(float deltaTime)
	{
		IRenderer& appRenderer = Application::Get().GetAppRenderer();

		appRenderer.Begin();
		appRenderer.Clear({ 1.0f, 0.0f, 1.0f, 1.0f });
		appRenderer.End();
	}

}
