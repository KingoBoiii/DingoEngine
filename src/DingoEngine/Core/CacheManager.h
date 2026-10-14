#pragma once
#include <filesystem>

namespace Dingo::Internal
{

	// The shader and font caches: beside the executable, or under the user data folder when that
	// is read-only.
	class CacheManager
	{
	public:
		static void Initialize();
		static void Shutdown();

		static std::filesystem::path GetCacheBaseDirectory();
		static std::filesystem::path GetCacheDirectory(const std::filesystem::path& subCacheDirectory);
	};

}
