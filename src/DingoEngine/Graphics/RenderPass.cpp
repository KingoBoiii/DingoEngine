#include "depch.h"
#include "DingoEngine/Graphics/RenderPass.h"

#include "NVRHI/NvrhiRenderPass.h"

namespace Dingo
{
	RenderPass* RenderPass::Create(const RenderPassParams& params)
	{
		DE_CORE_ASSERT(params.Pipeline, "RenderPass must have a valid Pipeline set before creation.");

		return new NvrhiRenderPass(params);
	}

} // namespace Dingo

