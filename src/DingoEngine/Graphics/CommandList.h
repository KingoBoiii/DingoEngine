#pragma once
#include "DingoEngine/Graphics/ComputePass.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Texture.h"
#include "DingoEngine/Graphics/Viewport.h"

#include <glm/glm.hpp>

namespace Dingo::Internal
{

	struct CommandListParams
	{
	};

	class CommandList
	{
	public:
		[[nodiscard]] static CommandList* Create(const CommandListParams& params = {});

	public:
		CommandList(const CommandListParams& params) 
			: m_Params(params)
		{}
		virtual ~CommandList() = default;

	protected:
		// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		// Releases the GPU object early; the destructor releases everything anyway.
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
		[[nodiscard]] virtual bool SetPipeline(Pipeline* pipeline) = 0;
		[[nodiscard]] virtual bool SetRenderPass(RenderPass* renderPass) = 0;
		virtual void AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		virtual void SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset = 0) = 0;
		// The bytes the next draws pass to the shader's push-constant block, up to its size; the rest
		// stays zero. SetPipeline/SetRenderPass zero them.
		virtual void SetPushConstants(const void* data, uint32_t size) = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) = 0;

		// Runs the compute pass over groupsX x groupsY x groupsZ thread groups. The graphics state
		// stays as it was set, so the next draw needs no rebinding. False when the pass can't run.
		virtual bool Dispatch(ComputePass* pass, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;

		// How many draws followed an UploadBuffer or UploadTexture into the target the draw before
		// them used. On Vulkan each such upload ends the render pass and the draw begins it again.
		// Uploads made outside this command list, readbacks and barriers are not counted.
		uint64_t GetUploadPassBreaks() const { return m_UploadPassBreaks; }

	protected:
		CommandListParams m_Params;
		uint64_t m_UploadPassBreaks = 0;
	};

}
