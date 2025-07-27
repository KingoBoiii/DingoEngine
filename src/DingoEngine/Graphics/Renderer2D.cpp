#include "depch.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include "DingoEngine/Graphics/Builders/PipelineBuilder.h"
#include "DingoEngine/Graphics/Builders/GraphicsBufferBuilder.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

	namespace Shaders
	{

		constexpr const char* Renderer2DQuadShader = R"(
#type vertex
#version 450
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout (std140, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 0) out vec4 v_Color;

void main()
{
	gl_Position = ProjectionView * vec4(a_Position, 1.0);
	v_Color = a_Color;
}

#type fragment
#version 450
layout(location = 0) in vec4 v_Color;

layout(location = 0) out vec4 o_Color;

void main()
{
	o_Color = v_Color;
}
		)";

	}

	void Renderer2D::Initialize()
	{
		m_TargetFramebuffer = m_Params.TargetFramebuffer;
		if (m_TargetFramebuffer == nullptr)
		{
			m_TargetFramebuffer = Framebuffer::Create(FramebufferParams()
				.SetDebugName("Renderer2DFramebuffer")
				.SetWidth(800)
				.SetHeight(600)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
			m_TargetFramebuffer->Initialize();
		}

		CommandListParams commandListParams = CommandListParams()
			.SetTargetFramebuffer(m_TargetFramebuffer);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();

		CreateQuadIndexBuffer();

		m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		m_CameraUniformBuffer = GraphicsBufferBuilder()
			.SetType(BufferType::UniformBuffer)
			.SetDebugName("Renderer2DCameraUniformBuffer")
			.SetByteSize(sizeof(CameraData))
			.SetDirectUpload(false)
			.SetIsVolatile(true)
			.Create();

		CreateQuadPipeline();
	}

	void Renderer2D::Shutdown()
	{
		DestroyQuadPipeline();

		if (m_CameraUniformBuffer)
		{
			m_CameraUniformBuffer->Destroy();
			m_CameraUniformBuffer = nullptr;
		}

		if (m_CommandList)
		{
			m_CommandList->Destroy();
			m_CommandList = nullptr;
		}

		if (m_TargetFramebuffer != m_Params.TargetFramebuffer)
		{
			m_TargetFramebuffer->Destroy();
			m_TargetFramebuffer = nullptr;
		}
	}

	void Renderer2D::Resize(uint32_t width, uint32_t height)
	{
		if (width != m_TargetFramebuffer->GetParams().Width || height != m_TargetFramebuffer->GetParams().Height)
		{
			m_TargetFramebuffer->Resize(width, height);
		}
	}

	void Renderer2D::BeginScene(const glm::mat4& projectionViewMatrix)
	{
		m_CameraData.ProjectionViewMatrix = projectionViewMatrix;

		m_QuadPipeline.IndexCount = 0;
		m_QuadPipeline.VertexBufferPtr = m_QuadPipeline.VertexBufferBase;
	}

	void Renderer2D::EndScene()
	{
		m_CommandList->Begin();
		m_CommandList->Clear(m_TargetFramebuffer, 0, m_Params.ClearColor);

		if(m_QuadPipeline.IndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)m_QuadPipeline.VertexBufferPtr - (uint8_t*)m_QuadPipeline.VertexBufferBase);
			m_CommandList->UploadBuffer(m_QuadPipeline.VertexBuffer, m_QuadPipeline.VertexBufferBase, dataSize);

			m_CommandList->SetPipeline(m_QuadPipeline.Pipeline);
			m_CommandList->SetIndexBuffer(m_QuadIndexBuffer);
			m_CommandList->AddVertexBuffer(m_QuadPipeline.VertexBuffer, 0, 0);
			m_CommandList->UploadBuffer(m_CameraUniformBuffer, &m_CameraData, sizeof(CameraData));

			m_CommandList->DrawIndexed(m_QuadPipeline.IndexCount);
		}

		m_CommandList->End();
	}

	void Renderer2D::Clear(const glm::vec4& clearColor)
	{
		m_Params.ClearColor = clearColor;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		if(m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
		{
			DE_CORE_ERROR("Renderer2D: Quad index count exceeded the maximum limit.");
			return;
		}

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), { position.x, position.y, 0.0f }) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
	}

	void Renderer2D::CreateQuadIndexBuffer()
	{
		uint16_t* quadIndices = new uint16_t[m_Params.Capabilities.GetQuadIndexCount()];

		uint16_t offset = 0;
		for (uint32_t i = 0; i < m_Params.Capabilities.GetQuadIndexCount(); i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		m_QuadIndexBuffer = GraphicsBufferBuilder()
			.SetType(BufferType::IndexBuffer)
			.SetDebugName("Renderer2DQuadIndexBuffer")
			.SetDirectUpload(true)
			.SetByteSize(sizeof(uint16_t) * m_Params.Capabilities.GetQuadIndexCount())
			.SetInitialData(quadIndices)
			.Create();

		delete[] quadIndices;
	}

	void Renderer2D::CreateQuadPipeline()
	{
		m_QuadPipeline = QuadPipeline();

		m_QuadPipeline.Shader = Shader::CreateFromSource("Renderer2DQuadShader", Shaders::Renderer2DQuadShader);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(QuadVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(QuadVertex, Position))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(QuadVertex, Color));

		m_QuadPipeline.Pipeline = PipelineBuilder()
			.SetDebugName("Renderer2DQuadPipeline")
			.SetFramebuffer(m_TargetFramebuffer)
			.SetShader(m_QuadPipeline.Shader)
			.SetVertexLayout(vertexLayout)
			.SetCullMode(CullMode::BackAndFront)
			.SetUniformBuffer(m_CameraUniformBuffer)
			.Create();

		m_QuadPipeline.VertexBuffer = GraphicsBufferBuilder()
			.SetType(BufferType::VertexBuffer)
			.SetDebugName("Renderer2DQuadVertexBuffer")
			.SetByteSize(sizeof(QuadVertex) * m_Params.Capabilities.GetQuadVertexCount())
			.Create();

		m_QuadPipeline.VertexBufferBase = new QuadVertex[m_Params.Capabilities.GetQuadVertexCount()];
	}

	void Renderer2D::DestroyQuadPipeline()
	{
		if (m_QuadPipeline.VertexBuffer)
		{
			m_QuadPipeline.VertexBuffer->Destroy();
			delete[] m_QuadPipeline.VertexBufferBase;
			m_QuadPipeline.VertexBuffer = nullptr;
			m_QuadPipeline.VertexBufferBase = nullptr;
			m_QuadPipeline.VertexBufferPtr = nullptr;
		}
		m_QuadPipeline.IndexCount = 0;

		if (m_QuadIndexBuffer)
		{
			m_QuadIndexBuffer->Destroy();
			m_QuadIndexBuffer = nullptr;
		}

		if (m_QuadPipeline.Pipeline)
		{
			m_QuadPipeline.Pipeline->Destroy();
			m_QuadPipeline.Pipeline = nullptr;
		}

		if (m_QuadPipeline.Shader)
		{
			m_QuadPipeline.Shader->Destroy();
			m_QuadPipeline.Shader = nullptr;
		}
	}


} // namespace Dingo
