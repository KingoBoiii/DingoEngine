#include "TexturedQuadLayer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>

#include <imgui.h>

struct CameraTransform
{
	glm::mat4 ProjectionView;
};

void TexturedQuadLayer::OnAttach()
{
	Dingo::CommandListParams commandListParams = Dingo::CommandListParams()
		.SetTargetSwapChain(true);

	m_CommandList = Dingo::CommandList::Create(commandListParams);
	m_CommandList->Initialize();

	m_UniformBuffer = Dingo::GraphicsBufferBuilder()
		.SetDebugName("Textured Quad Uniform Buffer")
		.SetByteSize(sizeof(CameraTransform))
		.SetType(Dingo::BufferType::UniformBuffer)
		.SetIsVolatile(true)
		.SetDirectUpload(false)
		.Create();

	uint32_t width, height, channels;
	const uint8_t* textureData = Dingo::FileSystem::ReadImage("assets/textures/dickbutt_transparent.png", &width, &height, &channels, true, true);

	Dingo::TextureParams textureParams = {
		.DebugName = "Dickbutt Texture",
		.Width = width,
		.Height = height,
		.Format = channels == 4 ? Dingo::TextureFormat::RGBA : Dingo::TextureFormat::RGB,
		.Dimension = Dingo::TextureDimension::Texture2D,
	};

	m_Texture = Dingo::Texture::Create(textureParams);
	m_Texture->Initialize();
	m_Texture->Upload(textureData, width * channels);
	//m_Texture->Upload("assets/textures/dickbutt_transparent.png");

	m_Shader = Dingo::Shader::CreateFromFile("Textured Quad", "assets/shaders/textured_quad.dshader");

	Dingo::VertexLayout vertexLayout = Dingo::VertexLayout()
		.SetStride(sizeof(Vertex))
		.AddAttribute("inPosition", nvrhi::Format::RG32_FLOAT, offsetof(Vertex, position))
		.AddAttribute("inColor", nvrhi::Format::RGB32_FLOAT, offsetof(Vertex, color))
		.AddAttribute("inTexCoord", nvrhi::Format::RG32_FLOAT, offsetof(Vertex, texCoord));

	m_Pipeline = Dingo::PipelineBuilder()
		.SetDebugName("Textured Quad Pipeline")
		.SetShader(m_Shader)
		.SetFramebuffer(Dingo::Application::Get().GetSwapChain()->GetCurrentFramebuffer())
		.SetFillMode(Dingo::FillMode::Solid)
		.SetCullMode(Dingo::CullMode::BackAndFront)
		.SetVertexLayout(vertexLayout)
		.SetUniformBuffer(m_UniformBuffer)
		.SetTexture(m_Texture)
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
	const float aspectRatio = Dingo::Application::Get().GetWindow().GetAspectRatio();
	const glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
	m_UniformBuffer->Upload(&projectionMatrix, sizeof(CameraTransform));

	m_CommandList->Begin();
	m_CommandList->Clear();
	m_CommandList->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, m_UniformBuffer);
	m_CommandList->End();
}

void TexturedQuadLayer::OnImGuiRender()
{
	ImGui::ShowDemoWindow();
}
