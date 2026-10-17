#pragma once
#include <cstdint>

namespace Dingo
{

	// FNV-1a over explicit little-endian fields. Every state hash goes through it, never over struct memory, so padding
	// and platform do not matter.
	class Fnv1a
	{
	public:
		void Byte(uint8_t value)
		{
			m_Hash ^= value;
			m_Hash *= 0x100000001B3ull;
		}

		void U16(uint16_t value) { Byte(static_cast<uint8_t>(value)); Byte(static_cast<uint8_t>(value >> 8)); }
		void U32(uint32_t value) { U16(static_cast<uint16_t>(value)); U16(static_cast<uint16_t>(value >> 16)); }
		void U64(uint64_t value) { U32(static_cast<uint32_t>(value)); U32(static_cast<uint32_t>(value >> 32)); }
		void I32(int32_t value) { U32(static_cast<uint32_t>(value)); }

		uint64_t Value() const { return m_Hash; }

	private:
		uint64_t m_Hash = 0xCBF29CE484222325ull;
	};

}
