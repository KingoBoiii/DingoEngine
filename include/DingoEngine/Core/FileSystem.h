#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
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
		// A PNG, JPEG, BMP, TGA, ... file. flipVertically puts the file's last row first, as the
		// engine's textures want; forceRGBA gives 4 channels whatever the file has. nullopt
		// (logged) on failure.
		static std::optional<ImageData> ReadImage(const std::filesystem::path& filepath, bool flipVertically = true, bool forceRGBA = false);
		// Writes width x height pixels of `channels` bytes each (1 to 4), first row first, as a PNG,
		// or a BMP, TGA or JPEG by the path's extension. flipVertically writes the last row first, the
		// inverse of ReadImage's. The directory must exist. False (logged) on failure.
		static bool WriteImage(const std::filesystem::path& filepath, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels, bool flipVertically = false);
	};

}
