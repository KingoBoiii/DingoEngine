#include "IndexBufferQuadLayer.h"

#include <glm/glm.hpp>

void IndexBufferQuadLayer::OnAttach()
{
	Dingo::CommandListParams commandListParams = Dingo::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = Dingo::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_Shader = Dingo::ShaderBuilder()
		.SetName("Graphics Buffer")
		.AddShaderType(Dingo::ShaderType::Vertex, "assets/shaders/graphics_buffer.vert")
		.AddShaderType(Dingo::ShaderType::Fragment, "assets/shaders/graphics_buffer.frag")
		.Create();

	Dingo::VertexLayout vertexLayout = Dingo::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	m_Pipeline = Dingo::PipelineBuilder()
		.SetDebugName("Index Buffer Quad Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(Dingo::Application::Get().GetSwapChain()->GetCurrentFramebuffer())
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

	m_IndexBuffer = Dingo::GraphicsBufferBuilder()
		.SetDebugName("Quad Index Buffer")
		.SetByteSize(sizeof(uint16_t) * m_Indices.size())
		.SetType(Dingo::BufferType::IndexBuffer)
		.SetDirectUpload(true)
		.SetInitialData(m_Indices.data())
		.Create();
}

void IndexBufferQuadLayer::OnDetach()
{
	m_IndexBuffer->Destroy();
	m_VertexBuffer->Destroy();
	m_Pipeline->Destroy();
	m_Shader->Destroy();
	m_CommandList->Destroy();
}

void IndexBufferQuadLayer::OnUpdate()
{
	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer);
	m_CommandList->End();
}
