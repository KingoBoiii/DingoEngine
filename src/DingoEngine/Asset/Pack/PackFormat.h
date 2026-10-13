#pragma once

#include "DingoEngine/Asset/Pack/ChaCha20.h"

#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace Dingo::Internal::Pack
{

	static_assert(std::endian::native == std::endian::little, "the .dpak format is read and written as little-endian structs");

	// Layout: Header, the entry blobs, then the TOC. The TOC is deflated, then encrypted like an
	// entry, so file names exist only inside it. An entry's bytes are deflated (unless that saves
	// under 5%), then encrypted. Every nonce is the salt's last 12 bytes with the first four XORed
	// with the entry's index; the TOC and the key check take indices of their own.
	inline constexpr char k_Magic[4] = { 'D', 'P', 'A', 'K' };
	inline constexpr uint16_t k_Version = 1;

	enum HeaderFlags : uint16_t
	{
		HeaderFlag_Encrypted = 1
	};

	enum EntryFlags : uint32_t
	{
		EntryFlag_Deflated = 1,
		EntryFlag_Encrypted = 2
	};

#pragma pack(push, 1)
	struct Header
	{
		char Magic[4];
		uint16_t Version;
		uint16_t Flags;
		uint8_t Salt[16];
		uint64_t TocOffset;
		uint64_t TocPackedSize;
		uint64_t TocRawSize;
		uint32_t TocCrc;
		uint32_t EntryCount;
		// The first keystream bytes of the key-check nonce, so a wrong key fails as such rather than
		// as a corrupt TOC. Zero when not encrypted.
		uint8_t KeyCheck[8];
	};

	// Followed in the TOC by NameLength bytes of UTF-8 name.
	struct TocEntry
	{
		uint64_t PathHash;
		uint64_t Offset;
		uint64_t PackedSize;
		uint64_t RawSize;
		uint32_t Crc;
		uint32_t Flags;
		uint16_t NameLength;
	};
#pragma pack(pop)

	static_assert(sizeof(Header) == 64);
	static_assert(sizeof(TocEntry) == 42);

	// stb's zlib calls take int sizes.
	inline constexpr uint64_t k_MaxEntrySize = static_cast<uint64_t>((std::numeric_limits<int>::max)());
	inline constexpr uint32_t k_MaxEntries = 0xFFFF0000u;
	inline constexpr uint32_t k_TocNonceIndex = 0xFFFFFFFFu;
	inline constexpr uint32_t k_KeyCheckNonceIndex = 0xFFFFFFFEu;

	inline ChaCha20::Nonce MakeNonce(const uint8_t (&salt)[16], uint32_t index)
	{
		ChaCha20::Nonce nonce;
		for (size_t i = 0; i < nonce.size(); ++i)
			nonce[i] = salt[4 + i];
		for (size_t i = 0; i < 4; ++i)
			nonce[i] ^= static_cast<uint8_t>(index >> (8 * i));
		return nonce;
	}

	inline std::string FoldCase(std::string_view name)
	{
		std::string folded(name);
		for (char& c : folded)
		{
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		}
		return folded;
	}

	// FNV-1a 64 of the case-folded name.
	inline uint64_t HashPath(std::string_view name)
	{
		uint64_t hash = 14695981039346656037ull;
		for (char c : name)
		{
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
			hash ^= static_cast<uint8_t>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

}
