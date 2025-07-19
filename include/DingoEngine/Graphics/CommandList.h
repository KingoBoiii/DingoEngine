#pragma once
#include "Framebuffer.h"
#include "Pipeline.h"
#include "GraphicsBuffer.h"

#include <nvrhi/nvrhi.h>

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
		CommandList(const CommandListParams& params);
		~CommandList() = default;

	public:
		void Initialize();
		void Destroy();

		void Begin();
		void End();

		void Clear();
		void Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor = glm::vec3(0.3f));

		void Draw(Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer);
		void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer);

	private:
		CommandListParams m_Params;
		Framebuffer* m_TargetFramebuffer = nullptr;

		nvrhi::CommandListHandle m_CommandListHandle;
	};

}
