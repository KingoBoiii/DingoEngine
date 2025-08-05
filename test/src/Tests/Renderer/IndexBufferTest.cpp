#include "IndexBufferTest.h"

namespace Dingo
{

	void IndexBufferTest::Initialize()
	{
		static const char* ShaderSource = R"(
#type vertex
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 1) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
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

		m_Shader = Shader::CreateFromSource("Index Buffer Shader", ShaderSource);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(Vertex))
			.AddAttribute("inPosition", Format::RG32_FLOAT, 0)
			.AddAttribute("inColor", Format::RGB32_FLOAT, sizeof(glm::vec2));

		m_Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Index Buffer Pipeline")
			.SetShader(m_Shader)
			.SetFramebuffer(m_Renderer->GetTargetFramebuffer())
			.SetFillMode(FillMode::Solid)
			.SetCullMode(CullMode::BackAndFront)
			.SetVertexLayout(vertexLayout));

		m_VertexBuffer = GraphicsBuffer::CreateVertexBuffer(sizeof(Vertex) * m_Vertices.size(), m_Vertices.data());

		m_IndexBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Index Buffer")
			.SetByteSize(sizeof(uint16_t) * m_Indices.size())
			.SetType(BufferType::IndexBuffer)
			.SetDirectUpload(true)
			.SetInitialData(m_Indices.data())
			.Create();
	}

	void IndexBufferTest::Update(float deltaTime)
	{
		m_Renderer->Begin();
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer);
		m_Renderer->End();
	}

	void IndexBufferTest::Cleanup()
	{
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
