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

			// Horizontally center the string on position.x instead of starting there. The
			// width is taken from the pen while the glyphs are emitted and the quads are
			// shifted afterwards, so this costs one walk of the string where
			// GetStringWidth() + DrawText() costs two. Multi-line strings center as a
			// block on their widest line, matching what GetStringWidth() reports.
			bool Centered = false;
		};

		void DrawText(const std::string& string, const Font* font, const glm::vec2& position, float size = 1.0f, const TextParameters& textParameters = {});
		void DrawText(const std::string& string, const Font* font, const glm::vec3& position, float size = 1.0f, const TextParameters& textParameters = {});

		// Per-scene render statistics: reset each BeginScene, complete after EndScene
		// (they reflect the most recent BeginScene/EndScene pass, not a whole frame).
		struct Statistics
		{
			uint32_t DrawCalls = 0;     // batches actually flushed (quad + circle + text)
			uint32_t QuadCount = 0;     // quads submitted (incl. rotated / textured)
			uint32_t CircleCount = 0;
			uint32_t TextQuadCount = 0; // glyph quads

			uint32_t GetTotalQuads() const { return QuadCount + CircleCount + TextQuadCount; }
			uint32_t GetVertexCount() const { return GetTotalQuads() * 4; }
			uint32_t GetIndexCount() const { return GetTotalQuads() * 6; }
		};

		const Statistics& GetStatistics() const { return m_Statistics; }
		const Renderer2DCapabilities& GetCapabilities() const { return m_Params.Capabilities; }

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

		void CreateQuadIndexBuffer();
		void CreateQuadPass();
		void CreateCirclePass();
		void CreateTextPass();

	private:
		// Binding slots for the Renderer2D shaders. All three declare the camera; only the
		// quad and text shaders declare the texture and sampler.
		static constexpr uint32_t k_CameraBinding = 0;
		static constexpr uint32_t k_TextureBinding = 1;
		static constexpr uint32_t k_SamplerBinding = 2;

		struct BatchPassParams
		{
			std::string Name;
			const char* ShaderSource = nullptr;
			VertexLayout VertexLayout;
			CullMode CullMode = CullMode::Back;
			Renderer2DCapabilities Capabilities;
			GraphicsBuffer* CameraUniformBuffer = nullptr;
			GraphicsBuffer* IndexBuffer = nullptr;
			// Null binds no sampler: a shader that declares none (circles) rejects the
			// binding set if one is in the desc.
			Sampler* BatchSampler = nullptr;
		};

		// One auto-batching pass. Vertices accumulate into VertexBufferBase; each flush
		// claims the next (vertex buffer, render pass) pair from a pool grown on demand
		// and reused every frame.
		//
		// Every batch owns its render pass and re-binds + re-bakes at flush, including
		// passes whose bindings never vary: an NVRHI binding set is immutable once baked,
		// so one pass shared across batches draws them all with the last batch's
		// resources — which is how a second font in a frame rendered from the first
		// font's atlas.
		template<typename TVertex>
		struct BatchPass
		{
			BatchPass() = default;

			// Owns raw shader/pipeline/render-pass/buffer pointers that Destroy() releases,
			// so a copy would double-release and a move would leave a live husk.
			BatchPass(const BatchPass&) = delete;
			BatchPass& operator=(const BatchPass&) = delete;

			uint32_t IndexCount = 0;
			TVertex* VertexBufferBase = nullptr;
			TVertex* VertexBufferPtr = nullptr;

			void Initialize(const BatchPassParams& params)
			{
				m_Params = params;

				m_Shader = Shader::CreateFromSource(m_Params.Name + "Shader", m_Params.ShaderSource);

				m_Pipeline = Pipeline::Create(PipelineParams()
					.SetDebugName(m_Params.Name + "Pipeline")
					.SetFramebuffer(Renderer::GetSwapChainFramebuffer())
					.SetShader(m_Shader)
					.SetVertexLayout(m_Params.VertexLayout)
					.SetCullMode(m_Params.CullMode)
					.SetDepthTest(false)
					.SetDepthWrite(false));

				VertexBufferBase = new TVertex[m_Params.Capabilities.GetQuadVertexCount()];
				VertexBufferPtr = VertexBufferBase;
			}

			void Destroy()
			{
				for (GraphicsBuffer*& vertexBuffer : m_VertexBuffers)
					DestroyAndDelete(vertexBuffer);
				m_VertexBuffers.clear();

				for (RenderPass*& renderPass : m_RenderPasses)
					DestroyAndDelete(renderPass);
				m_RenderPasses.clear();

				delete[] VertexBufferBase;
				VertexBufferBase = nullptr;
				VertexBufferPtr = nullptr;
				IndexCount = 0;
				m_BatchIndex = 0;

				DestroyAndDelete(m_Pipeline);
				DestroyAndDelete(m_Shader);
			}

			void Reset()
			{
				IndexCount = 0;
				VertexBufferPtr = VertexBufferBase;
				m_BatchIndex = 0;
			}

			bool HasRoomForQuad() const { return IndexCount + 6 <= m_Params.Capabilities.GetQuadIndexCount(); }
			bool HasRoomForQuads(size_t quadCount) const { return IndexCount + quadCount * 6ull <= m_Params.Capabilities.GetQuadIndexCount(); }

			// bindBatch receives this batch's render pass to bind whatever varies per
			// batch. Returns false when there was nothing accumulated to submit.
			template<typename TBindBatch>
			bool Flush(TBindBatch&& bindBatch)
			{
				if (IndexCount == 0)
					return false;

				if (m_BatchIndex >= m_VertexBuffers.size())
				{
					// DirectUpload = false: filled through Renderer::Upload (the deferred frame
					// command list) so the write is ordered before the draw within that one list.
					m_VertexBuffers.push_back(GraphicsBuffer::CreateVertexBuffer(sizeof(TVertex) * m_Params.Capabilities.GetQuadVertexCount(), nullptr, false, m_Params.Name + "VertexBuffer"));
					m_RenderPasses.push_back(CreateRenderPass());
				}

				GraphicsBuffer* vertexBuffer = m_VertexBuffers[m_BatchIndex];
				RenderPass* renderPass = m_RenderPasses[m_BatchIndex];

				uint32_t dataSize = (uint32_t)((uint8_t*)VertexBufferPtr - (uint8_t*)VertexBufferBase);
				Renderer::Upload(vertexBuffer, VertexBufferBase, dataSize);

				bindBatch(renderPass);
				renderPass->Bake();

				Renderer::DrawIndexed(renderPass, vertexBuffer, m_Params.IndexBuffer, IndexCount);

				m_BatchIndex++;
				IndexCount = 0;
				VertexBufferPtr = VertexBufferBase;
				return true;
			}

			bool Flush() { return Flush([](RenderPass*) {}); }

		private:
			RenderPass* CreateRenderPass()
			{
				RenderPass* renderPass = RenderPass::Create(RenderPassParams().SetPipeline(m_Pipeline));
				renderPass->Initialize();
				renderPass->SetUniformBuffer(k_CameraBinding, m_Params.CameraUniformBuffer);

				if (m_Params.BatchSampler)
					renderPass->SetSampler(k_SamplerBinding, m_Params.BatchSampler);

				return renderPass;
			}

		private:
			BatchPassParams m_Params;
			Shader* m_Shader = nullptr;
			Pipeline* m_Pipeline = nullptr;

			std::vector<RenderPass*> m_RenderPasses;
			std::vector<GraphicsBuffer*> m_VertexBuffers;
			uint32_t m_BatchIndex = 0;
		};

	private:
		/**************************************************
		***		GENERAL									***
		**************************************************/
		Renderer2DParams m_Params;
		Statistics m_Statistics;
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
		***		PASSES									***
		**************************************************/
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			float TexIndex = 0.0f;
		};

		struct CircleVertex
		{
			glm::vec3 WorldPosition;
			glm::vec3 LocalPosition;
			glm::vec4 Color;
			float Thickness;
			float Fade;
		};

		struct TextVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
		};

		BatchPass<QuadVertex> m_QuadPass;
		BatchPass<CircleVertex> m_CirclePass;
		BatchPass<TextVertex> m_TextPass;

		// The atlas the text batch in progress samples. DrawText closes that batch when
		// the font changes, so consecutive batches can hold different atlases.
		Texture* m_FontAtlasTexture = nullptr;
	};

} // namespace Dingo
