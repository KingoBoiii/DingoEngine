#include "depch.h"
#include "DingoEngine/Core/UUID.h"

#include <random>

namespace Dingo
{

	UUID UUID::Generate()
	{
		thread_local std::mt19937_64 engine = []
		{
			std::random_device device;
			std::seed_seq seed{ device(), device(), device(), device() };
			return std::mt19937_64(seed);
		}();
		std::uniform_int_distribution<uint64_t> distribution(1, UINT64_MAX);
		return UUID(distribution(engine));
	}

}
