#include "depch.h"
#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Core/FileReaders.h"
#include "DingoEngine/Core/PathText.h"
#include "DingoEngine/Core/VirtualFileSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <vector>

namespace Dingo
{

	std::optional<std::vector<uint8_t>> FileSystem::ReadFileBytes(const std::filesystem::path& filepath)
	{
		return VirtualFileSystem::ReadBytes(filepath);
	}

	std::string Internal::ReadTextFile(const std::filesystem::path& path)
	{
		std::optional<std::vector<uint8_t>> bytes = VirtualFileSystem::ReadBytes(path);
		if (!bytes)
		{
			DE_CORE_ERROR("Failed to open file: {}", Internal::PathToUtf8(path));
			return "";
		}
		return std::string(bytes->begin(), bytes->end());
	}

	namespace
	{

		// stbi_set_flip_vertically_on_load writes stb-global state and decodes run on both the main
		// thread and the asset loader thread; serialize the whole decode.
		std::mutex s_StbMutex;

	}

	const uint8_t* Internal::ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically, bool forceRGBA)
	{
		std::optional<std::vector<uint8_t>> bytes = VirtualFileSystem::ReadBytes(filepath);
		if (!bytes || bytes->empty())
		{
			DE_CORE_ERROR("Failed to load image: {}", Internal::PathToUtf8(filepath));
			return nullptr;
		}
		const uint8_t* pixels = ReadImageFromMemory(bytes->data(), bytes->size(), width, height, channels, flipVertically, forceRGBA);
		if (!pixels)
			DE_CORE_ERROR("Failed to load image: {}", Internal::PathToUtf8(filepath));
		return pixels;
	}

	const uint8_t* Internal::ReadImageFromMemory(const void* bytes, size_t size, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically, bool forceRGBA)
	{
		if (!bytes || size == 0 || size > static_cast<size_t>((std::numeric_limits<int>::max)()))
		{
			DE_CORE_ERROR("Failed to load image from memory: {} bytes", size);
			return nullptr;
		}

		std::scoped_lock lock(s_StbMutex);

		stbi_set_flip_vertically_on_load(flipVertically);

		int32_t widthTemp, heightTemp, channelsTemp;
		uint8_t* data = stbi_load_from_memory(static_cast<const stbi_uc*>(bytes), static_cast<int>(size), &widthTemp, &heightTemp, &channelsTemp,
			forceRGBA ? STBI_rgb_alpha : STBI_default);
		if (!data)
		{
			DE_CORE_ERROR("Failed to load image from memory: {}", stbi_failure_reason());
			return nullptr;
		}

		*width = static_cast<uint32_t>(widthTemp);
		*height = static_cast<uint32_t>(heightTemp);
		*channels = forceRGBA ? 4 : static_cast<uint32_t>(channelsTemp);
		return data;
	}

	void Internal::FreeImage(const uint8_t* data)
	{
		stbi_image_free(const_cast<uint8_t*>(data));
	}

	std::optional<ImageData> FileSystem::ReadImage(const std::filesystem::path& filepath, bool flipVertically, bool forceRGBA)
	{
		ImageData image;
		const uint8_t* data = Internal::ReadImage(filepath, &image.Width, &image.Height, &image.Channels, flipVertically, forceRGBA);
		if (!data)
			return std::nullopt;
		if (forceRGBA)
			image.Channels = 4;
		image.Pixels.assign(data, data + static_cast<size_t>(image.Width) * image.Height * image.Channels);
		Internal::FreeImage(data);
		return image;
	}

	bool FileSystem::WriteImage(const std::filesystem::path& filepath, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels, bool flipVertically)
	{
		if (!pixels || width == 0 || height == 0 || channels < 1 || channels > 4)
		{
			DE_CORE_ERROR("WriteImage: nothing to write to {} ({}x{}, {} channels).", Internal::PathToUtf8(filepath), width, height, channels);
			return false;
		}

		// Flipped here rather than through stbi_flip_vertically_on_write, which is global state.
		const size_t rowBytes = static_cast<size_t>(width) * channels;
		std::vector<uint8_t> flipped;
		if (flipVertically)
		{
			flipped.resize(rowBytes * height);
			for (uint32_t row = 0; row < height; ++row)
				std::memcpy(flipped.data() + row * rowBytes, pixels + (height - 1 - row) * rowBytes, rowBytes);
			pixels = flipped.data();
		}

		// stb encodes into memory and the stream writes the file, which takes a path stb's fopen
		// can't open (any non-ASCII name on Windows).
		std::vector<uint8_t> encoded;
		auto append = [](void* context, void* data, int size)
		{
			std::vector<uint8_t>& bytes = *static_cast<std::vector<uint8_t>*>(context);
			bytes.insert(bytes.end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
		};

		std::string extension = Internal::PathToUtf8(filepath.extension());
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		const int w = static_cast<int>(width), h = static_cast<int>(height), c = static_cast<int>(channels);
		int encodedOk = 0;
		if (extension == ".bmp")
			encodedOk = stbi_write_bmp_to_func(append, &encoded, w, h, c, pixels);
		else if (extension == ".tga")
			encodedOk = stbi_write_tga_to_func(append, &encoded, w, h, c, pixels);
		else if (extension == ".jpg" || extension == ".jpeg")
			encodedOk = stbi_write_jpg_to_func(append, &encoded, w, h, c, pixels, 90);
		else
			encodedOk = stbi_write_png_to_func(append, &encoded, w, h, c, pixels, static_cast<int>(rowBytes));

		std::ofstream file;
		if (encodedOk)
			file.open(filepath, std::ios::binary | std::ios::trunc);
		if (!encodedOk || !file.is_open())
		{
			DE_CORE_ERROR("WriteImage: failed to write {}", Internal::PathToUtf8(filepath));
			return false;
		}

		file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
		file.close();
		if (file.fail())
		{
			DE_CORE_ERROR("WriteImage: failed to write {}", Internal::PathToUtf8(filepath));
			return false;
		}
		return true;
	}

	std::string FileSystem::PathToUtf8(const std::filesystem::path& path)
	{
		return Internal::PathToUtf8(path);
	}

	std::string FileSystem::PathToGenericUtf8(const std::filesystem::path& path)
	{
		return Internal::PathToGenericUtf8(path);
	}

	std::filesystem::path FileSystem::PathFromUtf8(std::string_view text)
	{
		return Internal::PathFromUtf8(text);
	}

}
