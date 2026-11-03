#include "Checks/ProcessMemory.h"

#if defined(DE_PLATFORM_WINDOWS)
	#include <Windows.h>
	#include <psapi.h>
#else
	#include <fstream>
	#include <string>
#endif

namespace Dingo
{

	uint64_t GetProcessPrivateBytes()
	{
#if defined(DE_PLATFORM_WINDOWS)
		PROCESS_MEMORY_COUNTERS_EX counters{};
		if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
			return 0;
		return counters.PrivateUsage;
#else
		std::ifstream status("/proc/self/status");
		std::string key;
		uint64_t kilobytes = 0;
		while (status >> key)
		{
			if (key == "RssAnon:")
			{
				status >> kilobytes;
				return kilobytes * 1024;
			}
			status.ignore(256, '\n');
		}
		return 0;
#endif
	}

}
