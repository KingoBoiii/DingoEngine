#include "depch.h"
#include "DingoEngine/Asset/Pack/ChaCha20.h"

#include <algorithm>

namespace Dingo::Internal::ChaCha20
{

	namespace
	{

		constexpr uint32_t RotateLeft(uint32_t value, int bits)
		{
			return (value << bits) | (value >> (32 - bits));
		}

		constexpr void QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d)
		{
			a += b; d ^= a; d = RotateLeft(d, 16);
			c += d; b ^= c; b = RotateLeft(b, 12);
			a += b; d ^= a; d = RotateLeft(d, 8);
			c += d; b ^= c; b = RotateLeft(b, 7);
		}

		uint32_t LoadLittleEndian(const uint8_t* bytes)
		{
			return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8)
				| (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
		}

		void StoreLittleEndian(uint32_t value, uint8_t* bytes)
		{
			bytes[0] = static_cast<uint8_t>(value);
			bytes[1] = static_cast<uint8_t>(value >> 8);
			bytes[2] = static_cast<uint8_t>(value >> 16);
			bytes[3] = static_cast<uint8_t>(value >> 24);
		}

	}

	void Block(const Key& key, uint32_t counter, const Nonce& nonce, uint8_t out[64])
	{
		uint32_t state[16] = { 0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u };
		for (int i = 0; i < 8; ++i)
			state[4 + i] = LoadLittleEndian(key.data() + 4 * i);
		state[12] = counter;
		for (int i = 0; i < 3; ++i)
			state[13 + i] = LoadLittleEndian(nonce.data() + 4 * i);

		uint32_t working[16];
		std::copy(std::begin(state), std::end(state), working);
		for (int round = 0; round < 10; ++round)
		{
			QuarterRound(working[0], working[4], working[8], working[12]);
			QuarterRound(working[1], working[5], working[9], working[13]);
			QuarterRound(working[2], working[6], working[10], working[14]);
			QuarterRound(working[3], working[7], working[11], working[15]);
			QuarterRound(working[0], working[5], working[10], working[15]);
			QuarterRound(working[1], working[6], working[11], working[12]);
			QuarterRound(working[2], working[7], working[8], working[13]);
			QuarterRound(working[3], working[4], working[9], working[14]);
		}

		for (int i = 0; i < 16; ++i)
			StoreLittleEndian(working[i] + state[i], out + 4 * i);
	}

	void Apply(const Key& key, const Nonce& nonce, uint32_t counter, uint8_t* data, size_t size)
	{
		uint8_t keystream[64];
		for (size_t offset = 0; offset < size; offset += 64, ++counter)
		{
			Block(key, counter, nonce, keystream);
			const size_t count = (std::min)(size - offset, size_t(64));
			for (size_t i = 0; i < count; ++i)
				data[offset + i] ^= keystream[i];
		}
	}

}
