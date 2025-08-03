#pragma once
#include "DingoEngine/Graphics/Sampler.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiSampler : public Sampler
	{
	public:
		NvrhiSampler(const SamplerParams& params)
			: Sampler(params)
		{}
		virtual ~NvrhiSampler() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

	private:
		nvrhi::SamplerHandle m_Handle;

		friend class NvrhiRenderPass; // Allow RenderPass to access private members
		friend class NvrhiPipeline; // Allow Pipeline to access private members
	};

}
