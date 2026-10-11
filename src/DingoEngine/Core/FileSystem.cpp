#include "depch.h"
#include "DingoEngine/Core/FileSystem.h"

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

	std::string FileSystem::ReadTextFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			DE_CORE_ERROR("Failed to open file: {}", path.string());
			return "";
		}

		size_t fileSize = file.tellg();
		std::string buffer(fileSize, '\0');

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}
	
	namespace
	{

		// stbi_set_flip_vertically_on_load writes stb-global state and decodes run on both the main
		// thread and the asset loader thread; serialize the whole decode.
		std::mutex s_StbMutex;

	}

	const uint8_t* FileSystem::ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically, bool forceRGBA)
	{
		std::scoped_lock lock(s_StbMutex);

		stbi_set_flip_vertically_on_load(flipVertically);

		int32_t widthTemp, heightTemp, channelsTemp;
		uint8_t* data = stbi_load(filepath.string().c_str(), &widthTemp, &heightTemp, &channelsTemp, forceRGBA ? STBI_rgb_alpha : STBI_default);
		if (!data)
		{
			DE_CORE_ERROR("Failed to load image: {}", filepath.string());
			return nullptr;
		}

		*width = static_cast<uint32_t>(widthTemp);
		*height = static_cast<uint32_t>(heightTemp);
		*channels = static_cast<uint32_t>(channelsTemp);

		if (forceRGBA)
		{
			*channels = 4; // Ensure channels is set to 4 if we forced RGBA
		}

		return data;
	}

	const uint8_t* FileSystem::ReadImageFromMemory(const void* bytes, size_t size, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically, bool forceRGBA)
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

	void FileSystem::FreeImage(const uint8_t* data)
	{
		stbi_image_free(const_cast<uint8_t*>(data));
	}

	bool FileSystem::WriteImage(const std::filesystem::path& filepath, uint32_t width, uint32_t height, uint32_t channels, const uint8_t* pixels, bool flipVertically)
	{
		if (!pixels || width == 0 || height == 0 || channels < 1 || channels > 4)
		{
			DE_CORE_ERROR("WriteImage: nothing to write to {} ({}x{}, {} channels).", filepath.string(), width, height, channels);
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

		std::string extension = filepath.extension().string();
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
			DE_CORE_ERROR("WriteImage: failed to write {}", filepath.string());
			return false;
		}

		file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
		file.close();
		if (file.fail())
		{
			DE_CORE_ERROR("WriteImage: failed to write {}", filepath.string());
			return false;
		}
		return true;
	}

	std::string FileSystem::GetFileName(const std::filesystem::path& filepath)
	{
		const std::string& filepathString = filepath.string();

		// Extract name from filepath
		auto lastSlash = filepathString.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepathString.rfind('.');
		auto count = lastDot == std::string::npos ? filepathString.size() - lastSlash : lastDot - lastSlash;
		return filepathString.substr(lastSlash, count);
	}

}
