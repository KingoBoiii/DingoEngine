#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "Enums.h"

namespace DingoEngine
{

	struct PipelineParams
	{
		Shader* Shader = nullptr;
		Framebuffer* Framebuffer = nullptr;
		FillMode FillMode = FillMode::Solid;
		CullMode CullMode = CullMode::Back;

		PipelineParams& SetShader(DingoEngine::Shader* shader)
		{
			Shader = shader;
			return *this;
		}

		PipelineParams& SetFramebuffer(DingoEngine::Framebuffer* framebuffer)
		{
			Framebuffer = framebuffer;
			return *this;
		}

		PipelineParams& SetFillMode(DingoEngine::FillMode fillMode)
		{
			FillMode = fillMode;
			return *this;
		}

		PipelineParams& SetCullMode(DingoEngine::CullMode cullMode)
		{
			CullMode = cullMode;
			return *this;
		}
	};

	class Pipeline
	{
	public:
		static Pipeline* Create(Shader* shader, Framebuffer* framebuffer);
		static Pipeline* Create(const PipelineParams& params);

	public:
		Pipeline(const PipelineParams& params);
		~Pipeline() = default;

	public:
		void Initialize();
		void Destroy();

		const PipelineParams& GetParams() const { return m_Params; }

	private:
		PipelineParams m_Params;
		nvrhi::InputLayoutHandle m_InputLayoutHandle;
		nvrhi::GraphicsPipelineHandle m_GraphicsPipelineHandle;

		friend class CommandList;
	};

}
