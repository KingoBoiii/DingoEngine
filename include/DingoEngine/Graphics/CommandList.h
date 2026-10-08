#pragma once
#include "Framebuffer.h"
#include "Pipeline.h"
#include "GraphicsBuffer.h"
#include "RenderPass.h"
#include "ComputePass.h"
#include "Texture.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct CommandListParams
	{
	};

	// A rectangle of the bound framebuffer, in pixels from its top-left corner. Draws are scissored
	// to it as well.
	struct Viewport
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
	};

	class CommandList
	{
	public:
		static CommandList* Create(const CommandListParams& params = {});

	public:
		CommandList(const CommandListParams& params) 
			: m_Params(params)
		{}
		virtual ~CommandList() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual void Begin() = 0;
		virtual void Close() = 0;   // Seal the command list without submitting to the GPU
		virtual void Execute() = 0; // Submit a sealed command list to the GPU
		virtual void End() = 0;     // Close() + Execute() (convenience for single-threaded use)

		// True between Begin() and Close(): resource writes can be recorded into this list
		// instead of a throwaway one of their own.
		virtual bool IsRecording() const = 0;

		virtual void Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor = glm::vec3(0.3f)) = 0;

		virtual void UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;
		virtual void UploadTexture(Texture* texture, const void* data, uint64_t rowPitch) = 0;

		// Binds the framebuffer with a viewport covering all of it.
		virtual void SetFramebuffer(Framebuffer* framebuffer) = 0;
		// Narrows the viewport and scissor of the framebuffer SetFramebuffer bound, until the next
		// SetFramebuffer.
		virtual void SetViewport(const Viewport& viewport) = 0;
		// SetPipeline/SetRenderPass reset the graphics state to the pipeline and its bindings.
		// Neither binds a framebuffer -- follow with SetFramebuffer() before drawing.
		// Both return false when the pipeline could not be built (a failed shader compile,
		// including one from a hot-reload): the state is left empty and the caller must skip
		// the draw. Draw/DrawIndexed also refuse to submit without a pipeline.
		virtual bool SetPipeline(Pipeline* pipeline) = 0;
		virtual bool SetRenderPass(RenderPass* renderPass) = 0;
		virtual void AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		virtual void SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset = 0) = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;

		// Runs the compute pass over groupsX x groupsY x groupsZ thread groups. The graphics state
		// stays as it was set, so the next draw needs no rebinding. False when the pass can't run.
		virtual bool Dispatch(ComputePass* pass, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;

	protected:
		CommandListParams m_Params;
	};

}
