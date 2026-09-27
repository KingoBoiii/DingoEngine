#include "depch.h"
#include "DingoEngine/Core/UUID.h"

#include <random>

namespace Dingo
{

	static std::random_device s_RandomDevice;
	static std::mt19937_64 s_Engine(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformDistribution(1, UINT64_MAX);

	UUID UUID::Generate()
	{
		return UUID(s_UniformDistribution(s_Engine));
	}

}
