#pragma once
#include "Framebuffer.h"
#include "Pipeline.h"
#include "Buffer.h"
#include "UniformBuffer.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine
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
		void Draw(Pipeline* pipeline, VertexBuffer* vertexBuffer);
		void DrawIndexed(Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer);
		void DrawIndexed(Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer, UniformBuffer* uniformBuffer);

	private:
		CommandListParams m_Params;
		Framebuffer* m_TargetFramebuffer = nullptr;

		nvrhi::CommandListHandle m_CommandListHandle;
	};

}
