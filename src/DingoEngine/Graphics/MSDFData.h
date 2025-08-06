#pragma once

#include <vector>

#define MSDFGEN_PUBLIC
#include <msdf-atlas-gen.h>

namespace Dingo
{

	struct MSDFData
	{
		std::vector<msdf_atlas::GlyphGeometry> Glyphs;
		msdf_atlas::FontGeometry FontGeometry;
	};

}
