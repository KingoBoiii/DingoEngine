#pragma once
#include <DingoEngine.h>

class VertexBufferTriangleLayer : public Dingo::Layer
{
public:
	VertexBufferTriangleLayer() : Layer("Vertex Buffer Triangle Layer") {}
	virtual ~VertexBufferTriangleLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;

private:
	Dingo::CommandList* m_CommandList = nullptr;
	Dingo::Shader* m_Shader = nullptr;
	Dingo::Pipeline* m_Pipeline = nullptr;

	Dingo::GraphicsBuffer* m_VertexBuffer = nullptr;

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
