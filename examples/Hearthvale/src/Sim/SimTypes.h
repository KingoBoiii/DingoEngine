#pragma once
#include <cstdint>

namespace Dingo
{

	enum class Season : uint8_t { Spring, Summer, Fall, Winter };
	enum class Weather : uint8_t { Clear, Rain, Snow };

	const char* ToString(Season season);
	const char* ToString(Weather weather);

}
