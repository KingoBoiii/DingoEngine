#pragma once
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/CommandList.h"

#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct Renderer2DParams
	{
		Framebuffer* TargetFramebuffer = nullptr; // nullptr = use swap chain
		glm::vec4 ClearColor = glm::vec4(1.0f);
	};

	class Renderer2D
	{
	public:
		Renderer2D(const Renderer2DParams& params)
			: m_Params(params)
		{}
		virtual ~Renderer2D() = default;

	public:
		void Initialize();
		void Shutdown();
		void Resize(uint32_t width, uint32_t height);

		void Begin2D(const glm::mat4& projectionViewMatrix);
		void End2D();

		void Clear(const glm::vec4& clearColor);

		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);

		Texture* GetOutput() const { return m_TargetFramebuffer->GetAttachment(0); }
		glm::vec2 GetViewportSize() const
		{
			return glm::vec2(m_TargetFramebuffer->GetParams().Width, m_TargetFramebuffer->GetParams().Height);
		}

	private:
		void CreateQuadPipeline();
		void DestroyQuadPipeline();

	protected:
		Renderer2DParams m_Params;
		Framebuffer* m_TargetFramebuffer = nullptr;
		CommandList* m_CommandList = nullptr;

		struct CameraData
		{
			glm::mat4 ProjectionViewMatrix;
		};
		CameraData m_CameraData = {};
		GraphicsBuffer* m_CameraUniformBuffer = nullptr;

		glm::vec4 m_QuadVertexPositions[4] = {};

		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
		};

		uint32_t m_QuadIndexCount = 0;
		QuadVertex* m_QuadVertexBufferBase = nullptr;
		QuadVertex* m_QuadVertexBufferPtr = nullptr;

		Shader* m_QuadShader = nullptr;
		Pipeline* m_QuadPipeline = nullptr;
		GraphicsBuffer* m_QuadVertexBuffer = nullptr;
		GraphicsBuffer* m_QuadIndexBuffer = nullptr;
	};

} // namespace Dingo
