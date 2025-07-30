#pragma once
#include "DingoEngine/Graphics/Renderer.h"

#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct Renderer2DCapabilities
	{
		uint32_t MaxQuads = 1000;					// Maximum number of quads that can be drawn in a single frame

		constexpr uint32_t GetQuadVertexCount() const
		{
			return MaxQuads * 4;					// 4 vertices per quad
		}

		constexpr uint32_t GetQuadIndexCount() const
		{
			return MaxQuads * 6;					// 6 indices per quad (2 triangles)
		}
	};

	struct Renderer2DParams
	{
		Framebuffer* TargetFramebuffer = nullptr;
		glm::vec4 ClearColor = glm::vec4(1.0f);
		Renderer2DCapabilities Capabilities = {};
	};

	class Renderer2D
	{
	public:
		static Renderer2D* Create(Framebuffer* framebuffer, const Renderer2DCapabilities& capabilities = {});
		static Renderer2D* Create(const Renderer2DParams& params = {});

	public:
		Renderer2D() = delete;
		Renderer2D(const Renderer2D&) = delete;
		Renderer2D& operator=(const Renderer2D&) = delete;
		Renderer2D(Renderer2D&&) = delete;
		Renderer2D& operator=(Renderer2D&&) = delete;
		virtual ~Renderer2D() = default;

	public:
		void Initialize();
		void Shutdown();
		void Resize(uint32_t width, uint32_t height);

		void BeginScene(const glm::mat4& projectionViewMatrix);
		void EndScene();

		void Clear(const glm::vec4& clearColor);

		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);

		Texture* GetOutput() const { return m_Renderer->GetOutput(); }
		glm::vec2 GetViewportSize() const
		{
			return glm::vec2(m_Renderer->GetTargetFramebuffer()->GetParams().Width, m_Renderer->GetTargetFramebuffer()->GetParams().Height);
		}

	private:
		Renderer2D(const Renderer2DParams& params)
			: m_Params(params)
		{}

	private:
		void CreateQuadIndexBuffer();
		void CreateQuadPipeline();
		void DestroyQuadPipeline();

	private:
		/**************************************************
		***		GENERAL									***
		**************************************************/
		Renderer2DParams m_Params;
		Renderer* m_Renderer = nullptr;
		GraphicsBuffer* m_QuadIndexBuffer = nullptr;

		struct CameraData
		{
			glm::mat4 ProjectionViewMatrix;
		};
		CameraData m_CameraData = {};
		GraphicsBuffer* m_CameraUniformBuffer = nullptr;

		glm::vec4 m_QuadVertexPositions[4] = {};

		/**************************************************
		***		QUAD									***
		**************************************************/
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
		};

		struct QuadPipeline
		{
			uint32_t IndexCount = 0;
			QuadVertex* VertexBufferBase = nullptr;
			QuadVertex* VertexBufferPtr = nullptr;

			Shader* Shader = nullptr;
			Pipeline* Pipeline = nullptr;
			RenderPass* RenderPass = nullptr;
			GraphicsBuffer* VertexBuffer = nullptr;
		} m_QuadPipeline;
	};

} // namespace Dingo
