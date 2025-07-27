#include "depch.h"
#include "DingoEngine/Core/CacheManager.h"

namespace Dingo
{

	void CacheManager::Initialize()
	{
		if (!std::filesystem::exists(GetCacheBaseDirectory()))
		{
			std::filesystem::create_directories(GetCacheBaseDirectory());
		}

		// Initialize cache manager resources here
	}

	void CacheManager::Shutdown()
	{
		// Clean up cache manager resources here
	}

	std::filesystem::path CacheManager::GetCacheBaseDirectory()
	{
		return std::filesystem::current_path() / ".cache";
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
