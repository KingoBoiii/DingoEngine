#pragma once
#include "RendererTest.h"

namespace Dingo
{

	class UniformBufferTest : public RendererTest
	{
	public:
		virtual ~UniformBufferTest() = default;

	public:
		virtual void InitializeGraphics() override;
		virtual void Update(float deltaTime) override;
		virtual void CleanupGraphics() override;

	private:
		Shader* m_Shader = nullptr;
		Pipeline* m_Pipeline = nullptr;
		GraphicsBuffer* m_VertexBuffer = nullptr;
		GraphicsBuffer* m_IndexBuffer = nullptr;
		GraphicsBuffer* m_UniformBuffer = nullptr;

		struct Vertex
		{
			glm::vec2 position;
			glm::vec3 color;
		};

		const std::vector<Vertex> m_Vertices = {
			{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
		};

		const std::vector<uint16_t> m_Indices = {
			0, 1, 2, 2, 3, 0
		};

		struct CameraTransform
		{
			glm::mat4 ProjectionView;
		};
	};

}


