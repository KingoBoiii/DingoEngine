#include "StaticTriangleLayer.h"

void StaticTriangleLayer::OnAttach()
{
	DingoEngine::CommandListParams commandListParams = DingoEngine::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = DingoEngine::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_Shader = DingoEngine::ShaderBuilder()
		.SetName("Camera Transformation")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/static_triangle.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/static_triangle.frag.spv")
		.Create();

	m_Pipeline = DingoEngine::PipelineBuilder()
		.SetDebugName("Static Triangle Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(DingoEngine::Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront)
		.Create();
}

void StaticTriangleLayer::OnDetach()
{
	m_Pipeline->Destroy();
	m_Shader->Destroy();
	m_CommandList->Destroy();
}

void StaticTriangleLayer::OnUpdate()
{
	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->Draw(m_Pipeline);
	m_CommandList->End();
}
