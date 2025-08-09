#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <vector>

#define BACKGROUND_SCROLLING_SPEED 0.5f

namespace Dingo
{

	enum class GameState
	{
		Menu,
		Game,
		Dead
	};

	struct Pipe
	{
		float x;
		float gapY;
		float gapHeight;
		bool scored = false;
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

	private:
		void UpdateGameStateMenu(float deltaTime, Renderer2D& renderer);
		void UpdateGameStateGame(float deltaTime, Renderer2D& renderer);
		void UpdateGameStateDead(float deltaTime, Renderer2D& renderer);

		void UpdateScrollingBackground(float deltaTime, Renderer2D& renderer);
		void UpdatePipes(float deltaTime, Renderer2D& renderer);
		void RenderPipes(Renderer2D& renderer);

		void UpdateBird(float deltaTime);
		void RenderBird(Renderer2D& renderer);

		void UpdateGround(float deltaTime);
		void RenderGround(Renderer2D& renderer);

		void RenderCenteredText(Renderer2D& renderer, const std::string& text, float fontSize, const glm::vec2& offset = glm::vec2(0.0f)) const;

	private:
		float m_OrthographicSize = 5.0f;
		float m_OrthographicNear = -1.0f;
		float m_OrthographicFar = 1.0f;
		glm::mat4 m_ProjectionViewMatrix = glm::mat4(1.0f);

		GameState m_GameState = GameState::Menu;
		float m_AspectRatio = 1.0f;
		float m_Width = 0.0f;
		float m_Height = 0.0f;
		uint32_t m_Score = 0;

		float m_BirdY = 0.0f;
		float m_BirdVelocity = 0.0f;
		float m_BirdWidth = 0.0f;
		float m_BirdHeight = 0.0f;
		const float m_Gravity = -2.5f;
		const float m_JumpVelocity = 1.5f;

		float m_BackgroundOffset = 0.0f;
		float m_BackgroundScrollSpeed = 0.0f; // Adjust for desired speed

		float m_GroundOffset = 0.0f;
		float m_GroundY = 0.0f;
		float m_GroundQuadWidth = 0.0f; // Width of the ground quad
		const float m_GroundHeight = 1.0f; // Height of the ground in world units

		std::vector<Pipe> m_Pipes;
		float m_PipeSpawnTimer = 0.0f;
		const float m_PipeSpawnInterval = 2.0f; // seconds
		const float m_PipeSpeed = 2.0f;         // units per second
		const float m_PipeWidth = 0.7f;
		const float m_PipeHeight = 6.0f;
		const float m_PipeGapHeight = 2.0f;

		Texture* m_MenuTexture = nullptr;

		Texture* m_BackgroundTexture = nullptr;
		Texture* m_GroundTexture = nullptr;
		Texture* m_PipeTexture = nullptr;
		Texture* m_BirdTexture = nullptr;

		Font* m_Font = nullptr;
	};

}
