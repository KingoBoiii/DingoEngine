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
		nvrhi::BindingSetHandle m_BindingSetHandle;
		nvrhi::GraphicsPipelineHandle m_GraphicsPipelineHandle;
		// Shader generation this PSO was built from; a mismatch at bind time means the
		// shader was hot-reloaded and the pipeline is lazily rebuilt. Same for the
		// params texture baked into the binding set, if there is one.
		uint32_t m_BuiltShaderGeneration = 0;
		uint32_t m_BuiltTextureGeneration = 0;
		// Latched on the first build, while the params framebuffer is still guaranteed
		// live: marks a target that a resize frees, so a rebuild must re-resolve it.
		bool m_TargetsSwapChain = false;

		friend class NvrhiCommandList; // Allow CommandList to access private members
		friend class NvrhiRenderPass; // Allow RenderPass to access private members
	};

}
