#include "depch.h"
#include "DingoEngine/Graphics/Sampler.h"

#include "NVRHI/NvrhiSampler.h"

namespace Dingo
{

	Sampler* Sampler::Create(const SamplerParams& params)
	{
		Sampler* sampler = new NvrhiSampler(params);
		sampler->Initialize();
		return sampler;
	}

}
