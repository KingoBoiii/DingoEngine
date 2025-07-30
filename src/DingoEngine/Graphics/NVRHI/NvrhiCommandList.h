#pragma once
#include "DingoEngine/Graphics/CommandList.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiCommandList : public CommandList
	{
	public:
		NvrhiCommandList(const CommandListParams& params)
			: CommandList(params)
		{}
		~NvrhiCommandList() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		//virtual void Begin() override;
		virtual void Begin(Framebuffer* framebuffer) override;
		virtual void End() override;

		virtual void Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor = glm::vec3(0.3f)) override;

		virtual void UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset = 0) override;

		virtual void SetPipeline(Pipeline* pipeline) override;
		virtual void SetRenderPass(RenderPass* renderPass) override;
		virtual void AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot = 0, uint64_t offset = 0) override;
		virtual void SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset = 0) override;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) override;

	private:
		bool m_HasBegun = false; // Track if the command list has begun
		nvrhi::GraphicsState m_GraphicsState;
		nvrhi::CommandListHandle m_CommandListHandle;
	};

}
