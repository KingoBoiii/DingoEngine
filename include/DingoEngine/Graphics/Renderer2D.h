#pragma once
#include "DingoEngine/Graphics/Renderer.h"

#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Font.h"

#include <array>
#include <vector>

#include <glm/glm.hpp>

#undef DrawText

namespace Dingo
{

	struct Renderer2DCapabilities
	{
		// Maximum quads per *batch*. The renderer auto-flushes into a new batch
		// when this is exceeded, so it is a draw-call/memory granularity knob, not
		// a hard limit on quads per frame. Larger = fewer draw calls, more memory
		// per batch buffer. Index values stay within MaxQuads*4 per batch.
		uint32_t MaxQuads = 2000;

		constexpr uint32_t GetQuadVertexCount() const { return MaxQuads * 4; }
		constexpr uint32_t GetQuadIndexCount()  const { return MaxQuads * 6; }
	};

	struct Renderer2DParams
	{
		glm::vec4 ClearColor = glm::vec4(1.0f);
		Renderer2DCapabilities Capabilities = {};
	};

	class Renderer2D
	{
	public:
		static Renderer2D* Create(const Renderer2DCapabilities& capabilities = {});
		static Renderer2D* Create(const Renderer2DParams& params);

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

		Texture* GetOutput() const { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }
		glm::vec2 GetViewportSize() const
		{
			auto* fb = Renderer::GetSwapChainFramebuffer();
			return glm::vec2(fb->GetParams().Width, fb->GetParams().Height);
		}

	private:
		Renderer2D(const Renderer2DParams& params) : m_Params(params) {}

	private:
		float GetTextureIndex(Texture* texture);

		// Submits the currently-accumulated batch of each pass (upload + draw) and
		// resets it so accumulation can continue into a fresh batch. No-op when the
		// pass has nothing accumulated.
		void FlushQuad();
		void FlushCircle();
		void FlushText();

		void ResetQuadTextureSlots();

		// Pooled per-batch GPU resources, created on demand and reused every frame.
		RenderPass* CreateQuadRenderPass();
		GraphicsBuffer* CreateQuadVertexBuffer();
		GraphicsBuffer* CreateCircleVertexBuffer();
		GraphicsBuffer* CreateTextVertexBuffer();

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
		uint32_t m_TextureSlotIndex = 1;

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

			// One (render pass, vertex buffer) per batch within a frame. Each batch
			// needs its own RenderPass because it binds a different texture set and
			// NVRHI binding sets are immutable once baked. Grown on demand, the pool
			// is reused (re-baked / re-uploaded) every frame; BatchIndex is the next
			// free slot for the frame in progress.
			std::vector<RenderPass*> RenderPasses;
			std::vector<GraphicsBuffer*> VertexBuffers;
			uint32_t BatchIndex = 0;
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
			// Bindings are frame-constant (camera only), so a single render pass is
			// baked once and shared by every batch; only the vertex buffers pool.
			RenderPass* RenderPass = nullptr;
			std::vector<GraphicsBuffer*> VertexBuffers;
			uint32_t BatchIndex = 0;
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
			// A single font atlas is used per frame, so one render pass is baked once
			// per frame (on the first flush) and shared by every text batch; only the
			// vertex buffers pool.
			RenderPass* RenderPass = nullptr;
			std::vector<GraphicsBuffer*> VertexBuffers;
			uint32_t BatchIndex = 0;
		} m_TextQuadRenderPass;
		Texture* m_FontAtlasTexture = nullptr;
	};

} // namespace Dingo
