#include "depch.h"
#include "DingoEngine/Core/CacheManager.h"
#include "DingoEngine/Core/Platform.h"

namespace Dingo
{

	namespace
	{

		std::filesystem::path s_BaseDirectory;

		bool IsWritableDirectory(const std::filesystem::path& directory)
		{
			std::error_code errorCode;
			std::filesystem::create_directories(directory, errorCode);
			if (errorCode)
				return false;

			const std::filesystem::path probe = directory / ".write-probe";
			{
				std::ofstream stream(probe, std::ios::binary | std::ios::trunc);
				if (!stream)
					return false;
			}
			std::filesystem::remove(probe, errorCode);
			return true;
		}

		// Beside the executable, not the working directory: launching the same build from
		// elsewhere would otherwise recompile every shader and atlas. A read-only install
		// falls back to per-user storage so it still caches.
		std::filesystem::path ResolveBaseDirectory()
		{
			const std::filesystem::path besideExecutable = Platform::GetExecutableDirectory() / ".cache";
			if (IsWritableDirectory(besideExecutable))
				return besideExecutable;

			const std::filesystem::path perUser = Platform::GetUserDataDir("DingoEngine") / "cache" / Platform::GetExecutablePath().stem();
			DE_CORE_WARN("Cache directory '{}' is not writable; caching in '{}' instead", besideExecutable.string(), perUser.string());
			return perUser;
		}

	}

	void CacheManager::Initialize()
	{
		s_BaseDirectory = ResolveBaseDirectory();

		std::error_code errorCode;
		std::filesystem::create_directories(s_BaseDirectory, errorCode);
		if (errorCode)
			DE_CORE_ERROR("Failed to create cache directory '{}': {}", s_BaseDirectory.string(), errorCode.message());
	}

	void CacheManager::Shutdown()
	{
		s_BaseDirectory.clear();
	}

	std::filesystem::path CacheManager::GetCacheBaseDirectory()
	{
		if (s_BaseDirectory.empty())
			s_BaseDirectory = ResolveBaseDirectory();
		return s_BaseDirectory;
	}

	std::filesystem::path CacheManager::GetCacheDirectory(const std::filesystem::path& subCacheDirectory)
	{
		DE_CORE_ASSERT(!subCacheDirectory.empty(), "Sub-cache directory cannot be empty.");

		std::filesystem::path directory = GetCacheBaseDirectory() / subCacheDirectory;
		if (!std::filesystem::exists(directory))
		{
			std::filesystem::create_directories(directory);
		}

		return directory;
	}

} // namespace Dingo
