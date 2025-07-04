#pragma once
#include <DingoEngine.h>

class VertexBufferTriangleLayer : public DingoEngine::Layer
{
public:
	VertexBufferTriangleLayer() : Layer("Vertex Buffer Triangle Layer") {}
	virtual ~VertexBufferTriangleLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;

private:
	DingoEngine::CommandList* m_CommandList = nullptr;
	DingoEngine::Shader* m_Shader = nullptr;
	DingoEngine::Pipeline* m_Pipeline = nullptr;

	DingoEngine::GraphicsBuffer* m_VertexBuffer = nullptr;
};
