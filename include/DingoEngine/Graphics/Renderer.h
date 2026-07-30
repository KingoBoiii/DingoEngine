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

	// Ownership rule for every graphics resource: the factories hand out a raw `new`, and
	// Destroy() only releases the GPU handle — the host object is still the owner's. Every
	// owner pairs the two through this, so the pairing is not re-decided per site.
	template<typename T>
	void DestroyAndDelete(T*& resource)
	{
		if (!resource)
			return;

		resource->Destroy();
		delete resource;
		resource = nullptr;
	}

	// GraphicsBuffer's destructor is protected (only its factories construct one), so the
	// wrapper has to be deleted through the public polymorphic base it shares.
	inline void DestroyAndDelete(GraphicsBuffer*& buffer)
	{
		if (!buffer)
			return;

		buffer->Destroy();
		delete static_cast<GenericGraphicsBuffer<const void>*>(buffer);
		buffer = nullptr;
	}

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

		// Parks the render thread, submits any frame still in flight and drops the frame
		// command list, so the GPU is idle and nothing the renderer recorded still
		// references a resource. Queries and the shared static resources below stay valid
		// afterwards — Layer::OnDetach runs between this and Destroy(), and freeing GPU
		// resources there is exactly what it is for.
		static void Shutdown();

		// Frees the renderer's own static resources and internal state. Every query below
		// asserts and returns null after this point, so it must come after the layer stack
		// has been detached.
		static void Destroy();

		static void BeginFrame();
		static void EndFrame();

		// Thread-safe: records the new size and returns. The swap chain is recreated on the
		// render thread at the next safe point (after Present, before the next image acquire) --
		// resizing it here would race the frame currently in flight.
		static void QueueResize(int32_t width, int32_t height);

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

		// indexCount = 0 means "the whole index buffer", sized from the buffer's own
		// GraphicsFormat (Uint16/Uint32).
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

		// The frame command list only while it is open, else null — and null rather than an
		// assert before Initialize() or after Destroy(). For resource writes that want to
		// join the frame instead of opening a list of their own, but must also work when
		// called outside one (asset loads during OnAttach, say).
		static CommandList* TryGetRecordingCommandList();
		static Framebuffer*  GetSwapChainFramebuffer();

		// True while `framebuffer` is one the swap chain currently owns. Those are freed
		// and recreated on every resize, so anything holding one must re-resolve rather
		// than dereference what it captured.
		static bool IsSwapChainFramebuffer(const Framebuffer* framebuffer);

		// Bumped every time the swap chain recreates its framebuffers. Anything that caches
		// objects built against one must compare this rather than trusting the pointer,
		// which the allocator is free to hand back for a different framebuffer.
		static uint64_t GetSwapChainResizeGeneration();

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
