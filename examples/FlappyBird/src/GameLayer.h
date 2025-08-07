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

		float m_BirdY = 0.0f;
		float m_BirdVelocity = 0.0f;
		const float m_Gravity = -2.5f;
		const float m_JumpVelocity = 1.5f;

		float m_BackgroundOffset = 0.0f;
		const float m_BackgroundScrollSpeed = 0.5f; // Adjust for desired speed

		Texture* m_BackgroundTexture = nullptr;
		Texture* m_GroundTexture = nullptr;
		Texture* m_BirdTexture = nullptr;
	};

}
