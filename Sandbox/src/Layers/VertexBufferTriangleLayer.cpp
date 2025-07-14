#include "VertexBufferTriangleLayer.h"

#include <glm/glm.hpp>

void VertexBufferTriangleLayer::OnAttach()
{
	Dingo::CommandListParams commandListParams = Dingo::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = Dingo::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_Shader = Dingo::ShaderBuilder()
		.SetName("Camera Transformation")
		.AddShaderType(Dingo::ShaderType::Vertex, "assets/shaders/spv/graphics_buffer.vert.spv")
		.AddShaderType(Dingo::ShaderType::Fragment, "assets/shaders/spv/graphics_buffer.frag.spv")
		.Create();

	Dingo::VertexLayout vertexLayout = Dingo::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	m_Pipeline = Dingo::PipelineBuilder()
		.SetDebugName("Vertex Buffer Triangle Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(Dingo::Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(Dingo::FillMode::Solid)
		.SetCullMode(Dingo::CullMode::BackAndFront)
		.SetVertexLayout(vertexLayout)
		.Create();

	m_VertexBuffer = Dingo::GraphicsBufferBuilder()
		.SetDebugName("Quad Vertex Buffer")
		.SetByteSize(sizeof(Vertex) * m_Vertices.size())
		.SetType(Dingo::BufferType::VertexBuffer)
		.SetDirectUpload(true)
		.SetInitialData(m_Vertices.data())
		.Create();
}

void VertexBufferTriangleLayer::OnDetach()
{
	m_VertexBuffer->Destroy();
	m_Pipeline->Destroy();
	m_Shader->Destroy();
	m_CommandList->Destroy();
}

void VertexBufferTriangleLayer::OnUpdate()
{
	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->Draw(m_Pipeline, m_VertexBuffer);
	m_CommandList->End();
}
