#include "StaticTriangleLayer.h"

void StaticTriangleLayer::OnAttach()
{
	Dingo::CommandListParams commandListParams = Dingo::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = Dingo::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_Shader = Dingo::ShaderBuilder()
		.SetName("Camera Transformation")
		.AddShaderType(Dingo::ShaderType::Vertex, "assets/shaders/spv/static_triangle.vert.spv")
		.AddShaderType(Dingo::ShaderType::Fragment, "assets/shaders/spv/static_triangle.frag.spv")
		.Create();

	m_Pipeline = Dingo::PipelineBuilder()
		.SetDebugName("Static Triangle Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(Dingo::Application::Get().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(Dingo::FillMode::Solid)
		.SetCullMode(Dingo::CullMode::BackAndFront)
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
