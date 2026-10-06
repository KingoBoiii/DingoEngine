#pragma once

// Engine-internal: shared by Renderer2D::DrawText and Font::GetStringWidth, which must walk a
// string identically or centered text drifts off its measured width.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Dingo::Internal
{

	// Returns the codepoint starting at text[index] and advances index past it. A byte that does
	// not begin a well-formed sequence (stray continuation, truncated, overlong, surrogate, past
	// U+10FFFF) is returned as its Latin-1 codepoint and consumes one byte, so strings that were
	// never UTF-8 still draw their Latin-1 glyphs instead of vanishing.
	inline uint32_t DecodeUtf8(std::string_view text, size_t& index)
	{
		const uint8_t lead = static_cast<uint8_t>(text[index]);

		size_t length = 0;
		uint32_t codepoint = 0;
		uint32_t minimum = 0;
		if ((lead & 0xE0) == 0xC0)
		{
			length = 2;
			codepoint = lead & 0x1F;
			minimum = 0x80;
		}
		else if ((lead & 0xF0) == 0xE0)
		{
			length = 3;
			codepoint = lead & 0x0F;
			minimum = 0x800;
		}
		else if ((lead & 0xF8) == 0xF0)
		{
			length = 4;
			codepoint = lead & 0x07;
			minimum = 0x10000;
		}

		if (length == 0 || text.size() - index < length)
		{
			index++;
			return lead;
		}

		for (size_t i = 1; i < length; i++)
		{
			const uint8_t continuation = static_cast<uint8_t>(text[index + i]);
			if ((continuation & 0xC0) != 0x80)
			{
				index++;
				return lead;
			}
			codepoint = (codepoint << 6) | (continuation & 0x3F);
		}

		if (codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
		{
			index++;
			return lead;
		}

		index += length;
		return codepoint;
	}

}
