#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

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
		void RunWindingChecks();
		void RunDrawChecks();
		Framebuffer* CreateCheckTarget();
		void ReadCheckTarget(Framebuffer* target, const std::string& name, float left, float right);
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

	private:
		struct TransformUBO { glm::mat4 ViewProjection; glm::mat4 Model; };

		TestChecks m_Checks;

		Shader*         m_Shader   = nullptr;
		Material*       m_Material = nullptr;
		GraphicsBuffer* m_VB       = nullptr;
		GraphicsBuffer* m_IB       = nullptr;
		VertexLayout    m_Layout;
		uint32_t        m_IndexCount = 0;

		Material*       m_CheckOpaque      = nullptr;
		Material*       m_CheckTranslucent = nullptr;
		GraphicsBuffer* m_CheckVB          = nullptr;
		GraphicsBuffer* m_CheckIB          = nullptr;
		std::vector<Framebuffer*> m_CheckTargets;
		std::shared_ptr<int> m_Alive;
		bool m_DrawChecksRun = false;

		Mesh* m_BoxMesh    = nullptr;
		Mesh* m_SphereMesh = nullptr;
		bool  m_ShowSphere = false;

		PerspectiveCamera m_Camera;
		float m_Rotation      = 0.0f;
		float m_RotationSpeed = 45.0f;
		bool  m_AutoRotate    = true;
	};

}
