#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace Dingo
{

	// A 3D physics sandbox showcasing the engine's Jolt-backed PhysicsWorld3D.
	//
	// The engine has no 3D scene/ECS yet, so (like Breakout3D) this layer owns a
	// small mesh batcher and drives rendering itself: every frame it steps the
	// physics world, then draws each body at world->GetTransform(id). Gameplay:
	// a tower of dynamic boxes on a static floor; fire spheres to knock it down.
	class PhysicsLayer : public Layer
	{
	public:
		PhysicsLayer() : Layer("Physics3D") {}
		virtual ~PhysicsLayer() = default;

	public:
		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;
		void OnEvent(Event& event) override;

	private:
		// Scene / gameplay
		void BuildScene();
		void ResetScene();
		void FireSphere();

		// A simulated body plus the data needed to render it.
		struct Body
		{
			PhysicsBodyId3D Id = k_InvalidBody3D;
			bool IsSphere = false;
			glm::vec3 HalfExtents{ 0.5f };
			float Radius = 0.5f;
			glm::vec4 Color{ 1.0f };
		};
		Body& AddBox(const glm::vec3& position, const glm::vec3& halfExtents, const glm::vec4& color, BodyType3D type);
		Body& AddSphere(const glm::vec3& position, float radius, const glm::vec4& color, BodyType3D type);

		// Rendering
		void RenderScene();
		void RenderUI();
		void UpdateOrthoProjection();
		void DrawCenteredText(Renderer2D& r, const std::string& text, float size, float y, const glm::vec4& color);

		// Minimal 3D mesh batcher (same approach as the Breakout3D example).
		void InitScene3D();
		void ShutdownScene3D();
		void BeginScene3D(const PerspectiveCamera& camera, const glm::vec4& clearColor);
		void SubmitMesh(Mesh* mesh, const glm::mat4& transform, const glm::vec4& color);
		void FlushScene3D();

	private:
		PerspectiveCamera m_Camera;
		float m_AspectRatio = 16.0f / 9.0f;

		Font* m_Font = nullptr;
		glm::mat4 m_OrthoProjection = glm::mat4(1.0f);
		float m_OrthoSize = 5.0f;

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_SphereMesh = nullptr;

		PhysicsWorld3D* m_World = nullptr;
		std::vector<Body> m_Bodies;
		int m_Shots = 0;
		float m_LaunchX = 0.0f; // A/D shift the launch position

		// 3D batcher state (owned by this layer).
		struct MeshVertex { glm::vec3 Position; glm::vec4 Color; };
		struct Scene3D
		{
			struct CameraUBOData { glm::mat4 ViewProjection; };
			CameraUBOData CameraData = {};

			Shader*       MeshShader   = nullptr;
			Material*     MeshMaterial = nullptr;
			VertexLayout  MeshLayout;

			GraphicsBuffer* VertexBuffer     = nullptr;
			GraphicsBuffer* IndexBuffer      = nullptr;
			MeshVertex*     VertexBufferBase = nullptr;
			MeshVertex*     VertexBufferPtr  = nullptr;
			uint32_t*       IndexBufferBase  = nullptr;
			uint32_t*       IndexBufferPtr   = nullptr;
			uint32_t        IndexCount       = 0;
			uint32_t        VertexOffset     = 0;

			glm::vec4 ClearColor = { 0.10f, 0.12f, 0.16f, 1.0f };
		} m_Scene3D;
	};

}
