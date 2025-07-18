#pragma once
#include <DingoEngine.h>

class StaticTriangleLayer : public Dingo::Layer
{
public:
	StaticTriangleLayer() : Layer("Static Triangle Layer") {}
	virtual ~StaticTriangleLayer() = default;

public:
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate() override;
	
private:
	Dingo::CommandList* m_CommandList = nullptr;
	Dingo::Shader* m_Shader = nullptr;
	Dingo::Pipeline* m_Pipeline = nullptr;
};
