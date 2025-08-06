#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

namespace Dingo
{

	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Flappy Bird Game Layer")
		{}
		virtual ~GameLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;

#if DE_DEBUG
		virtual void OnImGuiRender() override;
#endif

	private:
		float m_OrthographicSize = 5.0f;
		float m_OrthographicNear = -1.0f;
		float m_OrthographicFar = 1.0f;
		glm::mat4 m_ProjectionViewMatrix = glm::mat4(1.0f);

		Texture* m_BackgroundTexture = nullptr;
	};

}
