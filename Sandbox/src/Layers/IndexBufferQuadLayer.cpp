#include "IndexBufferQuadLayer.h"

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;
};

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

void IndexBufferQuadLayer::OnAttach()
{
	DingoEngine::CommandListParams commandListParams = DingoEngine::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = DingoEngine::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	DingoEngine::ShaderParams shaderParams = DingoEngine::ShaderParams()
		.SetName("GraphicsBufferTriangle_IBO")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/graphics_buffer.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/graphics_buffer.frag.spv");

	m_Shader = DingoEngine::Shader::Create(shaderParams);
	m_Shader->Initialize();

	DingoEngine::VertexLayout vertexLayout = DingoEngine::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	DingoEngine::PipelineParams pipelineParams = DingoEngine::PipelineParams()
		.SetShader(m_Shader)
		.SetVertexLayout(vertexLayout)
		.SetFramebuffer(DingoEngine::Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront);

	m_Pipeline = DingoEngine::Pipeline::Create(pipelineParams);
	m_Pipeline->Initialize();

	m_VertexBuffer = DingoEngine::GraphicsBufferBuilder()
		.SetDebugName("Quad Vertex Buffer")
		.SetByteSize(sizeof(Vertex) * vertices.size())
		.SetType(DingoEngine::BufferType::VertexBuffer)
		.SetDirectUpload(true)
		.SetInitialData(vertices.data())
		.Create();

	m_IndexBuffer = DingoEngine::GraphicsBufferBuilder()
		.SetDebugName("Quad Index Buffer")
		.SetByteSize(sizeof(uint16_t) * indices.size())
		.SetType(DingoEngine::BufferType::IndexBuffer)
		.SetDirectUpload(true)
		.SetInitialData(indices.data())
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
