#include "depch.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "NVRHI/NvrhiPipeline.h"
#include "NVRHI/NvrhiGraphicsBuffer.h"

namespace Dingo
{

	Pipeline* Pipeline::Create(const PipelineParams& params)
	{
		NvrhiPipeline* pipeline = new NvrhiPipeline(params);
		pipeline->Initialize();
		return pipeline;
	}

}
