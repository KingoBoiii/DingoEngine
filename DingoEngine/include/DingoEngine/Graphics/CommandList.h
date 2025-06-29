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

		void Begin(Framebuffer* framebuffer, Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer);
		void Begin(Framebuffer* framebuffer, Pipeline* pipeline, VertexBuffer* vertexBuffer);
		void Begin(Framebuffer* framebuffer, Pipeline* pipeline);
		void End();

		void Clear(Framebuffer* framebuffer);
		void Draw();
		void DrawIndexed(IndexBuffer* indexBuffer);

	private:
		nvrhi::CommandListHandle m_CommandListHandle;
	};

}
