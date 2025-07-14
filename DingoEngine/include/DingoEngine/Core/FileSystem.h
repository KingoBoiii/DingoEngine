#pragma once
#include <filesystem>

namespace Dingo
{

	class FileSystem
	{
	public:
		static const uint8_t* ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically = true, bool forceRGBA = false);

		static std::string GetFileName(const std::filesystem::path& filepath);
	};

}
