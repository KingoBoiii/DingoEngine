#include <doctest/doctest.h>

#include "DingoEngine/Graphics/Utf8.h"

#include <string>
#include <vector>

using Dingo::Internal::DecodeUtf8;

namespace
{
	std::vector<uint32_t> Decode(std::string_view text)
	{
		std::vector<uint32_t> codepoints;
		for (size_t index = 0; index < text.size();)
			codepoints.push_back(DecodeUtf8(text, index));
		return codepoints;
	}

	using Codepoints = std::vector<uint32_t>;
}

TEST_SUITE("Utf8")
{
	TEST_CASE("well-formed sequences of every length")
	{
		CHECK(Decode("Az") == Codepoints{ 'A', 'z' });
		CHECK(Decode("\xC3\xA9") == Codepoints{ 0xE9 });
		CHECK(Decode("\xE2\x82\xAC") == Codepoints{ 0x20AC });
		CHECK(Decode("\xF0\x9F\x98\x80") == Codepoints{ 0x1F600 });
		CHECK(Decode("a\xE2\x80\x94" "b") == Codepoints{ 'a', 0x2014, 'b' });
	}

	TEST_CASE("bytes that aren't UTF-8 read as Latin-1, one byte each")
	{
		CHECK(Decode("\xE9t\xE9") == Codepoints{ 0xE9, 't', 0xE9 });
		CHECK(Decode("\x80") == Codepoints{ 0x80 });
		CHECK(Decode("\xC3") == Codepoints{ 0xC3 });
		CHECK(Decode("\xE2\x82") == Codepoints{ 0xE2, 0x82 });
		CHECK(Decode("\xC3" "A") == Codepoints{ 0xC3, 'A' });
	}

	TEST_CASE("overlong forms, surrogates and codepoints past U+10FFFF are rejected")
	{
		CHECK(Decode("\xC0\x80") == Codepoints{ 0xC0, 0x80 });
		CHECK(Decode("\xE0\x80\xAF") == Codepoints{ 0xE0, 0x80, 0xAF });
		CHECK(Decode("\xED\xA0\x80") == Codepoints{ 0xED, 0xA0, 0x80 });
		CHECK(Decode("\xF4\x90\x80\x80") == Codepoints{ 0xF4, 0x90, 0x80, 0x80 });
		CHECK(Decode("\xF4\x8F\xBF\xBF") == Codepoints{ 0x10FFFF });
	}
}
