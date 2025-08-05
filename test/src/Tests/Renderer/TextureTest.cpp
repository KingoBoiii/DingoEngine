#include "TextureTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

	void TextureTest::Initialize()
	{
		static const char* ShaderSource = R"(
#type vertex
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout (std140, set = 0, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vTexCoord;

void main() {
    gl_Position = ProjectionView * vec4(inPosition, 0.0, 1.0);
    vColor = inColor;
    vTexCoord = inTexCoord;
}

#type fragment
#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(set = 0, binding = 1) uniform texture2D uTexture;
layout(set = 0, binding = 2) uniform sampler uTextureSampler;

void main() {
    outColor = texture(sampler2D(uTexture, uTextureSampler), vTexCoord);
    //outColor = texture(uTextureSampler, vTexCoord);
}
)";

		m_UniformBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Uniform Buffer")
			.SetByteSize(sizeof(CameraTransform))
			.SetType(Dingo::BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false)
			.Create();

		uint32_t width, height, channels;
		const uint8_t* textureData = Dingo::FileSystem::ReadImage("assets/textures/container.jpg", &width, &height, &channels, true, true);

		Dingo::TextureParams textureParams = {
			.DebugName = "Wooden Container",
			.Width = width,
			.Height = height,
			.Format = channels == 4 ? Dingo::TextureFormat::RGBA : Dingo::TextureFormat::RGB,
			.Dimension = Dingo::TextureDimension::Texture2D,
		};

		m_Texture = Dingo::Texture::Create(textureParams);
		m_Texture->Initialize();
		m_Texture->Upload(textureData, width * channels);

		m_Shader = Dingo::Shader::CreateFromSource("Texture Shader", ShaderSource);

		Dingo::VertexLayout vertexLayout = Dingo::VertexLayout()
			.SetStride(sizeof(Vertex))
			.AddAttribute("inPosition", Format::RG32_FLOAT, offsetof(Vertex, position))
			.AddAttribute("inColor", Format::RGB32_FLOAT, offsetof(Vertex, color))
			.AddAttribute("inTexCoord", Format::RG32_FLOAT, offsetof(Vertex, texCoord));

		m_Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Texture Quad Pipeline")
			.SetShader(m_Shader)
			.SetFramebuffer(m_Renderer->GetTargetFramebuffer())
			.SetFillMode(Dingo::FillMode::Solid)
			.SetCullMode(Dingo::CullMode::BackAndFront)
			.SetVertexLayout(vertexLayout)
			.SetUniformBuffer(m_UniformBuffer)
			.SetTexture(m_Texture));

		m_VertexBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Vertex Buffer")
			.SetByteSize(sizeof(Vertex) * m_Vertices.size())
			.SetType(Dingo::BufferType::VertexBuffer)
			.SetDirectUpload(true)
			.SetInitialData(m_Vertices.data())
			.Create();

		m_IndexBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Index Buffer")
			.SetByteSize(sizeof(uint16_t) * m_Indices.size())
			.SetType(Dingo::BufferType::IndexBuffer)
			.SetDirectUpload(true)
			.SetInitialData(m_Indices.data())
			.Create();
	}

	void TextureTest::Update(float deltaTime)
	{
		const glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), m_AspectRatio, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
		m_UniformBuffer->Upload(&projectionMatrix, sizeof(CameraTransform));

		m_Renderer->Begin();
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, m_UniformBuffer);
		m_Renderer->End();
	}

	void TextureTest::Cleanup()
	{
		if (m_Texture)
		{
			m_Texture->Destroy();
			m_Texture = nullptr;
		}

		if (m_UniformBuffer)
		{
			m_UniformBuffer->Destroy();
			m_UniformBuffer = nullptr;
		}

		if (m_IndexBuffer)
		{
			m_IndexBuffer->Destroy();
			m_IndexBuffer = nullptr;
		}

		if (m_VertexBuffer)
		{
			m_VertexBuffer->Destroy();
			m_VertexBuffer = nullptr;
		}

		if (m_Pipeline)
		{
			m_Pipeline->Destroy();
			m_Pipeline = nullptr;
		}

		if (m_Shader)
		{
			m_Shader->Destroy();
			m_Shader = nullptr;
		}
	}

}
