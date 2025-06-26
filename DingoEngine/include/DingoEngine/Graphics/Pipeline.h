#pragma once
#pragma once
#include "Shader.h"
#include "Framebuffer.h"

namespace DingoEngine
{

	class Pipeline
	{
	public:
		static Pipeline* Create(Shader* shader, Framebuffer* framebuffer);

	public:
		Pipeline(Shader* shader, Framebuffer* framebuffer);
		~Pipeline() = default;

	public:
		void Initialize();
		void Destroy();

	private:
		Shader* m_Shader;
		Framebuffer* m_Framebuffer;
		nvrhi::InputLayoutHandle m_InputLayoutHandle;
		nvrhi::GraphicsPipelineHandle m_GraphicsPipelineHandle;

		friend class CommandList;
	};

}
