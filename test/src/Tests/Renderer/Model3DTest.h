#pragma once
#include "Tests/GraphicsTest.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Dingo
{

	class Model3DTest : public GraphicsTest
	{
	public:
		Model3DTest() = default;
		virtual ~Model3DTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void LoadModel(const std::string& path);
		void UnloadModel();

	private:
		struct TransformUBO { glm::mat4 ViewProjection; glm::mat4 Model; };

		struct GpuSubMesh
		{
			GraphicsBuffer* VB         = nullptr;
			GraphicsBuffer* IB         = nullptr;
			Material*       Mat        = nullptr;
			uint32_t        IndexCount = 0;
		};

		Shader*               m_Shader = nullptr;
		VertexLayout          m_Layout;
		std::vector<GpuSubMesh> m_GpuSubMeshes;

		Model* m_Model = nullptr;
		bool   m_LoadFailed = false;

		PerspectiveCamera m_Camera;
		float m_Rotation      = 0.0f;
		float m_RotationSpeed = 30.0f;
		bool  m_AutoRotate    = true;

		char m_PathBuf[512] = "assets/models/Duck/Duck.gltf";
	};

}
