#pragma once
#include <DingoEngine.h>

class StaticTriangleLayer : public DingoEngine::Layer
{
public:
	StaticTriangleLayer() : Layer("Static Triangle Layer") {}
	virtual ~StaticTriangleLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;
	
private:
	DingoEngine::CommandList* m_CommandList = nullptr;
	DingoEngine::Shader* m_Shader = nullptr;
	DingoEngine::Pipeline* m_Pipeline = nullptr;
};
