#pragma once
#include "DingoEngine/Graphics/Pipeline.h"

#include "NvrhiShader.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiPipeline : public Pipeline
	{
	public:
		NvrhiPipeline(const PipelineParams& params)
			: Pipeline(params)
		{}
		virtual ~NvrhiPipeline() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

	private:
		void CreateInputLayout(NvrhiShader* nvrhiShader);
		void CreateBindingSet(nvrhi::BindingLayoutHandle bindingLayoutHandle);

	private:
		nvrhi::InputLayoutHandle m_InputLayoutHandle;
		nvrhi::BindingLayoutHandle m_BindingLayoutHandle;
		nvrhi::BindingSetHandle m_BindingSetHandle;
		nvrhi::GraphicsPipelineHandle m_GraphicsPipelineHandle;

		friend class CommandList; // Allow CommandList to access private members
	};

}
