#include "BreakoutLayer.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <format>

namespace
{
	constexpr const char* k_MeshShaderSource = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform CameraData { mat4 ViewProjection; };

layout(location = 0) out vec4 v_Color;

void main()
{
	gl_Position = ViewProjection * vec4(a_Position, 1.0);
	v_Color = a_Color;
}

#type fragment
#version 450

layout(location = 0) in  vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main() { o_Color = v_Color; }
)";

	static constexpr uint32_t k_MaxVertices = 65536;
	static constexpr uint32_t k_MaxIndices  = 65536;
}

namespace Dingo
{

	// Brick grid layout
	static constexpr int   BrickCols     = 7;
	static constexpr int   BrickRows     = 5;
	static constexpr float BrickSpacingX = 1.4f;
	static constexpr float BrickSpacingZ = 0.85f;
	static constexpr float BrickStartX   = -(BrickCols - 1) * BrickSpacingX * 0.5f;
	static constexpr float BrickStartZ   = -2.5f;

	// Row colors (front → back)
	static constexpr glm::vec4 BrickColors[BrickRows] = {
		{ 1.0f, 0.35f, 0.15f, 1.0f },  // orange
		{ 1.0f, 0.85f, 0.10f, 1.0f },  // yellow
		{ 0.20f, 0.85f, 0.30f, 1.0f }, // green
		{ 0.15f, 0.75f, 1.0f, 1.0f },  // cyan
		{ 0.70f, 0.25f, 1.0f, 1.0f },  // purple
	};

	// -------------------------------------------------------
	// Lifecycle
	// -------------------------------------------------------

	void BreakoutLayer::OnAttach()
	{
		m_BoxMesh    = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere(0.5f, 16, 16);

		const Window& window = Application::Get().GetWindow();
		m_AspectRatio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 200.0f);
		m_Camera.SetPosition({ 0.0f, 13.0f, 15.0f });
		m_Camera.SetTarget({ 0.0f, 0.0f, -1.0f });

		m_Font = Font::Create("assets/fonts/arialbd.ttf");
		UpdateOrthoProjection();

		InitScene3D();
		ResetGame();
	}

	void BreakoutLayer::OnDetach()
	{
		ShutdownScene3D();

		m_Font->Destroy();
		delete m_Font;
		m_Font = nullptr;

		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh    = nullptr;
		m_SphereMesh = nullptr;
	}

	void BreakoutLayer::OnUpdate(float deltaTime)
	{
		if (m_State == BreakoutState::Playing)
			UpdateGame(deltaTime);

		RenderScene();
		RenderUI();
	}

	void BreakoutLayer::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e)
		{
			if (e.GetWidth() > 0 && e.GetHeight() > 0)
			{
				m_AspectRatio = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
				m_Camera.SetAspectRatio(m_AspectRatio);
				UpdateOrthoProjection();
			}
			return false;
		});
		dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)
		{
			if (e.GetKeyCode() == Key::Space)
			{
				if ((m_State == BreakoutState::Menu || m_State == BreakoutState::Playing) && !m_Ball.Launched)
				{
					m_State = BreakoutState::Playing;
					m_Ball.Launched = true;
					m_Ball.Velocity = { 3.5f, 0.0f, -BallSpeed };
				}
				else if (m_State == BreakoutState::GameOver || m_State == BreakoutState::Win)
				{
					ResetGame();
				}
			}
			return false;
		});
	}

#ifndef DE_DISTRIBUTION
	void BreakoutLayer::OnImGuiRender()
	{
		ImGui::Begin("Breakout 3D");

		switch (m_State)
		{
			case BreakoutState::Menu:
				ImGui::Text("Press SPACE to launch the ball");
				break;
			case BreakoutState::Playing:
				ImGui::Text("Score: %d", m_Score);
				ImGui::Text("Lives: %d", m_Lives);
				ImGui::Text("Bricks left: %d",
					static_cast<int>(std::count_if(m_Bricks.begin(), m_Bricks.end(),
						[](const Brick& b) { return b.Alive; })));
				ImGui::Separator();
				ImGui::Text("A/D or Left/Right: move paddle");
				break;
			case BreakoutState::GameOver:
				ImGui::TextColored({ 1,0.2f,0.2f,1 }, "GAME OVER");
				ImGui::Text("Final score: %d", m_Score);
				ImGui::Text("Press SPACE to restart");
				break;
			case BreakoutState::Win:
				ImGui::TextColored({ 0.2f,1,0.2f,1 }, "YOU WIN!");
				ImGui::Text("Final score: %d", m_Score);
				ImGui::Text("Press SPACE to play again");
				break;
		}

		ImGui::End();
	}
