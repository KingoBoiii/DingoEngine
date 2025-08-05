#include "depch.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include "DingoEngine/Core/Application.h"

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
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;

layout (std140, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out flat float v_TexIndex;

void main()
{
	gl_Position = ProjectionView * vec4(a_Position, 1.0);
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
}

#type fragment
#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat float v_TexIndex;

layout(location = 0) out vec4 o_Color;

layout (set = 0, binding = 1) uniform texture2D u_Textures[32];
layout (set = 0, binding = 2) uniform sampler u_Sampler;

void main()
{
	o_Color = texture(sampler2D(u_Textures[int(v_TexIndex)], u_Sampler), v_TexCoord) * v_Color;
	
	if (o_Color.a == 0.0)
	{
		discard; // Skip rendering if the color is fully transparent
	}
}
		)";

	}

	Renderer2D* Renderer2D::Create(Framebuffer* framebuffer, const Renderer2DCapabilities& capabilities)
	{
		return new Renderer2D(Renderer2DParams{
			.TargetFramebuffer = framebuffer,
			.Capabilities = capabilities
			});
	}

	Renderer2D* Renderer2D::Create(const Renderer2DParams& params)
	{
		return new Renderer2D(params);
	}

	void Renderer2D::Initialize()
	{
		m_Renderer = Renderer::Create(RendererParams{
			.TargetSwapChain = false,
			.FramebufferName = "Renderer2DFramebuffer" });
		m_Renderer->Initialize();

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

		// Set all texture slots to 0
		m_TextureSlots[0] = Renderer::GetWhiteTexture();
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
		{
			m_TextureSlots[i] = nullptr;
		}

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

		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void Renderer2D::Resize(uint32_t width, uint32_t height)
	{
		m_Renderer->Resize(width, height);
	}

	void Renderer2D::BeginScene(const glm::mat4& projectionViewMatrix)
	{
		m_CameraData.ProjectionViewMatrix = projectionViewMatrix;
		m_CameraUniformBuffer->Upload(&m_CameraData, sizeof(CameraData));

		m_QuadPipeline.IndexCount = 0;
		m_QuadPipeline.VertexBufferPtr = m_QuadPipeline.VertexBufferBase;

		m_TextureSlotIndex = 1; // Start from 1 since index 0 is reserved for the white texture
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
		{
			m_TextureSlots[i] = nullptr; // Reset texture slots
		}
	}

	void Renderer2D::EndScene()
	{
		if (m_QuadPipeline.IndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)m_QuadPipeline.VertexBufferPtr - (uint8_t*)m_QuadPipeline.VertexBufferBase);
			m_QuadPipeline.VertexBuffer->Upload(m_QuadPipeline.VertexBufferBase, dataSize);

			for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			{
				if (m_TextureSlots[i])
				{
					m_QuadPipeline.RenderPass->SetTexture(1, m_TextureSlots[i], i);
					continue;
				}

				m_QuadPipeline.RenderPass->SetTexture(1, m_TextureSlots[0], i);
			}
			m_QuadPipeline.RenderPass->Bake();

			m_Renderer->BeginRenderPass(m_QuadPipeline.RenderPass);

			m_Renderer->Clear(m_Params.ClearColor);

			m_Renderer->DrawIndexed(m_QuadPipeline.VertexBuffer, m_QuadIndexBuffer, m_CameraUniformBuffer, m_QuadPipeline.IndexCount);

			m_Renderer->EndRenderPass();
		}
	}

	void Renderer2D::Clear(const glm::vec4& clearColor)
	{
		m_Params.ClearColor = clearColor;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
		{
			DE_CORE_ERROR("Renderer2D: Quad index count exceeded the maximum limit.");
			return;
		}

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = 0.0f;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
		{
			DE_CORE_ERROR("Renderer2D: Quad index count exceeded the maximum limit.");
			return;
		}

		float textureIndex = GetTextureIndex(texture);

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = textureIndex;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
	}

	float Renderer2D::GetTextureIndex(Texture* texture)
	{
		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->NativeEquals(texture))
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			//if (m_TextureSlotIndex >= MaxTextureSlots)
			//	FlushAndReset();

			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		return textureIndex;
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
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(QuadVertex, Color))
			.AddAttribute("a_TexCoord", Format::RGBA32_FLOAT, offsetof(QuadVertex, TexCoord))
			.AddAttribute("a_TexIndex", Format::R32_FLOAT, offsetof(QuadVertex, TexIndex));

		m_QuadPipeline.Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Renderer2DQuadPipeline")
			.SetFramebuffer(m_Renderer->GetTargetFramebuffer())
			.SetShader(m_QuadPipeline.Shader)
			.SetVertexLayout(vertexLayout)
			.SetCullMode(CullMode::BackAndFront));

		m_QuadPipeline.VertexBuffer = GraphicsBufferBuilder()
			.SetType(BufferType::VertexBuffer)
			.SetDebugName("Renderer2DQuadVertexBuffer")
			.SetByteSize(sizeof(QuadVertex) * m_Params.Capabilities.GetQuadVertexCount())
			.SetDirectUpload(true)
			.Create();

		RenderPassParams renderPassParams = RenderPassParams()
			.SetPipeline(m_QuadPipeline.Pipeline);

		m_QuadPipeline.RenderPass = RenderPass::Create(renderPassParams);
		m_QuadPipeline.RenderPass->Initialize();
		m_QuadPipeline.RenderPass->SetUniformBuffer(0, m_CameraUniformBuffer);
		m_QuadPipeline.RenderPass->SetSampler(2, Renderer::GetClampSampler());
		for (uint32_t i = 0; i < MaxTextureSlots; i++)
		{
			m_QuadPipeline.RenderPass->SetTexture(1, Renderer::GetWhiteTexture(), i);
		}
		m_QuadPipeline.RenderPass->Bake();

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

		if (m_QuadPipeline.RenderPass)
		{
			m_QuadPipeline.RenderPass->Destroy();
			m_QuadPipeline.RenderPass = nullptr;
		}
	}


} // namespace Dingo
