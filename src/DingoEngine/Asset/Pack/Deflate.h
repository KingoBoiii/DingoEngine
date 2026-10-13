#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Dingo::Internal
{

	// zlib streams through stb (stb_image_write's compressor, stb_image's inflater). stb's inflater
	// doesn't check the Adler-32, so callers check their own CRC.
	std::optional<std::vector<uint8_t>> Deflate(const uint8_t* data, size_t size);
	// `rawSize` must be the exact inflated size.
	std::optional<std::vector<uint8_t>> Inflate(const uint8_t* data, size_t size, size_t rawSize);

}
