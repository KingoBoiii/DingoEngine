#include "depch.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/GraphicsDevice.h"

#include "NVRHI/NvrhiRenderPass.h"

namespace Dingo
{
	RenderPass* RenderPass::Create(const RenderPassParams& params)
	{
		if (!Internal::RequireGraphicsDevice("RenderPass::Create"))
			return nullptr;

		DE_CORE_ASSERT(params.Pipeline, "RenderPass must have a valid Pipeline set before creation.");

		RenderPass* renderPass = new NvrhiRenderPass(params);
		renderPass->Initialize();
		return renderPass;
	}

} // namespace Dingo

