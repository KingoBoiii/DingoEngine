#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Dingo::Internal::ChaCha20
{

	// RFC 8439's ChaCha20: a 256-bit key, a 96-bit nonce and a 32-bit block counter.
	using Key = std::array<uint8_t, 32>;
	using Nonce = std::array<uint8_t, 12>;

	void Block(const Key& key, uint32_t counter, const Nonce& nonce, uint8_t out[64]);
	// XORs the keystream from block `counter` over `data`; the same call encrypts and decrypts.
	void Apply(const Key& key, const Nonce& nonce, uint32_t counter, uint8_t* data, size_t size);

}
