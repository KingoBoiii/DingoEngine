#include "depch.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "NVRHI/NvrhiPipeline.h"
#include "NVRHI/NvrhiGraphicsBuffer.h"

namespace Dingo
{

	Pipeline* Pipeline::Create(const PipelineParams& params)
	{
		if (!params.Shader || !params.Framebuffer)
		{
			DE_CORE_ERROR("Pipeline '{}': no {}, so it is not created", params.DebugName, params.Shader ? "framebuffer" : "shader");
			return nullptr;
		}

		NvrhiPipeline* pipeline = new NvrhiPipeline(params);
		pipeline->Initialize();
		return pipeline;
	}

}
