#pragma once
#include <DingoEngine.h>

#define USE_GRAPHICS_BUFFER 1

class CameraTransformationQuadLayer : public DingoEngine::Layer
{
public:
	CameraTransformationQuadLayer() : Layer("Camera Transformation Layer") {}
	virtual ~CameraTransformationQuadLayer() = default;

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
};
