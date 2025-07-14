#include "depch.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "NVRHI/NvrhiPipeline.h"
#include "NVRHI/NvrhiGraphicsBuffer.h"

namespace Dingo
{

	Pipeline* Pipeline::Create(Shader* shader, Framebuffer* framebuffer)
	{
		PipelineParams params = PipelineParams()
			.SetShader(shader)
			.SetFramebuffer(framebuffer);

		return Create(params);
	}

	Pipeline* Pipeline::Create(const PipelineParams& params)
	{
		return new NvrhiPipeline(params);
	}

}
