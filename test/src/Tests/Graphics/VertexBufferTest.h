#pragma once
#include "GraphicsTest.h"

namespace Dingo
{

	class VertexBufferTest : public GraphicsTest
	{
	public:
		virtual ~VertexBufferTest() = default;

	public:
		virtual void InitializeGraphics() override;
		virtual void Update(float deltaTime) override;
		virtual void CleanupGraphics() override;

	private:
		Shader* m_Shader = nullptr;
		Pipeline* m_Pipeline = nullptr;
		GraphicsBuffer* m_VertexBuffer = nullptr;

		struct Vertex
		{
			glm::vec2 position;
			glm::vec3 color;
		};

		const std::vector<Vertex> m_Vertices = {
			{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
			{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
		};
	};

}
