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

	// Decoded pixels, first row first, Channels bytes a pixel.
	struct ImageData
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Channels = 0;
		std::vector<uint8_t> Pixels;
	};

	class FileSystem
	{
	public:
		// Every read goes through the VirtualFileSystem, so it finds packed files too. nullopt when the
		// file is in no mounted package and not on disk.
		static std::optional<std::vector<uint8_t>> ReadFileBytes(const std::filesystem::path& filepath);
		// A PNG, JPEG, BMP, TGA, ... file. flipVertically puts the file's last row first, as the
		// engine's textures want; forceRGBA gives 4 channels whatever the file has. nullopt
		// (logged) on failure.
		static std::optional<ImageData> ReadImage(const std::filesystem::path& filepath, bool flipVertically = true, bool forceRGBA = false);
		// Writes width x height pixels of `channels` bytes each (1 to 4), first row first, as a PNG,
		// or a BMP, TGA or JPEG by the path's extension. flipVertically writes the last row first, the
		// inverse of ReadImage's. The directory must exist. False (logged) on failure.
		static bool WriteImage(const std::filesystem::path& filepath, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels, bool flipVertically = false);

		// Paths as text, UTF-8 on every platform. On Windows path::string() and path(std::string)
		// go through the ANSI code page, and string() throws on a character it can't map.
		static std::string PathToUtf8(const std::filesystem::path& path);
		// PathToUtf8 with '/' separators.
		static std::string PathToGenericUtf8(const std::filesystem::path& path);
		static std::filesystem::path PathFromUtf8(std::string_view text);
	};

}
