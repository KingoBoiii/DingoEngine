#include "depch.h"
#include "DingoEngine/Asset/Pack/Crc32.h"

#include <array>

namespace Dingo::Internal
{

	namespace
	{

		constexpr std::array<uint32_t, 256> MakeTable()
		{
			std::array<uint32_t, 256> table{};
			for (uint32_t i = 0; i < 256; ++i)
			{
				uint32_t value = i;
				for (int bit = 0; bit < 8; ++bit)
					value = (value & 1) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
				table[i] = value;
			}
			return table;
		}

		constexpr std::array<uint32_t, 256> s_Table = MakeTable();

	}

	uint32_t Crc32(const void* data, size_t size, uint32_t crc)
	{
		const uint8_t* bytes = static_cast<const uint8_t*>(data);
		crc = ~crc;
		for (size_t i = 0; i < size; ++i)
			crc = s_Table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
		return ~crc;
	}

}
