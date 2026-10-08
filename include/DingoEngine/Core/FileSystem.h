#pragma once
#include <filesystem>

namespace Dingo
{

	class FileSystem
	{
	public:
		static std::string ReadTextFile(const std::filesystem::path& filepath);
		static const uint8_t* ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically = true, bool forceRGBA = false);
		// Frees a buffer returned by ReadImage. Safe on nullptr.
		static void FreeImage(const uint8_t* data);
		// Writes width x height pixels of `channels` bytes each (1 to 4), first row first, as a PNG,
		// or a BMP, TGA or JPEG by the path's extension. flipVertically writes the last row first, the
		// inverse of ReadImage's. The directory must exist. False (logged) on failure.
		static bool WriteImage(const std::filesystem::path& filepath, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels, bool flipVertically = false);

		static std::string GetFileName(const std::filesystem::path& filepath);
	};

}
