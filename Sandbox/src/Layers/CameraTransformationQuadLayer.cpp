#include "CameraTransformationQuadLayer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

	m_UniformBuffer = DingoEngine::GraphicsBufferBuilder()
		.SetDebugName("Camera Transform Buffer")
		.SetByteSize(sizeof(CameraTransform))
		.SetType(DingoEngine::BufferType::UniformBuffer)
		.SetIsVolatile(true)
		.SetDirectUpload(false)
		.Create();

	m_Shader = DingoEngine::ShaderBuilder()
		.SetName("Camera Transformation")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/camera_transformation.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/camera_transformation.frag.spv")
		.Create();

	DingoEngine::VertexLayout vertexLayout = DingoEngine::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2));

	m_Pipeline = DingoEngine::PipelineBuilder()
		.SetDebugName("Camera Transformation Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(DingoEngine::Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront)
		.SetVertexLayout(vertexLayout)
		.SetUniformBuffer(m_UniformBuffer)
		.Create();

	m_VertexBuffer = DingoEngine::GraphicsBufferBuilder()
		.SetDebugName("Quad Vertex Buffer")
		.SetByteSize(sizeof(Vertex) * m_Vertices.size())
		.SetType(DingoEngine::BufferType::VertexBuffer)
		.SetDirectUpload(true)
		.SetInitialData(m_Vertices.data())
		.Create();

	m_IndexBuffer = DingoEngine::GraphicsBufferBuilder()
		.SetDebugName("Quad Index Buffer")
		.SetByteSize(sizeof(uint16_t) * m_Indices.size())
		.SetType(DingoEngine::BufferType::IndexBuffer)
		.SetDirectUpload(true)
		.SetInitialData(m_Indices.data())
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
	m_UniformBuffer->Upload(&projectionMatrix, sizeof(CameraTransform));

	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, m_UniformBuffer);
	m_CommandList->End();
}
