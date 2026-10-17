#include "depch.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/GraphicsDevice.h"

#include "NVRHI/NvrhiSampler.h"

#include <atomic>

namespace Dingo
{

	Sampler* Sampler::Create(const SamplerParams& params)
	{
		if (!Internal::RequireGraphicsDevice("Sampler::Create"))
			return nullptr;

		Sampler* sampler = new NvrhiSampler(params);
		sampler->Initialize();
		return sampler;
	}

	uint64_t Sampler::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

}
