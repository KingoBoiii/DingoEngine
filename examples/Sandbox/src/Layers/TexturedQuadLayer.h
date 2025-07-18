#pragma once
#include <DingoEngine.h>

class TexturedQuadLayer : public Dingo::Layer
{
public:
	TexturedQuadLayer() : Layer("Textured Quad Layer") {}
	virtual ~TexturedQuadLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;
	virtual void OnImGuiRender() override;

private:
	Dingo::CommandList* m_CommandList = nullptr;
	Dingo::Shader* m_Shader = nullptr;
	Dingo::Pipeline* m_Pipeline = nullptr;

	Dingo::GraphicsBuffer* m_VertexBuffer = nullptr;
	Dingo::GraphicsBuffer* m_IndexBuffer = nullptr;
	Dingo::GraphicsBuffer* m_UniformBuffer = nullptr;

	Dingo::Texture* m_Texture = nullptr;

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
};
