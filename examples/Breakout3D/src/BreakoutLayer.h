#pragma once
#include <DingoEngine.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace Dingo
{

	// -------------------------------------------------------
	// Game objects
	// -------------------------------------------------------

	struct Ball
	{
		glm::vec3 Position  = { 0.0f, 0.25f, 2.5f };
		glm::vec3 Velocity  = { 0.0f, 0.0f,  0.0f };
		float     Radius    = 0.25f;
		bool      Launched  = false;
	};

	struct Paddle
	{
		glm::vec3 Position = { 0.0f, 0.2f, 3.5f };
		glm::vec3 Size     = { 2.5f, 0.4f, 0.8f };
		float     Speed    = 8.0f;
	};

	struct Brick
	{
		glm::vec3 Position;
		glm::vec3 Size  = { 0.85f, 0.4f, 0.5f };
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool      Alive = true;
	};

	// -------------------------------------------------------
	// AABB helpers
	// -------------------------------------------------------

	struct AABB
	{
		glm::vec3 Min;
		glm::vec3 Max;

		static AABB FromCenterSize(const glm::vec3& center, const glm::vec3& size)
		{
			return { center - size * 0.5f, center + size * 0.5f };
		}
	};

	// -------------------------------------------------------
	// Layer
	// -------------------------------------------------------

	enum class BreakoutState { Menu, Playing, GameOver, Win };

	class BreakoutLayer : public Layer
	{
	public:
		BreakoutLayer() : Layer("Breakout3D") {}
		virtual ~BreakoutLayer() = default;

	public:
		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;
		void OnEvent(Event& event) override;
		void OnImGuiRender() override;

	private:
		void ResetGame();
		void BuildBricks();
		void UpdateGame(float dt);
		void RenderScene();
		void RenderUI();
		void DrawCenteredText(Renderer2D& r, const std::string& text, float size, float y, const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
		void UpdateOrthoProjection();

		bool CheckBallBrickCollision(const Ball& ball, const Brick& brick) const;
		void ResolveBallBrickCollision(Ball& ball, const Brick& brick);

	private:
		// Camera
		PerspectiveCamera m_Camera;
		float m_AspectRatio = 16.0f / 9.0f;

		// UI
		Font* m_Font = nullptr;
		glm::mat4 m_OrthoProjection = glm::mat4(1.0f);
		float m_OrthoSize = 5.0f;

		// Meshes
		Mesh* m_BoxMesh    = nullptr;
		Mesh* m_SphereMesh = nullptr;

		// Scene bounds (XZ play field)
		static constexpr float FieldHalfX   = 5.5f;   // side walls at ±FieldHalfX
		static constexpr float FieldFarZ    = -6.5f;  // back wall Z
		static constexpr float FieldNearZ   = 4.5f;   // death zone Z (past paddle)
		static constexpr float BallSpeed    = 7.0f;

		// Game state
		Ball                m_Ball;
		Paddle              m_Paddle;
		std::vector<Brick>  m_Bricks;
		int                 m_Lives = 3;
		int                 m_Score = 0;
		BreakoutState       m_State = BreakoutState::Menu;
	};

}
