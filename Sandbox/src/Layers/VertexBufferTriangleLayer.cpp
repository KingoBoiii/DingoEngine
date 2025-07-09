#include "VertexBufferTriangleLayer.h"

#include <glm/glm.hpp>

void VertexBufferTriangleLayer::OnAttach()
{
	DingoEngine::CommandListParams commandListParams = DingoEngine::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = DingoEngine::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_Shader = DingoEngine::ShaderBuilder()
		.SetName("Camera Transformation")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/graphics_buffer.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/graphics_buffer.frag.spv")
		.Create();

	DingoEngine::VertexLayout vertexLayout = DingoEngine::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	m_Pipeline = DingoEngine::PipelineBuilder()
		.SetDebugName("Vertex Buffer Triangle Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(DingoEngine::Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront)
		.SetVertexLayout(vertexLayout)
		.Create();

	m_VertexBuffer = DingoEngine::GraphicsBufferBuilder()
		.SetDebugName("Quad Vertex Buffer")
		.SetByteSize(sizeof(Vertex) * m_Vertices.size())
		.SetType(DingoEngine::BufferType::VertexBuffer)
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
