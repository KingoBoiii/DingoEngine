#include "TexturedQuadLayer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>

struct CameraTransform
{
	glm::mat4 ProjectionView;
};

void TexturedQuadLayer::OnAttach()
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

	DingoEngine::TextureParams textureParams = {
		.Format = DingoEngine::TextureFormat::RGB,
		.Dimension = DingoEngine::TextureDimension::Texture2D,
		.Width = 920,
		.Height = 643
	};

	m_Texture = DingoEngine::Texture::Create(textureParams);
	m_Texture->Initialize();

	{
		std::ifstream file("assets/textures/dickbutt.png", std::ios::ate | std::ios::binary);

		size_t fileSize = file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		m_Texture->Upload(buffer.data(), fileSize);
	}

	m_Shader = DingoEngine::ShaderBuilder()
		.SetName("Textured Quad")
		.AddShaderType(DingoEngine::ShaderType::Vertex, "assets/shaders/spv/textured_quad.vert.spv")
		.AddShaderType(DingoEngine::ShaderType::Fragment, "assets/shaders/spv/textured_quad.frag.spv")
		.Create();

	DingoEngine::VertexLayout vertexLayout = DingoEngine::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, 0)
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, sizeof(glm::vec2))
		.AddAttribute("inTexCoord", nvrhi::Format::RG32_FLOAT, sizeof(glm::vec2) + sizeof(glm::vec3));

	m_Pipeline = DingoEngine::PipelineBuilder()
		.SetDebugName("Textured Quad Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(DingoEngine::Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(DingoEngine::FillMode::Solid)
		.SetCullMode(DingoEngine::CullMode::BackAndFront)
		.SetVertexLayout(vertexLayout)
		.SetUniformBuffer(m_UniformBuffer)
		.SetTexture(m_Texture)
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

void TexturedQuadLayer::OnDetach()
{
	m_Texture->Destroy();
	m_UniformBuffer->Destroy();
	m_IndexBuffer->Destroy();
	m_VertexBuffer->Destroy();
	m_Pipeline->Destroy();
	m_Shader->Destroy();
	m_CommandList->Destroy();
}

void TexturedQuadLayer::OnUpdate()
{
	const float aspectRatio = DingoEngine::Application::Get().GetWindow().GetAspectRatio();
	const glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
	m_UniformBuffer->Upload(&projectionMatrix, sizeof(CameraTransform));

	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, m_UniformBuffer);
	m_CommandList->End();
}
