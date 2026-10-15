#pragma once
#include "Defs/BuildDefs.h"
#include "Defs/ItemDefs.h"

#include <glm/glm.hpp>

#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	struct UiRect
	{
		float X = 0.0f;
		float Y = 0.0f;
		float W = 0.0f;
		float H = 0.0f;

		float Right() const { return X + W; }
		float Bottom() const { return Y + H; }
		float CenterX() const { return X + W * 0.5f; }
		float CenterY() const { return Y + H * 0.5f; }
		bool Inside(const UiRect& outer) const;
		bool Overlaps(const UiRect& other) const;
	};

	struct UiText
	{
		float Width = 0.0f;
		float Height = 0.0f;
		int Lines = 0;
	};

	// Same shape as Font::MeasureText, so --check can pass a conservative estimate and the windowed suite the font.
	using UiMeasure = std::function<UiText(const std::string& text, float size, float maxWidth)>;

	// DingoUI's ScaleWithScreen(1920 x 1080, match 0.5): the factor from design pixels to screen pixels.
	float UiScaleFactor(float screenW, float screenH);
	glm::vec2 UiDesignSize(float screenW, float screenH);

	// Over-estimates Inter-Bold: a fixed advance per character and one size-and-a-quarter per line. maxWidth <= 0 never wraps.
	UiText EstimateText(const std::string& text, float size, float maxWidth);

	// Whether `text` laid out at `size` inside a box of boxW x boxH stays inside it; maxWidth is boxW when wrapping, else 0.
	bool TextFitsBox(const std::string& text, float size, float boxW, float boxH, bool wrap, const UiMeasure& measure);

	// Greedy word wrap: every line measures at most maxWidth unless a single word is wider, which is broken between
	// characters. A newline in the text starts a new line; blank lines stay.
	std::vector<std::string> WrapLines(const std::string& text, float size, float maxWidth, const UiMeasure& measure);
	// WrapLines grouped into pages of at most maxLines lines; each page joins its lines with newlines. Never empty:
	// an empty text is one empty page.
	std::vector<std::string> PaginateText(const std::string& text, float size, float maxWidth, int maxLines, const UiMeasure& measure);

	// Breaks a name longer than maxChars at the space nearest its middle; a name with no space stays whole.
	std::string SplitName(const std::string& name, int maxChars);

	// "400 coins\n30 wood, 10 stone": coins on the first line, materials on the second, "free" when there is no cost.
	std::string CostLines(const BuildCost& cost);
	// The same on one line.
	std::string CostInline(const BuildCost& cost);

	// A placeholder colour per item until real icons arrive: by kind, hue per crop.
	glm::vec4 ItemSwatch(ItemId item);

	// Whether `rects` all stay inside `outer`.
	bool AllInside(const std::vector<UiRect>& rects, const UiRect& outer);

}
