#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <vector>

namespace Dingo
{

	struct Pipe
	{
		float x;
		float gapY;
		float gapHeight;
	};

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

		std::vector<Pipe> m_Pipes;
		float m_PipeSpawnTimer = 0.0f;
		const float m_PipeSpawnInterval = 2.0f; // seconds
		const float m_PipeSpeed = 2.0f;         // units per second
		const float m_PipeWidth = 0.7f;
		const float m_PipeHeight = 6.0f;
		const float m_PipeGapHeight = 2.0f;

		Texture* m_BackgroundTexture = nullptr;
		Texture* m_GroundTexture = nullptr;
		Texture* m_PipeTexture = nullptr;
		Texture* m_BirdTexture = nullptr;
	};

}
