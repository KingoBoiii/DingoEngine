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

		CommandListParams& SetTargetSwapChain(bool targetSwapChain)
		{
			TargetSwapChain = targetSwapChain;
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
