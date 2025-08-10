#pragma once
#include "Framebuffer.h"
#include "Pipeline.h"
#include "GraphicsBuffer.h"
#include "RenderPass.h"

#include <glm/glm.hpp>

namespace Dingo
{

	struct CommandListParams
	{
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
		virtual void End() = 0;

		virtual void Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor = glm::vec3(0.3f)) = 0;

		virtual void UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;

		virtual void SetFramebuffer(Framebuffer* framebuffer) = 0;
		virtual void SetPipeline(Pipeline* pipeline) = 0;
		virtual void SetRenderPass(RenderPass* renderPass) = 0;
		virtual void AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		virtual void SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset = 0) = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;

	protected:
		CommandListParams m_Params;
	};

}
