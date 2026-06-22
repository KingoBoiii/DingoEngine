#include "PhysicsLayer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <format>

namespace
{
	// Minimal flat-shaded mesh shader (vertex colour), same approach as Breakout3D.
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

	constexpr uint32_t k_MaxVertices = 65536;
	constexpr uint32_t k_MaxIndices  = 65536;
}

namespace Dingo
{

	// --- Tuning ---
	static const glm::vec3 GRAVITY        = { 0.0f, -9.81f, 0.0f };
	static constexpr int   TOWER_COLS     = 4;
	static constexpr int   TOWER_ROWS     = 6;
	static const glm::vec3 BLOCK_HALF     = { 0.5f, 0.5f, 0.5f };
	static constexpr float BLOCK_GAP      = 1.02f;
	static constexpr float FLOOR_TOP_Y    = 0.0f;
	static constexpr float LAUNCH_Z       = 13.0f;
	static constexpr float LAUNCH_Y       = 2.0f;
	static constexpr float LAUNCH_SPEED   = 26.0f;
	static constexpr float SPHERE_RADIUS  = 0.6f;
	static constexpr float AIM_RANGE      = 6.0f;
	static constexpr float AIM_SPEED      = 7.0f;

	static const glm::vec4 ROW_COLORS[TOWER_ROWS] = {
		{ 0.95f, 0.40f, 0.30f, 1.0f },
		{ 0.97f, 0.70f, 0.25f, 1.0f },
		{ 0.55f, 0.85f, 0.35f, 1.0f },
		{ 0.30f, 0.80f, 0.75f, 1.0f },
		{ 0.40f, 0.60f, 0.95f, 1.0f },
		{ 0.70f, 0.45f, 0.90f, 1.0f },
	};

	// ----------------------------------------------------------------------
	// Lifecycle
	// ----------------------------------------------------------------------

	void PhysicsLayer::OnAttach()
	{
		m_BoxMesh    = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere(0.5f, 16, 16);

		const Window& window = Application::Get().GetWindow();
		m_AspectRatio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 500.0f);
		m_Camera.SetPosition({ 0.0f, 7.0f, 16.0f });
		m_Camera.SetTarget({ 0.0f, 2.5f, 0.0f });

		m_Font = Font::Create("assets/fonts/arialbd.ttf");
		UpdateOrthoProjection();

		InitScene3D();

		PhysicsWorld3DParams params;
		params.Gravity = GRAVITY;
		m_World = PhysicsWorld3D::Create(params);

