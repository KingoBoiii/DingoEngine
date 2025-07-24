#include "depch.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

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

		m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		m_QuadVertexBufferBase = new QuadVertex[1];
	}

	void Renderer2D::Shutdown()
	{
		if (m_CommandList)
		{
			m_CommandList->Destroy();
			m_CommandList = nullptr;
		}

		if (m_TargetFramebuffer)
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


} // namespace Dingo
