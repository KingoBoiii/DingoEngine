#pragma once

// Engine-internal: the one walk over a string that Font's metrics, Renderer2D::DrawText and the
// DingoUI batcher share, so a measured width, a wrap break and a drawn glyph never disagree.

#include "DingoEngine/Graphics/Utf8.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace msdf_atlas
{
	class GlyphGeometry;
	class FontGeometry;
}

namespace Dingo
{
	class Font;
}

namespace Dingo::Internal::TextLayout
{

	struct Params
	{
		// Pen units (1 = the font's ascender-to-descender height), added after every character.
		double Kerning = 0.0;
		// Multiplies pen units into the caller's units; MaxWidth is compared in the caller's units.
		double Size = 1.0;
		// 0 or less: no wrap. Lines break at spaces; a word wider than this breaks between
		// characters, and a line always takes at least one character.
		double MaxWidth = 0.0;
	};

	// The font's glyphs in pen units. Advances take the original codepoint so a '?' fallback is
	// never kerned as if it were a '?'.
	struct FontGlyphSource
	{
		using Glyph = const msdf_atlas::GlyphGeometry*;

		explicit FontGlyphSource(const Font& font);

		Glyph FindGlyph(uint32_t character) const;
		double GlyphAdvance(Glyph glyph, uint32_t character, uint32_t next) const;
		double SpaceAdvance(uint32_t next) const;

		const msdf_atlas::FontGeometry* Geometry = nullptr;
		Glyph Space = nullptr;
		double Scale = 1.0;
		double LineHeight = 0.0;
		double Ascender = 0.0;
		double Descender = 0.0;
	};

	namespace Detail
	{

		inline uint32_t Peek(std::string_view text, size_t index)
		{
			return index < text.size() ? DecodeUtf8(text, index) : 0;
		}

		// Past the spaces a soft break swallows, and a '\n' right after them, so a wrap that lands
		// on a line end doesn't add an empty line.
		inline size_t SkipSpaces(std::string_view text, size_t index)
		{
			while (index < text.size() && text[index] == ' ')
				++index;
			if (index < text.size() && text[index] == '\n')
				++index;
			return index;
		}

		// Advance of one character; false when it draws and measures nothing (no glyph and no '?').
		template<typename Source>
		bool Advance(const Source& source, uint32_t character, uint32_t next, double kerning, typename Source::Glyph& outGlyph, double& outAdvance)
		{
			outGlyph = typename Source::Glyph{};
			if (character == ' ')
			{
				outAdvance = source.SpaceAdvance(next) + kerning;
				return true;
			}
			if (character == '\t')
			{
				outAdvance = 4.0 * (source.SpaceAdvance(0) + kerning);
				return true;
			}

			outGlyph = source.FindGlyph(character);
			if (!outGlyph)
				return false;

			outAdvance = source.GlyphAdvance(outGlyph, character, next) + kerning;
			return true;
		}

		template<typename Source, typename GlyphFn>
		double EmitRange(const Source& source, std::string_view text, size_t begin, size_t end, uint32_t line, double kerning, GlyphFn& onGlyph)
		{
			double x = 0.0;
			size_t index = begin;
			while (index < end)
			{
				const uint32_t character = DecodeUtf8(text, index);
				const uint32_t next = Peek(text, index);

				typename Source::Glyph glyph{};
				double advance = 0.0;
				if (!Advance(source, character, next, kerning, glyph, advance))
					continue;

				if (glyph)
					onGlyph(glyph, character, x, line);

				x += advance;
			}
			return x;
		}

		struct LineBreak
		{
			size_t End = 0;
			size_t Resume = 0;
			bool Last = false;
		};

		template<typename Source>
		LineBreak MeasureWrappedLine(const Source& source, std::string_view text, size_t begin, const Params& params)
		{
			double x = 0.0;
			bool placedAny = false;
			bool placedWord = false;
			bool previousWasSpace = false;
			// The latest run of spaces after a word: where a soft break ends the line.
			size_t spaceRunStart = std::string_view::npos;

			size_t index = begin;
			while (index < text.size())
			{
				const size_t characterStart = index;
				const uint32_t character = DecodeUtf8(text, index);
				if (character == '\n')
					return { characterStart, index, false };

				const uint32_t next = Peek(text, index);
				typename Source::Glyph glyph{};
				double advance = 0.0;
				if (!Advance(source, character, next, params.Kerning, glyph, advance))
					continue;

				const bool isSpace = character == ' ';
				if (isSpace && !previousWasSpace && placedWord)
					spaceRunStart = characterStart;

				if (placedAny && (x + advance) * params.Size > params.MaxWidth)
				{
					const auto softBreak = [&](size_t end, size_t spaces)
					{
						const size_t resume = SkipSpaces(text, spaces);
						return LineBreak{ end, resume, resume >= text.size() };
					};
					if (isSpace)
						return softBreak(spaceRunStart != std::string_view::npos && previousWasSpace ? spaceRunStart : characterStart, characterStart);
					if (spaceRunStart != std::string_view::npos)
						return softBreak(spaceRunStart, spaceRunStart);
					return { characterStart, characterStart, false };
				}

				x += advance;
				placedAny = true;
				placedWord = placedWord || !isSpace;
				previousWasSpace = isSpace;
			}
			return { text.size(), text.size(), true };
		}

	}

	// Calls onGlyph(glyph, codepoint, penX, line) for every drawn glyph and onLine(line, width)
	// after each line's glyphs, '\n' and wrap breaks alike; a string ending in '\n' ends on an
	// empty line. Pen units: multiply by Params::Size for the caller's units.
	template<typename Source, typename GlyphFn, typename LineFn>
	void Walk(const Source& source, std::string_view text, const Params& params, GlyphFn&& onGlyph, LineFn&& onLine)
	{
		const bool wrap = params.MaxWidth > 0.0;
		size_t begin = 0;
		uint32_t line = 0;
		while (true)
		{
			Detail::LineBreak lineBreak;
			if (wrap)
			{
				lineBreak = Detail::MeasureWrappedLine(source, text, begin, params);
			}
			else
			{
				const size_t newline = text.find('\n', begin);
				lineBreak.Last = newline == std::string_view::npos;
				lineBreak.End = lineBreak.Last ? text.size() : newline;
				lineBreak.Resume = lineBreak.End + 1;
			}

			const double width = Detail::EmitRange(source, text, begin, lineBreak.End, line, params.Kerning, onGlyph);
			onLine(line, width);

			if (lineBreak.Last)
				break;

			begin = lineBreak.Resume;
			++line;
		}
	}

	struct Measurement
	{
		double Width = 0.0; // pen units, widest line
		uint32_t LineCount = 0;
	};

	template<typename Source>
	Measurement Measure(const Source& source, std::string_view text, const Params& params)
	{
		Measurement measurement;
		if (text.empty())
			return measurement;

		Walk(source, text, params,
			[](typename Source::Glyph, uint32_t, double, uint32_t) {},
			[&](uint32_t, double width)
			{
				if (width > measurement.Width)
					measurement.Width = width;
				++measurement.LineCount;
			});
		return measurement;
	}

}
