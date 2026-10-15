#include "depch.h"
#include "DingoEngine/Core/Platform.h"
#include "DingoEngine/Core/PathText.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string_view>

namespace Dingo::Platform
{

	namespace
	{
		bool IsDeviceName(std::string_view stem)
		{
			std::string upper(stem);
			for (char& c : upper)
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			while (!upper.empty() && upper.back() == ' ')
				upper.pop_back();
			if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" || upper == "CONIN$" || upper == "CONOUT$")
				return true;
			if (!upper.starts_with("COM") && !upper.starts_with("LPT"))
				return false;
			const std::string_view number = std::string_view(upper).substr(3);
			return (number.size() == 1 && number[0] >= '0' && number[0] <= '9')
				|| number == "\xC2\xB9" || number == "\xC2\xB2" || number == "\xC2\xB3";
		}

		// The same folder on every platform, and one Windows can create: a name made on Linux
		// with a ':' would otherwise lose the log, the dumps and the saves only on Windows.
		std::string FolderName(const std::string& appName)
		{
			std::string name = appName;
			for (char& c : name)
			{
				const unsigned char byte = static_cast<unsigned char>(c);
				if (byte < 0x20 || byte == 0x7F || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos)
					c = '_';
			}
			while (!name.empty() && (name.back() == '.' || name.back() == ' '))
				name.pop_back();
			if (name.empty())
				return "_";
			if (IsDeviceName(std::string_view(name).substr(0, name.find('.'))))
				name.insert(name.begin(), '_');
			return name;
		}
	}

	std::filesystem::path GetUserDataDir(const std::string& appName)
	{
		DE_CORE_ASSERT(!appName.empty(), "App name cannot be empty.");

		const std::filesystem::path folder = Internal::PathFromUtf8(FolderName(appName));
		std::filesystem::path directory;

#ifdef DE_PLATFORM_WINDOWS
		if (const wchar_t* localAppData = _wgetenv(L"LOCALAPPDATA"))
		{
			directory = std::filesystem::path(localAppData) / folder;
		}
		else
		{
			DE_CORE_ERROR("LOCALAPPDATA environment variable not set. Falling back to current directory for user data.");
			directory = std::filesystem::current_path() / folder;
		}
#else
		if (const char* xdgDataHome = std::getenv("XDG_DATA_HOME"))
		{
			directory = std::filesystem::path(xdgDataHome) / folder;
		}
		else if (const char* home = std::getenv("HOME"))
		{
			directory = std::filesystem::path(home) / ".local" / "share" / folder;
		}
		else
		{
			DE_CORE_ERROR("Neither XDG_DATA_HOME nor HOME environment variables are set. Falling back to current directory for user data.");
			directory = std::filesystem::current_path() / folder;
		}
#endif

		// A name the old rule kept as is ("Studio/Game" nested, a ':' on Linux) made a folder a
		// shipped game's players already have: it keeps its logs and saves there.
		const std::filesystem::path original = Internal::PathFromUtf8(appName);
		// Windows drops trailing dots and spaces, so "..." would name the parent folder itself.
		const bool aliased =appName.back() == '.' || appName.back() == ' ';
		const bool relative = !aliased && !original.has_root_path()
			&& std::none_of(original.begin(), original.end(), [](const std::filesystem::path& part) { return part == ".."; });
		const std::filesystem::path legacy = directory.parent_path() / original;
		std::error_code error;
		if (relative && legacy != directory && !std::filesystem::exists(directory, error) && std::filesystem::is_directory(legacy, error))
			directory = legacy;

		if (!std::filesystem::exists(directory))
		{
			std::error_code errorCode;
			std::filesystem::create_directories(directory, errorCode);
			if (errorCode)
			{
				DE_CORE_ERROR("Failed to create user data directory '{}': {}", Internal::PathToUtf8(directory), errorCode.message());
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
