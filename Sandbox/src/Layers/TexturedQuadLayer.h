#pragma once
#include <DingoEngine.h>

class TexturedQuadLayer : public DingoEngine::Layer
{
public:
	TexturedQuadLayer() : Layer("Textured Quad Layer") {}
	virtual ~TexturedQuadLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;

private:
	DingoEngine::CommandList* m_CommandList = nullptr;
	DingoEngine::Shader* m_Shader = nullptr;
	DingoEngine::Pipeline* m_Pipeline = nullptr;

	DingoEngine::GraphicsBuffer* m_VertexBuffer = nullptr;
	DingoEngine::GraphicsBuffer* m_IndexBuffer = nullptr;
	DingoEngine::GraphicsBuffer* m_UniformBuffer = nullptr;

	DingoEngine::Texture* m_Texture = nullptr;
};
