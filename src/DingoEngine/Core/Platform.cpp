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

	std::filesystem::path GetExecutablePath()
	{
#ifdef DE_PLATFORM_WINDOWS
		std::vector<wchar_t> buffer(MAX_PATH);
		for (;;)
		{
			const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (length == 0)
			{
				DE_CORE_ERROR("GetModuleFileNameW failed; falling back to the current directory.");
				return std::filesystem::current_path();
			}

			// A full buffer means the path was truncated.
			if (length < buffer.size())
				return std::filesystem::path(std::wstring(buffer.data(), length));

			buffer.resize(buffer.size() * 2);
		}
#else
		std::error_code errorCode;
		std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", errorCode);
		if (errorCode)
		{
			DE_CORE_ERROR("Failed to read /proc/self/exe: {}. Falling back to the current directory.", errorCode.message());
			return std::filesystem::current_path();
		}

		return path;
#endif
	}

	std::filesystem::path GetExecutableDirectory()
	{
		return GetExecutablePath().parent_path();
	}

	std::optional<std::filesystem::path> FindDirectoryUpward(const std::filesystem::path& name, const std::filesystem::path& start, uint32_t maxLevels)
	{
		std::error_code errorCode;
		std::filesystem::path directory = std::filesystem::absolute(start, errorCode);
		if (errorCode)
			directory = start;

		for (uint32_t level = 0;; ++level)
		{
			const std::filesystem::path candidate = directory / name;
			if (std::filesystem::is_directory(candidate, errorCode))
				return candidate;

			// The root is its own parent.
			const std::filesystem::path parent = directory.parent_path();
			if (level == maxLevels || parent.empty() || parent == directory)
				return std::nullopt;

			directory = parent;
		}
	}

} // namespace Dingo::Platform
