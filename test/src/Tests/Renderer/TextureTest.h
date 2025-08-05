#pragma once
#include "RendererTest.h"

namespace Dingo
{

	class TextureTest : public RendererTest
	{
	public:
		TextureTest(Renderer* renderer)
			: RendererTest(renderer)
		{}
		virtual ~TextureTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;

	private:
		Shader* m_Shader = nullptr;
		Pipeline* m_Pipeline = nullptr;
		GraphicsBuffer* m_VertexBuffer = nullptr;
		GraphicsBuffer* m_IndexBuffer = nullptr;
		GraphicsBuffer* m_UniformBuffer = nullptr;
		Texture* m_Texture = nullptr;

		struct Vertex
		{
			glm::vec2 position;
			glm::vec3 color;
			glm::vec2 texCoord;
		};

		const std::vector<Vertex> m_Vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Bottom-left
		{{0.5f, -0.5f},  {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // Bottom-right
		{{0.5f, 0.5f},   {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Top-right
		{{-0.5f, 0.5f},  {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}  // Top-left
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




