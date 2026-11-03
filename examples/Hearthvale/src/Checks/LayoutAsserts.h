#pragma once
#include "Ui/ScreenLayouts.h"
#include "Ui/UiLayout.h"

#include <set>
#include <string>
#include <vector>

namespace Dingo
{

	// The overflow, overlap and uniqueness tests every screen's pure layout has to pass, shared by the suites that build layouts.
	namespace LayoutAsserts
	{
		using namespace UiMetrics;

		struct Resolution
		{
			float W;
			float H;
		};

		inline constexpr Resolution k_Resolutions[] = {
			{ 1280.0f, 720.0f }, { 3840.0f, 2160.0f }, { 1920.0f, 1080.0f }, { 1280.0f, 1024.0f },
			{ 3440.0f, 1440.0f }, { 1024.0f, 768.0f }, { 2560.0f, 1080.0f } };

		inline const UiMeasure k_Estimate = [](const std::string& text, float size, float maxWidth) { return EstimateText(text, size, maxWidth); };

		inline bool WidgetsFit(const ScreenLayout& layout)
		{
			for (const ScreenWidget& widget : layout.Widgets)
			{
				if (!TextFitsBox(VisibleLabel(widget.Label), widget.TextSize, widget.Rect.W - 2.0f * BUTTON_PAD_X, widget.Rect.H - 2.0f * BUTTON_PAD_Y, false, k_Estimate))
					return false;
			}
			return true;
		}

		inline bool TextsFit(const ScreenLayout& layout)
		{
			for (const ScreenText& text : layout.Texts)
			{
				if (!TextFitsBox(text.Text, text.Size, text.Box.W, text.Box.H, text.Wrap, k_Estimate))
					return false;
			}
			return true;
		}

		inline bool EverythingInside(const ScreenLayout& layout)
		{
			std::vector<UiRect> rects = layout.WidgetRects();
			for (const ScreenText& text : layout.Texts)
				rects.push_back(text.Box);
			return AllInside(rects, layout.Panel);
		}

		inline bool NoWidgetsOverlap(const ScreenLayout& layout)
		{
			const std::vector<UiRect> rects = layout.WidgetRects();
			for (size_t a = 0; a < rects.size(); ++a)
			{
				for (size_t b = a + 1; b < rects.size(); ++b)
				{
					if (rects[a].Overlaps(rects[b]))
						return false;
				}
			}
			return true;
		}

		inline bool LabelsUnique(const ScreenLayout& layout)
		{
			std::set<std::string> seen;
			for (const ScreenWidget& widget : layout.Widgets)
			{
				if (!seen.insert(widget.Label).second)
					return false;
			}
			return true;
		}

		inline bool FitsEveryResolution(const ScreenLayout& layout)
		{
			for (const Resolution& resolution : k_Resolutions)
			{
				const glm::vec2 design = UiDesignSize(resolution.W, resolution.H);
				if (layout.Panel.W > design.x || layout.Panel.H > design.y)
					return false;
			}
			return true;
		}

	}

}
