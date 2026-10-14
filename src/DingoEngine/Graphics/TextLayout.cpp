#include "depch.h"
#include "DingoEngine/Graphics/TextLayout.h"
#include "DingoEngine/Graphics/Font.h"

#include "MSDFData.h"

namespace Dingo::Internal::TextLayout
{

	FontGlyphSource::FontGlyphSource(const Font& font)
	{
		const MSDFData* data = font.GetMSDFData();
		if (!data)
			return;

		Geometry = &data->FontGeometry;
		const msdfgen::FontMetrics& metrics = Geometry->getMetrics();
		Scale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		LineHeight = Scale * metrics.lineHeight;
		Ascender = Scale * metrics.ascenderY;
		Descender = Scale * metrics.descenderY;
		Space = Geometry->getGlyph(' ');
	}

	FontGlyphSource::Glyph FontGlyphSource::FindGlyph(uint32_t character) const
	{
		if (!Geometry)
			return nullptr;

		Glyph glyph = Geometry->getGlyph(character);
		if (!glyph)
			glyph = Geometry->getGlyph('?');
		return glyph;
	}

	double FontGlyphSource::GlyphAdvance(Glyph glyph, uint32_t character, uint32_t next) const
	{
		// getAdvance's out-param is replaced only when both glyphs of the pair exist, so seed it
		// with the glyph's own advance: the last character and a missing pair keep it.
		double advance = glyph->getAdvance();
		if (next != 0)
			Geometry->getAdvance(advance, character, next);
		return Scale * advance;
	}

	double FontGlyphSource::SpaceAdvance(uint32_t next) const
	{
		if (!Space)
			return 0.0;

		double advance = Space->getAdvance();
		if (next != 0)
			Geometry->getAdvance(advance, ' ', next);
		return Scale * advance;
	}

}
