#include "depch.h"
#include "DingoEngine/Graphics/Sampler.h"

#include "NVRHI/NvrhiSampler.h"

namespace Dingo
{

	Sampler* Sampler::Create(const SamplerParams& params)
	{
		return new NvrhiSampler(params);
	}

}
