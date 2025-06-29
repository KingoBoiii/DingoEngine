#pragma once
#include <DingoEngine.h>

class IndexBufferQuadLayer : public DingoEngine::Layer
{
public:
	IndexBufferQuadLayer() : Layer("Index Buffer Quad Layer") {}
	virtual ~IndexBufferQuadLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;

private:
	DingoEngine::CommandList* m_CommandList = nullptr;
	DingoEngine::Shader* m_Shader = nullptr;
	DingoEngine::Pipeline* m_Pipeline = nullptr;
	DingoEngine::VertexBuffer* m_VertexBuffer = nullptr;
	DingoEngine::IndexBuffer* m_IndexBuffer = nullptr;
};

