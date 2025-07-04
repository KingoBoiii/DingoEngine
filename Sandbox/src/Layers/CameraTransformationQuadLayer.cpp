#include "CameraTransformationQuadLayer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

struct CameraTransform
{
	glm::mat4 ProjectionView;
};

void CameraTransformationQuadLayer::OnAttach()
{
	DingoEngine::CommandListParams commandListParams = DingoEngine::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = DingoEngine::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_UniformBuffer = DingoEngine::UniformBuffer::Create(sizeof(CameraTransform));
	m_UniformBuffer->Initialize();

	DingoEngine::ShaderParams shaderParams = DingoEngine::ShaderParams()
		.SetName("Camera Transformation")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/camera_transformation.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/camera_transformation.frag.spv");

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
		.SetCullMode(DingoEngine::CullMode::BackAndFront)
		.SetUniformBuffer(m_UniformBuffer);

	m_Pipeline = DingoEngine::Pipeline::Create(pipelineParams);
	m_Pipeline->Initialize();

	m_VertexBuffer = DingoEngine::VertexBufferBuilder()
		.SetSize(sizeof(Vertex) * vertices.size())
		.SetData(vertices.data())
		.Create();

	m_IndexBuffer = DingoEngine::IndexBufferBuilder()
		.SetCount(indices.size())
		.SetIndices(indices.data())
		.Create();
}

void CameraTransformationQuadLayer::OnDetach()
{
	m_UniformBuffer->Destroy();
	m_IndexBuffer->Destroy();
	m_VertexBuffer->Destroy();
	m_Pipeline->Destroy();
	m_Shader->Destroy();
	m_CommandList->Destroy();
}

void CameraTransformationQuadLayer::OnUpdate()
{
	const float aspectRatio = DingoEngine::Application::Get().GetWindow().GetAspectRatio();
	const glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
	m_UniformBuffer->Upload(&projectionMatrix);

	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, m_UniformBuffer);
	m_CommandList->End();
}