		BuildScene();
	}

	void PhysicsLayer::OnDetach()
	{
		if (m_World)
		{
			m_World->Destroy();
			m_World = nullptr;
		}

		ShutdownScene3D();

		if (m_Font)
		{
			m_Font->Destroy();
			m_Font = nullptr;
		}

		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh = nullptr;
		m_SphereMesh = nullptr;
	}

	void PhysicsLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyDown(Key::Escape))
			Application::Get().Close();

		// Aim: shift the launch point left/right.
		if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  m_LaunchX -= AIM_SPEED * deltaTime;
		if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) m_LaunchX += AIM_SPEED * deltaTime;
		m_LaunchX = glm::clamp(m_LaunchX, -AIM_RANGE, AIM_RANGE);

		if (Input::IsKeyDown(Key::Space))
			FireSphere();
		if (Input::IsKeyDown(Key::R))
			ResetScene();

		m_World->Step(deltaTime);

		RenderScene();
		RenderUI();
	}

	void PhysicsLayer::OnEvent(Event& event)
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
	}

	// ----------------------------------------------------------------------
	// Scene / gameplay
	// ----------------------------------------------------------------------

	PhysicsLayer::Body& PhysicsLayer::AddBox(const glm::vec3& position, const glm::vec3& halfExtents, const glm::vec4& color, BodyType3D type)
	{
		RigidBodyParams3D params(type, ColliderShape3D::Box);
		params.Position = position;
		params.HalfExtents = halfExtents;
		params.Friction = 0.6f;
		params.Restitution = 0.05f;

		Body body;
		body.Id = m_World->CreateBody(params);
		body.IsSphere = false;
		body.HalfExtents = halfExtents;
		body.Color = color;
		m_Bodies.push_back(body);
		return m_Bodies.back();
	}

	PhysicsLayer::Body& PhysicsLayer::AddSphere(const glm::vec3& position, float radius, const glm::vec4& color, BodyType3D type)
	{
		RigidBodyParams3D params(type, ColliderShape3D::Sphere);
		params.Position = position;
		params.Radius = radius;
		params.Friction = 0.5f;
		params.Restitution = 0.25f;

		Body body;
		body.Id = m_World->CreateBody(params);
		body.IsSphere = true;
		body.Radius = radius;
		body.Color = color;
		m_Bodies.push_back(body);
		return m_Bodies.back();
	}

	void PhysicsLayer::BuildScene()
	{
		// Static floor (top surface at y = FLOOR_TOP_Y).
		AddBox({ 0.0f, FLOOR_TOP_Y - 0.5f, 0.0f }, { 20.0f, 0.5f, 20.0f }, { 0.28f, 0.31f, 0.36f, 1.0f }, BodyType3D::Static);

		// A wall/tower of dynamic boxes resting on the floor.
		for (int row = 0; row < TOWER_ROWS; row++)
		{
			for (int col = 0; col < TOWER_COLS; col++)
			{
				const glm::vec3 position = {
					(col - (TOWER_COLS - 1) * 0.5f) * BLOCK_GAP,
					FLOOR_TOP_Y + BLOCK_HALF.y + row * BLOCK_GAP,
					0.0f
				};
				AddBox(position, BLOCK_HALF, ROW_COLORS[row], BodyType3D::Dynamic);
			}
		}
	}

	void PhysicsLayer::ResetScene()
	{
		for (const Body& body : m_Bodies)
			m_World->DestroyBody(body.Id);
		m_Bodies.clear();
		m_Shots = 0;
		BuildScene();
	}

	void PhysicsLayer::FireSphere()
	{
		const glm::vec3 launch = { m_LaunchX, LAUNCH_Y, LAUNCH_Z };
		const PhysicsBodyId3D id = AddSphere(launch, SPHERE_RADIUS, { 1.0f, 0.85f, 0.20f, 1.0f }, BodyType3D::Dynamic).Id;

		// Aim at the base of the tower (slightly toward the centre).
		const glm::vec3 target = { m_LaunchX * 0.3f, LAUNCH_Y, 0.0f };
		const glm::vec3 direction = glm::normalize(target - launch);
		m_World->SetLinearVelocity(id, direction * LAUNCH_SPEED);

		m_Shots++;
	}

	// ----------------------------------------------------------------------
	// Rendering
	// ----------------------------------------------------------------------

	void PhysicsLayer::RenderScene()
	{
		BeginScene3D(m_Camera, m_Scene3D.ClearColor);

		for (const Body& body : m_Bodies)
		{
			glm::mat4 transform = m_World->GetTransform(body.Id);
			if (body.IsSphere)
			{
				transform = glm::scale(transform, glm::vec3(body.Radius * 2.0f));
				SubmitMesh(m_SphereMesh, transform, body.Color);
			}
			else
			{
				transform = glm::scale(transform, body.HalfExtents * 2.0f);
				SubmitMesh(m_BoxMesh, transform, body.Color);
			}
		}

		FlushScene3D();
	}

	void PhysicsLayer::UpdateOrthoProjection()
	{
		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * m_AspectRatio;
		m_OrthoProjection = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
	}

	void PhysicsLayer::DrawCenteredText(Renderer2D& r, const std::string& text, float size, float y, const glm::vec4& color)
	{
		const float width = m_Font->GetStringWidth(text, size);
		r.DrawText(text, m_Font, glm::vec2(-width * 0.5f, y), size, { color });
	}

	void PhysicsLayer::RenderUI()
	{
		if (!m_Font)
			return;

		const float halfH = m_OrthoSize * 0.5f;
		const float halfW = halfH * m_AspectRatio;
		const float padding = 0.2f;

		Renderer2D& r = Application::Get().GetRenderer2D();
		r.BeginScene(m_OrthoProjection);

		r.DrawText(std::format("SHOTS  {}", m_Shots), m_Font, glm::vec2(-halfW + padding, halfH - 0.55f), 0.45f);
		DrawCenteredText(r, "SPACE: fire    A/D: aim    R: reset", 0.4f, -halfH + 0.3f, { 0.85f, 0.87f, 0.95f, 1.0f });

		r.EndScene();
	}

	// ----------------------------------------------------------------------
	// Minimal 3D mesh batcher (mirrors the Breakout3D example)
	// ----------------------------------------------------------------------

	void PhysicsLayer::InitScene3D()
	{
		auto& s = m_Scene3D;

		s.VertexBuffer     = GraphicsBuffer::CreateVertexBuffer(sizeof(MeshVertex) * k_MaxVertices, nullptr, true, "Physics3D_VB");
		s.IndexBuffer      = GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * k_MaxIndices, nullptr, true, "Physics3D_IB", GraphicsFormat::Uint32);
		s.VertexBufferBase = new MeshVertex[k_MaxVertices];
		s.IndexBufferBase  = new uint32_t[k_MaxIndices];

		s.MeshShader = Shader::CreateFromSource("Physics3DMeshShader", k_MeshShaderSource);

		s.MeshLayout = VertexLayout()
			.SetStride(sizeof(MeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT,  offsetof(MeshVertex, Position))
			.AddAttribute("a_Color",    Format::RGBA32_FLOAT, offsetof(MeshVertex, Color));

		s.MeshMaterial = Material::Create(MaterialParams()
			.SetDebugName("Physics3D_Material")
			.SetShader(s.MeshShader)
			.SetCullMode(CullMode::None));

		s.MeshMaterial->SetUniform(s.CameraData);
	}

	void PhysicsLayer::ShutdownScene3D()
	{
		auto& s = m_Scene3D;

		delete[] s.VertexBufferBase;
		delete[] s.IndexBufferBase;
		s.VertexBufferBase = nullptr;
		s.IndexBufferBase  = nullptr;

		if (s.VertexBuffer) { s.VertexBuffer->Destroy(); s.VertexBuffer = nullptr; }
		if (s.IndexBuffer)  { s.IndexBuffer->Destroy();  s.IndexBuffer  = nullptr; }
		if (s.MeshMaterial) { s.MeshMaterial->Destroy(); delete s.MeshMaterial; s.MeshMaterial = nullptr; }
		if (s.MeshShader)   { s.MeshShader->Destroy();   s.MeshShader   = nullptr; }
	}

	void PhysicsLayer::BeginScene3D(const PerspectiveCamera& camera, const glm::vec4& clearColor)
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

	void PhysicsLayer::SubmitMesh(Mesh* mesh, const glm::mat4& transform, const glm::vec4& color)
	{
		auto& s = m_Scene3D;
		for (const auto& v : mesh->GetVertices())
		{
			s.VertexBufferPtr->Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			s.VertexBufferPtr->Color    = color;
			s.VertexBufferPtr++;
		}
		for (uint32_t idx : mesh->GetIndices())
			*s.IndexBufferPtr++ = idx + s.VertexOffset;

		s.VertexOffset += static_cast<uint32_t>(mesh->GetVertices().size());
		s.IndexCount   += static_cast<uint32_t>(mesh->GetIndices().size());
	}

	void PhysicsLayer::FlushScene3D()
	{
		auto& s = m_Scene3D;

		Renderer::Clear(s.ClearColor);

		if (s.IndexCount == 0)
			return;

		const uint32_t vertexDataSize = static_cast<uint32_t>(
			reinterpret_cast<uint8_t*>(s.VertexBufferPtr) - reinterpret_cast<uint8_t*>(s.VertexBufferBase));

		s.VertexBuffer->Upload(s.VertexBufferBase, vertexDataSize);
		s.IndexBuffer->Upload(s.IndexBufferBase, s.IndexCount * sizeof(uint32_t));

		Renderer::DrawIndexed(s.MeshMaterial, s.MeshLayout, s.VertexBuffer, s.IndexBuffer, s.IndexCount);
	}

}
