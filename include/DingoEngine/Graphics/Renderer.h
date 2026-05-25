#pragma once

#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Sampler.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class SwapChain;
	struct RendererData;

	class Renderer
	{
	public:
		Renderer() = delete;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		static void Initialize(SwapChain* swapChain);
		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();

		/**************************************************
		***		RENDER PASS API							***
		**************************************************/

		static void BeginRenderPass(RenderPass* renderPass);
		static void EndRenderPass();

		/**************************************************
		***		DRAW CALLS								***
		**************************************************/

		static void DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0);
		static void DrawIndexed(GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer, uint32_t indexCount = 0);

		/**************************************************
		***		GENERAL									***
		**************************************************/

		static void Begin();
		static void Close();
		static void Execute();
		static void End();

		static void Clear(Framebuffer* framebuffer, const glm::vec4& clearColor);
		static void Clear(const glm::vec4& clearColor);

		static void Upload(GraphicsBuffer* buffer);
		static void Upload(GraphicsBuffer* buffer, const void* data, uint64_t size);

		static void Draw(Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		static void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		static void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer);
		static void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer);

		static Framebuffer* GetTargetFramebuffer();
		static Texture* GetOutput();

		/**************************************************
		***		STATIC RESOURCES 						***
		**************************************************/

		static Texture* GetWhiteTexture();
		static Sampler* GetClampSampler();
		static Sampler* GetPointSampler();

	private:
		static void RenderThreadLoop();

		static RendererData* s_Data;
	};

}
