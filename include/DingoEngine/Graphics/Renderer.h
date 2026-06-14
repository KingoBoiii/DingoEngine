#pragma once

#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/Material.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class SwapChain;

	// Renderer is a stateless gateway: all draw calls require explicit
	// resources (Pipeline or RenderPass, vertex/index buffers, etc.).
	// No per-draw implicit state is stored between calls.
	class Renderer
	{
	public:
		Renderer() = delete;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		/**************************************************
		***		LIFECYCLE								***
		**************************************************/

		static void Initialize(SwapChain* swapChain);
		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();

		/**************************************************
		***		COMMAND LIST MANAGEMENT					***
		**************************************************/

		static void Begin();
		static void Close();
		static void Execute();

		/**************************************************
		***		RESOURCE UPLOAD							***
		**************************************************/

		static void Upload(GraphicsBuffer* buffer);
		static void Upload(GraphicsBuffer* buffer, const void* data, uint64_t size);

		/**************************************************
		***		CLEAR									***
		**************************************************/

		static void Clear(Framebuffer* framebuffer, const glm::vec4& clearColor);
		static void Clear(const glm::vec4& clearColor);

		/**************************************************
		***		DRAW — explicit Pipeline				***
		**************************************************/

		static void Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount = 1);
		static void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount = 1);
		static void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0);

		/**************************************************
		***		DRAW — explicit RenderPass				***
		**************************************************/

		// Self-contained: sets render pass bindings + framebuffer, then draws.
		static void DrawIndexed(RenderPass* renderPass, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0);

		/**************************************************
		***		DRAW — Material							***
		**************************************************/

		// Lazily creates (and caches) the pipeline + render pass for the given
		// vertex layout, uploads dirty uniforms, then draws.
		static void DrawIndexed(Material* material, const VertexLayout& layout, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0);

		/**************************************************
		***		QUERIES									***
		**************************************************/

		// Override the render target used by all no-arg draw/clear calls.
		// Pass nullptr (or call ResetRenderTarget) to revert to the swap chain.
		static void SetRenderTarget(Framebuffer* framebuffer);
		static void ResetRenderTarget();

		static CommandList*  GetCommandList();
		static Framebuffer*  GetSwapChainFramebuffer();

		/**************************************************
		***		STATIC RESOURCES						***
		**************************************************/

		static Texture* GetWhiteTexture();
		static Sampler* GetClampSampler();
		static Sampler* GetPointSampler();

	private:
		static void RenderThreadLoop();
		static Framebuffer* GetCurrentTarget();

		static struct RendererData* s_Data;
	};

}
