#pragma once
#include <filesystem>

namespace Dingo
{

	class CacheManager
	{
	public:
		static void Initialize();
		static void Shutdown();

		static std::filesystem::path GetCacheBaseDirectory();
		static std::filesystem::path GetCacheDirectory(const std::filesystem::path& subCacheDirectory);
	};

}
