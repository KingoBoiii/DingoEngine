#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace Dingo::Internal
{

	std::string ReadTextFile(const std::filesystem::path& filepath);
	// Owned by the caller until FreeImage.
	const uint8_t* ReadImage(const std::filesystem::path& filepath, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically = true, bool forceRGBA = false);
	// ReadImage for an encoded image (PNG, JPEG, ...) already in memory.
	const uint8_t* ReadImageFromMemory(const void* bytes, size_t size, uint32_t* width, uint32_t* height, uint32_t* channels, bool flipVertically = true, bool forceRGBA = false);
	// Frees a buffer returned by ReadImage or ReadImageFromMemory. Safe on nullptr.
	void FreeImage(const uint8_t* data);

}
