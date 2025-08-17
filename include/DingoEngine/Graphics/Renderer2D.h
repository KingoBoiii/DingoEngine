#pragma once
#include "DingoEngine/Graphics/Renderer.h"

#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Font.h"

#include <array>

#include <glm/glm.hpp>

#undef DrawText

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
		static Renderer2D* Create(Renderer* renderer, const Renderer2DCapabilities& capabilities = {});
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

		void BeginScene(const glm::mat4& projectionViewMatrix);
		void EndScene();

		void Clear(const glm::vec4& clearColor);

		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);

		void DrawQuad(const glm::vec2& position, const glm::vec2& size, Texture* texture, const glm::vec4& color = glm::vec4(1.0f));
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture* texture, const glm::vec4& color = glm::vec4(1.0f));

		void DrawRotatedQuad(const glm::vec2& position, float rotation, const glm::vec2& size, Texture* texture, const glm::vec4& color = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec3& position, float rotation, const glm::vec2& size, Texture* texture, const glm::vec4& color = glm::vec4(1.0f));

		void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f, float fade = 0.005f);

		struct TextParameters
		{
			glm::vec4 Color{ 1.0f };
			float Kerning = 0.0f;
			float LineSpacing = 0.0f;
		};

		void DrawText(const std::string& string, const Font* font, const glm::vec2& position, float size = 1.0f, const TextParameters& textParameters = {});
		void DrawText(const std::string& string, const Font* font, const glm::vec3& position, float size = 1.0f, const TextParameters& textParameters = {});

		Texture* GetOutput() const { return m_Renderer->GetOutput(); }
		glm::vec2 GetViewportSize() const
		{
			return glm::vec2(m_Renderer->GetTargetFramebuffer()->GetParams().Width, m_Renderer->GetTargetFramebuffer()->GetParams().Height);
		}

	private:
		Renderer2D(const Renderer2DParams& params)
			: m_Params(params), m_OwnsRenderer(true)
		{}

		Renderer2D(Renderer* renderer, const Renderer2DParams& params)
			: m_Params(params), m_Renderer(renderer), m_OwnsRenderer(false)
		{}

	private:
		float GetTextureIndex(Texture* texture);

		void CreateQuadIndexBuffer();
		void CreateQuadPipeline();
		void DestroyQuadPipeline();

		void CreateCircleRenderPass();
		void DestroyCircleRenderPass();
		
		void CreateTextQuadRenderPass();
		void DestroyTextQuadRenderPass();

	private:
		/**************************************************
		***		GENERAL									***
		**************************************************/
		Renderer2DParams m_Params;
		Renderer* m_Renderer = nullptr;
		bool m_OwnsRenderer = true; // If true, Renderer2D will destroy the renderer on shutdown
		GraphicsBuffer* m_QuadIndexBuffer = nullptr;

		struct CameraData
		{
			glm::mat4 ProjectionViewMatrix;
		};
		CameraData m_CameraData = {};
		GraphicsBuffer* m_CameraUniformBuffer = nullptr;

		glm::vec4 m_QuadVertexPositions[4] = {};
		glm::vec2 m_TextureCoords[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		static const uint32_t MaxTextureSlots = 32;
		std::array<Texture*, MaxTextureSlots> m_TextureSlots = {};
		uint32_t m_TextureSlotIndex = 1; // 0 = white texture

		/**************************************************
		***		QUAD									***
		**************************************************/
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			float TexIndex = 0.0f;
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

		/**************************************************
		***		CIRCLE									***
		**************************************************/
		struct CircleVertex
		{
			glm::vec3 WorldPosition;
			glm::vec3 LocalPosition;
			glm::vec4 Color;
			float Thickness;
			float Fade;
		};

		struct CircleRenderPass
		{
			uint32_t IndexCount = 0;
			CircleVertex* VertexBufferBase = nullptr;
			CircleVertex* VertexBufferPtr = nullptr;

			Shader* Shader = nullptr;
			Pipeline* Pipeline = nullptr;
			RenderPass* RenderPass = nullptr;
			GraphicsBuffer* VertexBuffer = nullptr;
		} m_CircleRenderPass;

		/**************************************************
		***		TEXT									***
		**************************************************/
		struct TextVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
		};

		struct TextQuadRenderPass
		{
			uint32_t IndexCount = 0;
			TextVertex* VertexBufferBase = nullptr;
			TextVertex* VertexBufferPtr = nullptr;

			Shader* Shader = nullptr;
			Pipeline* Pipeline = nullptr;
			RenderPass* RenderPass = nullptr;
			GraphicsBuffer* VertexBuffer = nullptr;
		} m_TextQuadRenderPass;
		Texture* m_FontAtlasTexture = nullptr;
	};

} // namespace Dingo
