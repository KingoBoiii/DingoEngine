#include "Tests/TestChecks.h"

#include <format>

#ifdef DE_PLATFORM_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

namespace Dingo
{

	std::filesystem::path TestTempDirectory(std::string_view name)
	{
#ifdef DE_PLATFORM_WINDOWS
		const int processId = _getpid();
#else
		const int processId = static_cast<int>(getpid());
#endif
		return std::filesystem::temp_directory_path() / FileSystem::PathFromUtf8(std::format("{}-{}", name, processId));
	}

}
