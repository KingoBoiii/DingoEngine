#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	class FileSystem
	{
	public:
		// The reads go through the VirtualFileSystem, so they find packed files too.
		static std::optional<std::vector<uint8_t>> ReadFileBytes(const std::filesystem::path& filepath);
		static std::string ReadTextFile(const std::filesystem::path& filepath);
		static const uint8_t* ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically = true, bool forceRGBA = false);
		// ReadImage for an encoded image (PNG, JPEG, ...) already in memory.
		static const uint8_t* ReadImageFromMemory(const void* bytes, size_t size, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically = true, bool forceRGBA = false);
		// Frees a buffer returned by ReadImage or ReadImageFromMemory. Safe on nullptr.
		static void FreeImage(const uint8_t* data);
		// Writes width x height pixels of `channels` bytes each (1 to 4), first row first, as a PNG,
		// or a BMP, TGA or JPEG by the path's extension. flipVertically writes the last row first, the
		// inverse of ReadImage's. The directory must exist. False (logged) on failure.
		static bool WriteImage(const std::filesystem::path& filepath, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels, bool flipVertically = false);

		static std::string GetFileName(const std::filesystem::path& filepath);

		// Paths as text, UTF-8 on every platform. On Windows path::string() and path(std::string)
		// go through the ANSI code page, and string() throws on a character it can't map.
		static std::string PathToUtf8(const std::filesystem::path& path);
		// PathToUtf8 with '/' separators.
		static std::string PathToGenericUtf8(const std::filesystem::path& path);
		static std::filesystem::path PathFromUtf8(std::string_view text);
	};

}
