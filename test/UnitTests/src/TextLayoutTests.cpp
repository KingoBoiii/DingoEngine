#include <doctest/doctest.h>

#include "DingoEngine/Graphics/TextLayout.h"

#include <string>
#include <vector>

using namespace Dingo;
namespace TextLayout = Dingo::Internal::TextLayout;

namespace
{
	// Every glyph advances 1 pen unit and a space 0.5; '#' has no glyph and no '?' fallback.
	struct FixedSource
	{
		using Glyph = uint32_t;

		Glyph FindGlyph(uint32_t character) const { return character == '#' ? 0 : character; }
		double GlyphAdvance(Glyph, uint32_t, uint32_t) const { return 1.0; }
		double SpaceAdvance(uint32_t) const { return 0.5; }
	};

	struct Laid
	{
		std::vector<std::string> Lines;
		std::vector<double> Widths;
	};

	Laid Lay(const std::string& text, double maxWidth, double size = 1.0, double kerning = 0.0)
	{
		Laid laid;
		std::string current;
		TextLayout::Params params;
		params.MaxWidth = maxWidth;
		params.Size = size;
		params.Kerning = kerning;
		TextLayout::Walk(FixedSource{}, text, params,
			[&](uint32_t glyph, uint32_t, double, uint32_t) { current.push_back(static_cast<char>(glyph)); },
			[&](uint32_t, double width)
			{
				laid.Lines.push_back(current);
				laid.Widths.push_back(width);
				current.clear();
			});
		return laid;
	}

	TextLayout::Measurement Measure(const std::string& text, double maxWidth, double size = 1.0)
	{
		TextLayout::Params params;
		params.MaxWidth = maxWidth;
		params.Size = size;
		return TextLayout::Measure(FixedSource{}, text, params);
	}
}

TEST_SUITE("TextLayout")
{
	TEST_CASE("unwrapped width and lines")
	{
		CHECK(Measure("abc", 0.0).Width == doctest::Approx(3.0));
		CHECK(Measure("abc", 0.0).LineCount == 1);
		CHECK(Measure("a b", 0.0).Width == doctest::Approx(2.5));
		CHECK(Measure("ab\ncde", 0.0).Width == doctest::Approx(3.0));
		CHECK(Measure("ab\ncde", 0.0).LineCount == 2);
		CHECK(Measure("ab\n", 0.0).LineCount == 2);
		CHECK(Measure("", 0.0).LineCount == 0);
		CHECK(Measure("", 4.0).LineCount == 0);
	}

	TEST_CASE("kerning is added after every character")
	{
		const Laid laid = Lay("abc", 0.0, 1.0, 0.25);
		CHECK(laid.Widths[0] == doctest::Approx(3.75));
	}

	TEST_CASE("a character with no glyph is skipped, not a line end")
	{
		const Laid laid = Lay("a#b", 0.0);
		REQUIRE(laid.Lines.size() == 1);
		CHECK(laid.Lines[0] == "ab");
		CHECK(laid.Widths[0] == doctest::Approx(2.0));
	}

	TEST_CASE("wraps at the space before the word that overflows")
	{
		const Laid laid = Lay("aa bb cc", 5.0);
		REQUIRE(laid.Lines.size() == 2);
		CHECK(laid.Lines[0] == "aabb");
		CHECK(laid.Widths[0] == doctest::Approx(4.5));
		CHECK(laid.Lines[1] == "cc");
	}

	TEST_CASE("a word wider than the line breaks between characters")
	{
		const Laid laid = Lay("abcdefg", 3.0);
		REQUIRE(laid.Lines.size() == 3);
		CHECK(laid.Lines[0] == "abc");
		CHECK(laid.Lines[1] == "def");
		CHECK(laid.Lines[2] == "g");
	}

	TEST_CASE("a line always takes at least one character")
	{
		const Laid laid = Lay("abc", 0.5);
		REQUIRE(laid.Lines.size() == 3);
		CHECK(laid.Widths[0] == doctest::Approx(1.0));
	}

	TEST_CASE("a run of spaces at a break is swallowed")
	{
		const Laid laid = Lay("aa   bb", 3.0);
		REQUIRE(laid.Lines.size() == 2);
		CHECK(laid.Lines[0] == "aa");
		CHECK(laid.Widths[0] == doctest::Approx(2.0));
		CHECK(laid.Lines[1] == "bb");
	}

	TEST_CASE("a break on spaces before a newline adds no empty line")
	{
		const Laid laid = Lay("aa \nbb", 2.0);
		REQUIRE(laid.Lines.size() == 2);
		CHECK(laid.Lines[0] == "aa");
		CHECK(laid.Lines[1] == "bb");
	}

	TEST_CASE("trailing spaces past the width add no empty line")
	{
		CHECK(Lay("aa   ", 2.0).Lines.size() == 1);
	}

	TEST_CASE("newlines still break while wrapping")
	{
		const Laid laid = Lay("aa bb\ncc", 3.0);
		REQUIRE(laid.Lines.size() == 3);
		CHECK(laid.Lines[0] == "aa");
		CHECK(laid.Lines[1] == "bb");
		CHECK(laid.Lines[2] == "cc");
	}

	TEST_CASE("MaxWidth is in the caller's units")
	{
		const TextLayout::Measurement measurement = Measure("abcd", 1.0, 0.5);
		CHECK(measurement.LineCount == 2);
		CHECK(measurement.Width * 0.5 <= 1.0);
	}

	TEST_CASE("wrapped lines fit the width")
	{
		const std::string text = "The quick brown fox jumps over the lazy dog";
		const double unwrapped = Measure(text, 0.0).Width;
		for (double maxWidth = 1.0; maxWidth < unwrapped + 2.0; maxWidth += 0.75)
		{
			CAPTURE(maxWidth);
			const TextLayout::Measurement measurement = Measure(text, maxWidth);
			CHECK(measurement.Width <= maxWidth);
			CHECK(measurement.LineCount >= 1);
		}
		CHECK(Measure(text, unwrapped).LineCount == 1);
		CHECK(Measure(text, unwrapped).Width == doctest::Approx(unwrapped));
	}
}
