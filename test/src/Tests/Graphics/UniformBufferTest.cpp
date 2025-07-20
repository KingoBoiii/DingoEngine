#include "UniformBufferTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

	void UniformBufferTest::InitializeGraphics()
	{
		static const char* ShaderSource = R"(
#type vertex
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout (std140, set = 0, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 1) out vec3 fragColor;

void main() {
    gl_Position = ProjectionView * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}

#type fragment
#version 450

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec3 fragColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

		m_Shader = Shader::CreateFromSource("Uniform Buffer Shader", ShaderSource);

		m_UniformBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Uniform Buffer")
			.SetByteSize(sizeof(CameraTransform))
			.SetType(Dingo::BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false)
			.Create();

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(Vertex))
			.AddAttribute("inPosition", Format::RG32_FLOAT, 0)
			.AddAttribute("inColor", Format::RGB32_FLOAT, sizeof(glm::vec2));

		m_Pipeline = PipelineBuilder()
			.SetDebugName("Uniform Buffer Pipeline")
			.SetShader(m_Shader)
			.SetFramebuffer(m_Renderer->GetFramebuffer())
			.SetFillMode(FillMode::Solid)
			.SetCullMode(CullMode::BackAndFront)
			.SetVertexLayout(vertexLayout)
			.SetUniformBuffer(m_UniformBuffer)
			.Create();

		m_VertexBuffer = GraphicsBufferBuilder()
			.SetDebugName("Vertex Buffer")
			.SetByteSize(sizeof(Vertex) * m_Vertices.size())
			.SetType(BufferType::VertexBuffer)
			.SetDirectUpload(true)
			.SetInitialData(m_Vertices.data())
			.Create();

		m_IndexBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Index Buffer")
			.SetByteSize(sizeof(uint16_t) * m_Indices.size())
			.SetType(BufferType::IndexBuffer)
			.SetDirectUpload(true)
			.SetInitialData(m_Indices.data())
			.Create();
	}

	void UniformBufferTest::Update(float deltaTime)
	{
		const glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), m_AspectRatio, 0.1f, 100.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
		m_UniformBuffer->Upload(&projectionMatrix, sizeof(CameraTransform));

		m_Renderer->Begin();
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, m_UniformBuffer);
		m_Renderer->End();
	}

	void UniformBufferTest::CleanupGraphics()
	{
		if (m_UniformBuffer)
		{
			m_UniformBuffer->Destroy();
			m_UniformBuffer= nullptr;
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
