#pragma once
#include "Tests/GraphicsTest.h"
#include <glm/glm.hpp>

namespace Dingo
{

	class Mesh3DTest : public GraphicsTest
	{
	public:
		Mesh3DTest() = default;
		virtual ~Mesh3DTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void UploadMesh(Mesh* mesh);

	private:
		struct TransformUBO { glm::mat4 ViewProjection; glm::mat4 Model; };

		Shader*         m_Shader   = nullptr;
		Material*       m_Material = nullptr;
		GraphicsBuffer* m_VB       = nullptr;
		GraphicsBuffer* m_IB       = nullptr;
		VertexLayout    m_Layout;
		uint32_t        m_IndexCount = 0;

		Mesh* m_BoxMesh    = nullptr;
		Mesh* m_SphereMesh = nullptr;
		bool  m_ShowSphere = false;

		PerspectiveCamera m_Camera;
		float m_Rotation      = 0.0f;
		float m_RotationSpeed = 45.0f;
		bool  m_AutoRotate    = true;
	};

}
