#include "StaticTriangleTest.h"

namespace Dingo
{

	void StaticTriangleTest::InitializeGraphics()
	{
		static const char* ShaderSource = R"(
#type vertex
#version 450

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 1) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}

#type fragment
#version 450

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec3 fragColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

		m_Shader = Shader::CreateFromSource("Static Triangle Shader", ShaderSource);

		m_Pipeline = Dingo::PipelineBuilder()
			.SetDebugName("Static Triangle Pipeline")
			.SetShader(m_Shader)
			.SetFramebuffer(m_Renderer->GetFramebuffer())
			.SetFillMode(Dingo::FillMode::Solid)
			.SetCullMode(Dingo::CullMode::BackAndFront)
			.Create();
	}

	void StaticTriangleTest::Update(float deltaTime)
	{
		m_Renderer->Begin();
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->Draw(m_Pipeline);
		m_Renderer->End();
	}

	void StaticTriangleTest::CleanupGraphics()
	{
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
