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

		virtual void SetTexture(uint32_t slot, Texture* texture, uint32_t arrayElement = 0) = 0;

		virtual void Bake() = 0;

		const RenderPassParams& GetParams() const { return m_Params; }
		Pipeline* GetPipeline() const { return m_Params.Pipeline; }
		Framebuffer* GetTargetFramebuffer() const { return m_Params.Pipeline->GetTargetFramebuffer(); }

	protected:
		RenderPassParams m_Params;
	};

}
