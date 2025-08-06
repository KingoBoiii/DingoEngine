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
		bool TargetSwapChain = false; // If true, the command list will target the swap chain framebuffer
		Framebuffer* TargetFramebuffer = nullptr; // If not targeting swap chain, specify a framebuffer to target

		CommandListParams& SetTargetSwapChain(bool targetSwapChain)
		{
			TargetSwapChain = targetSwapChain;
			return *this;
		}

		CommandListParams& SetTargetFramebuffer(Framebuffer* framebuffer)
		{
			TargetFramebuffer = framebuffer;
			return *this;
		}
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
		virtual void Begin(Framebuffer* framebuffer) = 0; // TODO: shouldn't be used, use Begin() instead
		virtual void End() = 0;

		virtual void Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor = glm::vec3(0.3f)) = 0;

		virtual void UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;

		virtual void SetPipeline(Pipeline* pipeline) = 0;
		virtual void SetRenderPass(RenderPass* renderPass) = 0;
		virtual void AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		virtual void SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset = 0) = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;

	protected:
		CommandListParams m_Params;
		Framebuffer* m_TargetFramebuffer = nullptr;
	};

}
