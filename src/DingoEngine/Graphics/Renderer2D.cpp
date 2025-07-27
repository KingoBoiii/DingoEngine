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
layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec4 a_Color;

layout(location = 0) out vec4 v_Color;

void main()
{
	gl_Position = a_Position;
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

	constexpr uint32_t MAX_QUADS = 1000;
	constexpr uint32_t MAX_QUAD_VERTEX_COUNT = MAX_QUADS * 4;
	constexpr uint32_t MAX_QUAD_INDEX_COUNT = 6 * MAX_QUADS;

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

		uint16_t* quadIndices = new uint16_t[MAX_QUAD_INDEX_COUNT];

		uint16_t offset = 0;
		for (uint32_t i = 0; i < MAX_QUAD_INDEX_COUNT; i += 6)
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
			.SetByteSize(sizeof(uint16_t) * MAX_QUAD_INDEX_COUNT)
			.SetInitialData(quadIndices)
			.Create();

		m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		m_QuadVertexBufferBase = new QuadVertex[MAX_QUADS];

		CreateQuadPipeline();
	}

	void Renderer2D::Shutdown()
	{
		DestroyQuadPipeline();

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

	void Renderer2D::Begin2D()
	{
		m_QuadIndexCount = 0;
		m_QuadVertexBufferPtr = m_QuadVertexBufferBase;
	}

	void Renderer2D::End2D()
	{
		m_CommandList->Begin();
		m_CommandList->Clear(m_TargetFramebuffer, 0, m_Params.ClearColor);

		if(m_QuadIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)m_QuadVertexBufferPtr - (uint8_t*)m_QuadVertexBufferBase);
			m_CommandList->UploadBuffer(m_QuadVertexBuffer, m_QuadVertexBufferBase, dataSize);

			m_CommandList->SetPipeline(m_QuadPipeline);
			m_CommandList->SetIndexBuffer(m_QuadIndexBuffer);
			m_CommandList->AddVertexBuffer(m_QuadVertexBuffer, 0, 0);

			m_CommandList->DrawIndexed(m_QuadIndexCount);
		}

		m_CommandList->End();
	}

	void Renderer2D::Clear(const glm::vec4& clearColor)
	{
		m_Params.ClearColor = clearColor;
	}

	void Renderer2D::Upload(GraphicsBuffer* buffer)
	{}

	void Renderer2D::Upload(GraphicsBuffer* buffer, const void* data, uint64_t size)
	{}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), { position.x, position.y, 0.0f }) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadVertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadVertexBufferPtr->Color = color;
			m_QuadVertexBufferPtr++;
		}

		m_QuadIndexCount += 6;
	}

	void Renderer2D::CreateQuadPipeline()
	{
		m_QuadShader = Shader::CreateFromSource("Renderer2DQuadShader", Shaders::Renderer2DQuadShader);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(QuadVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(QuadVertex, Position))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(QuadVertex, Color));

		m_QuadPipeline = PipelineBuilder()
			.SetDebugName("Renderer2DQuadPipeline")
			.SetFramebuffer(m_TargetFramebuffer)
			.SetShader(m_QuadShader)
			.SetVertexLayout(vertexLayout)
			.Create();

		m_QuadVertexBuffer = GraphicsBufferBuilder()
			.SetType(BufferType::VertexBuffer)
			.SetDebugName("Renderer2DQuadVertexBuffer")
			.SetByteSize(sizeof(QuadVertex) * MAX_QUAD_VERTEX_COUNT)
			.Create();
	}

	void Renderer2D::DestroyQuadPipeline()
	{
		if (m_QuadVertexBuffer)
		{
			m_QuadVertexBuffer->Destroy();
			delete[] m_QuadVertexBufferBase;
			m_QuadVertexBuffer = nullptr;
			m_QuadVertexBufferBase = nullptr;
			m_QuadVertexBufferPtr = nullptr;
		}
		m_QuadIndexCount = 0;

		if (m_QuadIndexBuffer)
		{
			m_QuadIndexBuffer->Destroy();
			m_QuadIndexBuffer = nullptr;
		}

		if (m_QuadPipeline)
		{
			m_QuadPipeline->Destroy();
			m_QuadPipeline = nullptr;
		}

		if (m_QuadShader)
		{
			m_QuadShader->Destroy();
			m_QuadShader = nullptr;
		}
	}


} // namespace Dingo
