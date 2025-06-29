#pragma once
#include "Pipeline.h"
#include "Framebuffer.h"

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

		void Begin(Framebuffer* framebuffer, Pipeline* pipeline);
		void End();

		void Clear(Framebuffer* framebuffer);
		void Draw();

	private:
		nvrhi::CommandListHandle m_CommandListHandle;
	};

}
