#pragma once
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Texture.h"

namespace Dingo
{

	struct RenderPassParams
	{
		Pipeline* Pipeline = nullptr;

		RenderPassParams& SetPipeline(Dingo::Pipeline* pipeline)
		{
			Pipeline = pipeline;
			return *this;
		}
	};

	class RenderPass
	{
	public:
		static RenderPass* Create(const RenderPassParams& params);

	public:
		RenderPass(const RenderPassParams& params)
			: m_Params(params)
		{}
		virtual ~RenderPass() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual void SetUniformBuffer(uint32_t slot, GraphicsBuffer* buffer) = 0;

		virtual void SetTexture(uint32_t slot, Texture* texture) = 0;

		virtual void Bake() = 0;

	protected:
		RenderPassParams m_Params;
	};

}
