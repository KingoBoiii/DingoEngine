#pragma once
#include "Framebuffer.h"
#include "Pipeline.h"
#include "Buffer.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class CommandList
	{
	public:
		static CommandList* Create();

	public:
		CommandList() = default;
		~CommandList() = default;

	public:
		void Initialize();
		void Destroy();

		void Begin();
		void End();

		void Clear(Framebuffer* framebuffer);

		void Draw(Framebuffer* framebuffer, Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1);
		void Draw(Framebuffer* framebuffer, Pipeline* pipeline, VertexBuffer* vertexBuffer);
		void DrawIndexed(Framebuffer* framebuffer, Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer);

	private:
		nvrhi::CommandListHandle m_CommandListHandle;
	};

}
