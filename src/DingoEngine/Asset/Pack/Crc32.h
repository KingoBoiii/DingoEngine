#pragma once

#include <cstddef>
#include <cstdint>

namespace Dingo::Internal
{

	// CRC-32 (IEEE 802.3, the zlib and PNG one). Pass the previous result to continue a running CRC.
	uint32_t Crc32(const void* data, size_t size, uint32_t crc = 0);

}
