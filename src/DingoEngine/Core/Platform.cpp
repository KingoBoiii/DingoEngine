#include "depch.h"
#include "DingoEngine/Core/Platform.h"

#include <cstdlib>

namespace Dingo::Platform
{

	std::filesystem::path GetUserDataDir(const std::string& appName)
	{
		DE_CORE_ASSERT(!appName.empty(), "App name cannot be empty.");

		std::filesystem::path directory;

#ifdef DE_PLATFORM_WINDOWS
		if (const char* localAppData = std::getenv("LOCALAPPDATA"))
		{
			directory = std::filesystem::path(localAppData) / appName;
		}
		else
		{
			DE_CORE_ERROR("LOCALAPPDATA environment variable not set. Falling back to current directory for user data.");
			directory = std::filesystem::current_path() / appName;
		}
#else
		if (const char* xdgDataHome = std::getenv("XDG_DATA_HOME"))
		{
			directory = std::filesystem::path(xdgDataHome) / appName;
		}
		else if (const char* home = std::getenv("HOME"))
		{
			directory = std::filesystem::path(home) / ".local" / "share" / appName;
		}
		else
		{
			DE_CORE_ERROR("Neither XDG_DATA_HOME nor HOME environment variables are set. Falling back to current directory for user data.");
			directory = std::filesystem::current_path() / appName;
		}
#endif

		if (!std::filesystem::exists(directory))
		{
			std::error_code errorCode;
			std::filesystem::create_directories(directory, errorCode);
			if (errorCode)
			{
				DE_CORE_ERROR("Failed to create user data directory '{}': {}", directory.string(), errorCode.message());
			}
		}

		return directory;
	}

} // namespace Dingo::Platform
