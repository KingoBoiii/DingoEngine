#include "depch.h"
#include "DingoEngine/Asset/Pack/Deflate.h"

#include <stb_image.h>

#include <cstdlib>
#include <limits>

// Defined in FileSystem.cpp's stb_image_write implementation, which doesn't declare it.
extern "C" unsigned char* stbi_zlib_compress(unsigned char* data, int dataLength, int* outLength, int quality);

namespace Dingo::Internal
{

	namespace
	{

		constexpr size_t k_MaxStbSize = static_cast<size_t>((std::numeric_limits<int>::max)());
		constexpr int k_DeflateQuality = 8;

	}

	std::optional<std::vector<uint8_t>> Deflate(const uint8_t* data, size_t size)
	{
		if (size > k_MaxStbSize)
			return std::nullopt;

		int length = 0;
		unsigned char* compressed = stbi_zlib_compress(const_cast<unsigned char*>(data), static_cast<int>(size), &length, k_DeflateQuality);
		if (!compressed)
			return std::nullopt;

		std::vector<uint8_t> result(compressed, compressed + length);
		std::free(compressed);
		return result;
	}

	std::optional<std::vector<uint8_t>> Inflate(const uint8_t* data, size_t size, size_t rawSize)
	{
		if (size > k_MaxStbSize || rawSize > k_MaxStbSize)
			return std::nullopt;

		std::vector<uint8_t> result(rawSize);
		const int written = stbi_zlib_decode_buffer(reinterpret_cast<char*>(result.data()), static_cast<int>(rawSize),
			reinterpret_cast<const char*>(data), static_cast<int>(size));
		if (written < 0 || static_cast<size_t>(written) != rawSize)
			return std::nullopt;
		return result;
	}

}