#endif

	// -------------------------------------------------------
	// Game logic
	// -------------------------------------------------------

	void BreakoutLayer::ResetGame()
	{
		m_Ball.Position = { 0.0f, 0.25f, 2.5f };
		m_Ball.Velocity = { 0.0f, 0.0f, 0.0f };
		m_Ball.Launched = false;

		m_Paddle.Position = { 0.0f, 0.2f, 3.5f };

		m_Lives = 3;
		m_Score = 0;
		m_State = BreakoutState::Menu;

		BuildBricks();
	}

	void BreakoutLayer::BuildBricks()
	{
		m_Bricks.clear();
		m_Bricks.reserve(BrickCols * BrickRows);

		for (int row = 0; row < BrickRows; ++row)
		{
			for (int col = 0; col < BrickCols; ++col)
			{
				Brick brick;
				brick.Position = {
					BrickStartX + col * BrickSpacingX,
					0.2f,
					BrickStartZ - row * BrickSpacingZ
				};
				brick.Color = BrickColors[row];
				m_Bricks.push_back(brick);
			}
		}
	}

	void BreakoutLayer::UpdateGame(float dt)
	{
		// ── Paddle movement ──────────────────────────────────────
		const float paddleHalfX = m_Paddle.Size.x * 0.5f;

		if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))
			m_Paddle.Position.x -= m_Paddle.Speed * dt;
		if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right))
			m_Paddle.Position.x += m_Paddle.Speed * dt;

		m_Paddle.Position.x = std::clamp(m_Paddle.Position.x,
			-FieldHalfX + paddleHalfX, FieldHalfX - paddleHalfX);

		if (!m_Ball.Launched)
		{
			// Ball sits on paddle until launched
			m_Ball.Position.x = m_Paddle.Position.x;
			return;
		}

		// ── Ball movement ─────────────────────────────────────────
		m_Ball.Position += m_Ball.Velocity * dt;

		// Side walls
		if (m_Ball.Position.x - m_Ball.Radius < -FieldHalfX)
		{
			m_Ball.Position.x = -FieldHalfX + m_Ball.Radius;
			m_Ball.Velocity.x = std::abs(m_Ball.Velocity.x);
		}
		if (m_Ball.Position.x + m_Ball.Radius > FieldHalfX)
		{
			m_Ball.Position.x = FieldHalfX - m_Ball.Radius;
			m_Ball.Velocity.x = -std::abs(m_Ball.Velocity.x);
		}

		// Back wall
		if (m_Ball.Position.z - m_Ball.Radius < FieldFarZ)
		{
			m_Ball.Position.z = FieldFarZ + m_Ball.Radius;
			m_Ball.Velocity.z = std::abs(m_Ball.Velocity.z);
		}

		// Paddle collision
		AABB paddleBox = AABB::FromCenterSize(m_Paddle.Position, m_Paddle.Size);
		AABB ballBox   = AABB::FromCenterSize(m_Ball.Position, glm::vec3(m_Ball.Radius * 2.0f));

		auto overlaps = [](const AABB& a, const AABB& b)
		{
			return a.Min.x < b.Max.x && a.Max.x > b.Min.x &&
			       a.Min.z < b.Max.z && a.Max.z > b.Min.z;
		};

		if (m_Ball.Velocity.z > 0.0f && overlaps(ballBox, paddleBox))
		{
			// Reflect Z; add X influence based on hit offset from paddle center
			float hitOffset = (m_Ball.Position.x - m_Paddle.Position.x) / (m_Paddle.Size.x * 0.5f);
			m_Ball.Velocity.z = -std::abs(m_Ball.Velocity.z);
			m_Ball.Velocity.x = hitOffset * BallSpeed * 0.7f;
			// Clamp speed
			float speed = glm::length(glm::vec2(m_Ball.Velocity.x, m_Ball.Velocity.z));
			if (speed > BallSpeed * 1.2f)
			{
				m_Ball.Velocity.x *= BallSpeed * 1.2f / speed;
				m_Ball.Velocity.z *= BallSpeed * 1.2f / speed;
			}
			m_Ball.Position.z = paddleBox.Min.z - m_Ball.Radius;
		}

		// Death zone (ball passed paddle)
		if (m_Ball.Position.z > FieldNearZ)
		{
			--m_Lives;
			if (m_Lives <= 0)
			{
				m_State = BreakoutState::GameOver;
				return;
			}
			// Respawn ball on paddle
			m_Ball.Position = { m_Paddle.Position.x, 0.25f, m_Paddle.Position.z - 1.0f };
			m_Ball.Velocity = { 0.0f, 0.0f, 0.0f };
			m_Ball.Launched = false;
		}

		// ── Brick collisions ──────────────────────────────────────
		bool anyAlive = false;
		for (auto& brick : m_Bricks)
		{
			if (!brick.Alive) continue;
			anyAlive = true;

			if (CheckBallBrickCollision(m_Ball, brick))
			{
				ResolveBallBrickCollision(m_Ball, brick);
				brick.Alive = false;
				m_Score += 10;
			}
		}

		if (!anyAlive)
			m_State = BreakoutState::Win;
	}

	bool BreakoutLayer::CheckBallBrickCollision(const Ball& ball, const Brick& brick) const
	{
		AABB box = AABB::FromCenterSize(brick.Position, brick.Size);

		// Closest point on AABB to ball center (XZ only)
		float cx = std::clamp(ball.Position.x, box.Min.x, box.Max.x);
		float cz = std::clamp(ball.Position.z, box.Min.z, box.Max.z);

		float dx = ball.Position.x - cx;
		float dz = ball.Position.z - cz;
		return (dx * dx + dz * dz) <= (ball.Radius * ball.Radius);
	}

	void BreakoutLayer::ResolveBallBrickCollision(Ball& ball, const Brick& brick)
	{
		// Overlap in each axis
		float overlapX = (ball.Radius + brick.Size.x * 0.5f) - std::abs(ball.Position.x - brick.Position.x);
		float overlapZ = (ball.Radius + brick.Size.z * 0.5f) - std::abs(ball.Position.z - brick.Position.z);

		if (overlapX < overlapZ)
		{
			ball.Velocity.x = -ball.Velocity.x;
			float sign = (ball.Position.x < brick.Position.x) ? -1.0f : 1.0f;
			ball.Position.x += sign * overlapX;
		}
		else
		{
			ball.Velocity.z = -ball.Velocity.z;
			float sign = (ball.Position.z < brick.Position.z) ? -1.0f : 1.0f;
			ball.Position.z += sign * overlapZ;
		}
	}

	// -------------------------------------------------------
	// 3D scene management
	// -------------------------------------------------------

	void BreakoutLayer::InitScene3D()
	{
		auto& s = m_Scene3D;

		s.VertexBuffer     = GraphicsBuffer::CreateVertexBuffer(sizeof(MeshVertex) * k_MaxVertices, nullptr, true, "Breakout3D_VB");
		s.IndexBuffer      = GraphicsBuffer::CreateIndexBuffer(sizeof(uint16_t) * k_MaxIndices, nullptr, true, "Breakout3D_IB");
		s.VertexBufferBase = new MeshVertex[k_MaxVertices];
		s.IndexBufferBase  = new uint16_t[k_MaxIndices];

		s.MeshShader = Shader::CreateFromSource("Breakout3DMeshShader", k_MeshShaderSource);

		s.MeshLayout = VertexLayout()
			.SetStride(sizeof(MeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT,  offsetof(MeshVertex, Position))
			.AddAttribute("a_Color",    Format::RGBA32_FLOAT, offsetof(MeshVertex, Color));

		s.MeshMaterial = Material::Create(MaterialParams()
			.SetDebugName("Breakout3D_Material")
			.SetShader(s.MeshShader)
			.SetCullMode(CullMode::None));

		// Allocate the GPU uniform buffer by uploading the initial (zero) camera data.
		s.MeshMaterial->SetUniform(s.CameraData);
	}

	void BreakoutLayer::ShutdownScene3D()
	{
		auto& s = m_Scene3D;

		delete[] s.VertexBufferBase;
		delete[] s.IndexBufferBase;
		s.VertexBufferBase = nullptr;
		s.IndexBufferBase  = nullptr;

		if (s.VertexBuffer)  { s.VertexBuffer->Destroy();  s.VertexBuffer  = nullptr; }
		if (s.IndexBuffer)   { s.IndexBuffer->Destroy();   s.IndexBuffer   = nullptr; }
		if (s.MeshMaterial)  { s.MeshMaterial->Destroy();  delete s.MeshMaterial; s.MeshMaterial = nullptr; }
		if (s.MeshShader)    { s.MeshShader->Destroy();    s.MeshShader    = nullptr; }
	}

	void BreakoutLayer::BeginScene3D(const PerspectiveCamera& camera, const glm::vec4& clearColor)
	{
		auto& s = m_Scene3D;
		s.ClearColor = clearColor;

		s.CameraData.ViewProjection = camera.GetViewProjectionMatrix();
		s.MeshMaterial->SetUniform(s.CameraData);

		s.VertexBufferPtr = s.VertexBufferBase;
		s.IndexBufferPtr  = s.IndexBufferBase;
		s.IndexCount      = 0;
		s.VertexOffset    = 0;
	}

	void BreakoutLayer::SubmitMesh(Mesh* mesh, const glm::mat4& transform, const glm::vec4& color)
	{
		auto& s = m_Scene3D;
		for (const auto& v : mesh->GetVertices())
		{
			s.VertexBufferPtr->Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			s.VertexBufferPtr->Color    = color;
			s.VertexBufferPtr++;
		}
		for (uint16_t idx : mesh->GetIndices())
			*s.IndexBufferPtr++ = idx + s.VertexOffset;

		s.VertexOffset += static_cast<uint16_t>(mesh->GetVertices().size());
		s.IndexCount   += static_cast<uint32_t>(mesh->GetIndices().size());
	}

	void BreakoutLayer::FlushScene3D()
	{
		auto& s = m_Scene3D;

		Renderer::Clear(s.ClearColor);

		if (s.IndexCount == 0)
			return;

		uint32_t vertexDataSize = static_cast<uint32_t>(
			reinterpret_cast<uint8_t*>(s.VertexBufferPtr) - reinterpret_cast<uint8_t*>(s.VertexBufferBase));

		s.VertexBuffer->Upload(s.VertexBufferBase, vertexDataSize);
		s.IndexBuffer->Upload(s.IndexBufferBase, s.IndexCount * sizeof(uint16_t));

		Renderer::DrawIndexed(s.MeshMaterial, s.MeshLayout, s.VertexBuffer, s.IndexBuffer, s.IndexCount);
	}

	// -------------------------------------------------------
	// Rendering
	// -------------------------------------------------------

	void BreakoutLayer::RenderScene()
	{
		BeginScene3D(m_Camera, { 0.04f, 0.04f, 0.10f, 1.0f });

		// ── Floor (subtle dark slab) ──────────────────────────────
		{
			glm::mat4 t = glm::translate(glm::mat4(1.0f), { 0.0f, -0.15f, -1.5f });
			t = glm::scale(t, { 12.0f, 0.1f, 15.0f });
			SubmitMesh(m_BoxMesh, t, { 0.08f, 0.08f, 0.12f, 1.0f });
		}

		// ── Side walls ────────────────────────────────────────────
		{
			glm::mat4 tL = glm::translate(glm::mat4(1.0f), { -FieldHalfX - 0.25f, 0.5f, -1.5f });
			tL = glm::scale(tL, { 0.5f, 1.0f, 14.0f });
			SubmitMesh(m_BoxMesh, tL, { 0.2f, 0.2f, 0.3f, 1.0f });

			glm::mat4 tR = glm::translate(glm::mat4(1.0f), { FieldHalfX + 0.25f, 0.5f, -1.5f });
			tR = glm::scale(tR, { 0.5f, 1.0f, 14.0f });
			SubmitMesh(m_BoxMesh, tR, { 0.2f, 0.2f, 0.3f, 1.0f });
		}

		// ── Bricks (back rows first for painter's algorithm) ──────
		for (int row = BrickRows - 1; row >= 0; --row)
		{
			for (int col = 0; col < BrickCols; ++col)
			{
				const Brick& brick = m_Bricks[row * BrickCols + col];
				if (!brick.Alive) continue;

				glm::mat4 t = glm::translate(glm::mat4(1.0f), brick.Position);
				t = glm::scale(t, brick.Size);
				SubmitMesh(m_BoxMesh, t, brick.Color);
			}
		}

		// ── Ball ──────────────────────────────────────────────────
		{
			glm::mat4 t = glm::translate(glm::mat4(1.0f), m_Ball.Position);
			t = glm::scale(t, glm::vec3(m_Ball.Radius * 2.0f));
			SubmitMesh(m_SphereMesh, t, { 1.0f, 0.75f, 0.15f, 1.0f });
		}

		// ── Paddle (drawn last — closest to camera) ───────────────
		{
			glm::mat4 t = glm::translate(glm::mat4(1.0f), m_Paddle.Position);
			t = glm::scale(t, m_Paddle.Size);
			SubmitMesh(m_BoxMesh, t, { 0.30f, 0.55f, 1.0f, 1.0f });
		}

		FlushScene3D();
	}

	void BreakoutLayer::UpdateOrthoProjection()
	{
		float halfH = m_OrthoSize * 0.5f;
		float halfW = halfH * m_AspectRatio;
		m_OrthoProjection = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
	}

	void BreakoutLayer::DrawCenteredText(Renderer2D& r, const std::string& text, float size, float y, const glm::vec4& color)
	{
		float textWidth = m_Font->GetStringWidth(text, size);
		r.DrawText(text, m_Font, glm::vec2(-textWidth * 0.5f, y), size, { color });
	}

	void BreakoutLayer::RenderUI()
	{
		if (!m_Font)
			return;

		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * m_AspectRatio;
		const float padding = 0.2f;

		Renderer2D& r = Application::Get().GetRenderer2D();
		r.BeginScene(m_OrthoProjection);

		switch (m_State)
		{
			case BreakoutState::Menu:
				DrawCenteredText(r, "BREAKOUT 3D",        0.9f,  halfH * 0.35f, { 1.0f, 0.85f, 0.20f, 1.0f });
				DrawCenteredText(r, "Press SPACE to launch", 0.45f, -halfH * 0.15f);
				break;

			case BreakoutState::Playing:
				r.DrawText(std::format("SCORE  {}", m_Score), m_Font, glm::vec2(-halfW + padding,  halfH - 0.55f), 0.45f);
				r.DrawText(std::format("LIVES  {}", m_Lives),  m_Font, glm::vec2(-halfW + padding,  halfH - 1.05f), 0.45f);
				break;

			case BreakoutState::GameOver:
				DrawCenteredText(r, "GAME OVER",              0.9f,   halfH * 0.30f, { 1.0f, 0.25f, 0.20f, 1.0f });
				DrawCenteredText(r, std::format("Score: {}", m_Score), 0.45f, -halfH * 0.10f);
				DrawCenteredText(r, "Press SPACE to restart", 0.45f, -halfH * 0.50f);
				break;

			case BreakoutState::Win:
				DrawCenteredText(r, "YOU WIN!",                 0.9f,   halfH * 0.30f, { 0.25f, 1.0f, 0.40f, 1.0f });
				DrawCenteredText(r, std::format("Score: {}", m_Score),   0.45f, -halfH * 0.10f);
				DrawCenteredText(r, "Press SPACE to play again", 0.45f, -halfH * 0.50f);
				break;
		}

		r.EndScene();
	}

}
