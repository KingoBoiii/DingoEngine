#include "GameLayer.h"

namespace Dingo
{

	void GameLayer::OnAttach()
	{}

	void GameLayer::OnDetach()
	{}

	void GameLayer::OnUpdate(float deltaTime)
	{
		Renderer2D& renderer = Application::Get().GetRenderer2D();

		renderer.BeginScene(glm::mat4(1.0f));
		renderer.Clear(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
		renderer.DrawQuad(glm::vec2(0.0f, 0.0f), glm::vec2(2.0f, 2.0f), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
		renderer.EndScene();
	}

#if DE_DEBUG
	void GameLayer::OnImGuiRender()
	{}
#endif

}
