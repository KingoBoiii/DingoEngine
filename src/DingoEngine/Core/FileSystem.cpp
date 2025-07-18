#include "depch.h"
#include "DingoEngine/Core/FileSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Dingo
{

	std::string FileSystem::ReadTextFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		DE_CORE_ASSERT(file.is_open(), "Failed to open file: " + path.string());

		size_t fileSize = file.tellg();
		std::string buffer(fileSize, '\0');

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}
	
	const uint8_t* FileSystem::ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically, bool forceRGBA)
	{
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
