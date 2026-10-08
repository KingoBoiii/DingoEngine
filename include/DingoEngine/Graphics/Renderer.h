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

#include <functional>
#include <optional>
#include <vector>

namespace Dingo
{

	class SwapChain;
	class PostProcessStack;

	// One GPU pass timer (Renderer::BeginGpuTimer), over the last GpuTimers::k_HistoryLength frames
	// that measured it. A frame's sample adds up every time the timer ran in that frame.
	struct GpuTimerStats
	{
		const char* Name = nullptr;
		uint32_t Depth = 0;   // timers open around it when it was first seen
		uint32_t Samples = 0; // frames in the history, at most 120
		float LastMs = 0.0f;
		float MeanMs = 0.0f;
		float MaxMs = 0.0f;
	};

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

		// Stands in for BeginFrame/EndFrame in a frame that renders nothing (Application skips
		// frames while the window is minimized): it waits for the render thread to finish the
		// frame in flight, as BeginFrame does, but opens no command list. Until the next
		// BeginFrame, the Upload, Clear, Draw and DrawIndexed calls below are no-ops, and so are
		// Renderer2D and Renderer3D scenes and SceneRenderer::Render. A dropped Upload is not
		// redone later: data written once belongs in a DirectUpload buffer. Code that records into
		// GetCommandList() itself, or calls Begin/Close/Execute, must check IsFrameSkipped() first.
		// A BeginFrame that gets no swap-chain image to draw into (Vulkan, a window that can't be
		// presented to) reports IsFrameSkipped() too, with the same no-ops, though its command list
		// is open and its EndFrame still runs.
		static void SkipFrame();
		static bool IsFrameSkipped();

		// Thread-safe: records the new size and returns. The swap chain is recreated at the next
		// safe point: on the render thread after Present, before the next image acquire, or in a
		// BeginFrame that has no image yet. Resizing it here would race the frame in flight.
		static void QueueResize(int32_t width, int32_t height);

		/**************************************************
		***		GPU TIMERS								***
		**************************************************/

		// Measures the GPU time of the commands recorded between the two calls, which nest and must
		// pair up within a frame. `name` must outlive the call only. Results are read four frames
		// later, never waiting for the GPU (GetGpuTimers, the F8 Profiler tab, and Tracy plots in a
		// --profile build). In a frame that renders nothing they measure nothing. "Frame" times the
		// whole frame command list. At most 32 timers a frame; later ones warn once and are skipped.
		static void BeginGpuTimer(const char* name);
		static void EndGpuTimer();
		static const std::vector<GpuTimerStats>& GetGpuTimers();

		// How long the render thread spent on the last frame it submitted: executing the command
		// list and presenting.
		static float GetRenderThreadMilliseconds();

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
		static void DrawIndexed(RenderPass* renderPass, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0, uint32_t instanceCount = 1);
		// Without vertex buffers: the vertex stage builds its vertices from gl_VertexIndex.
		static void Draw(RenderPass* renderPass, uint32_t vertexCount, uint32_t instanceCount = 1);

		/**************************************************
		***		DRAW — Material							***
		**************************************************/

		// Lazily creates (and caches) the pipeline + render pass for the given
		// vertex layout, uploads the uniforms (once per frame and after each SetUniform), then draws.
		static void DrawIndexed(Material* material, const VertexLayout& layout, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, uint32_t indexCount = 0, uint32_t instanceCount = 1);
		// Without vertex buffers, as for a fullscreen pass: Draw(material, 3) with a vertex stage that
		// makes one triangle covering the target from gl_VertexIndex (DingoEngine/Fullscreen.glsl).
		static void Draw(Material* material, uint32_t vertexCount, uint32_t instanceCount = 1);

		// Runs a compute pass in the frame's command list, ordered with the draws around it. Nothing
		// in a frame that renders nothing (IsFrameSkipped).
		static void Dispatch(ComputePass* pass, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);

		/**************************************************
		***		QUERIES									***
		**************************************************/

		// Override the render target used by all no-arg draw/clear calls.
		// Pass nullptr (or call ResetRenderTarget) to revert to the swap chain. Either also drops a
		// SetViewport.
		static void SetRenderTarget(Framebuffer* framebuffer);
		static void ResetRenderTarget();

		// Limits the draws that follow to a rectangle of the current render target (a shadow-atlas
		// tile, a half-resolution pass), until ResetViewport or the next SetRenderTarget. Clears still
		// clear the whole target.
		static void SetViewport(const Viewport& viewport);
		static void ResetViewport();
		// The SetViewport rectangle in force, or empty while draws cover the whole target.
		static std::optional<Viewport> GetViewport();
		// The override, or null while draws go to the swap chain.
		static Framebuffer* GetRenderTarget();

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

		// Bumped each time the frame command list opens, so per-frame budgets (such as a volatile
		// buffer's writes) can tell when a new frame starts.
		static uint64_t GetFrameIndex();

		/**************************************************
		***		STATIC RESOURCES						***
		**************************************************/

		static Texture* GetWhiteTexture();
		static Sampler* GetClampSampler();
		static Sampler* GetPointSampler();

		// The 3D pass's post chain (PostProcess.h), shared by SceneRenderer and direct Renderer3D use.
		static PostProcessStack& GetPostProcessStack();

	private:
		static void RenderThreadLoop();
		static Framebuffer* GetCurrentTarget();
		static void BindTarget(Framebuffer* target);
		static RenderPass* PrepareMaterial(Material* material, const VertexLayout& layout, Framebuffer* target);

		// Runs fn on the main thread at the start of the next frame, or at Shutdown, once the
		// render thread has submitted the frame being recorded now: for reading back what it drew.
		static void RunAfterFrame(std::function<void()> fn);
		static void RunPendingAfterFrame();
		// While true the main thread may submit a command list of its own: the render thread waits
		// for the next frame. False from EndFrame until the next BeginFrame or SkipFrame.
		static bool IsRenderThreadParked();

		static struct RendererData* s_Data;

		friend class NvrhiTexture;
		friend class NvrhiGraphicsBuffer;
	};

}
