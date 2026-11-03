#pragma once
#include <cstdint>

namespace Dingo
{

	// Memory this process owns and no other shares, in bytes, or 0 when the platform can't say: private bytes on
	// Windows, anonymous resident memory on Linux. A soak watches it across autoplay days.
	uint64_t GetProcessPrivateBytes();

}
