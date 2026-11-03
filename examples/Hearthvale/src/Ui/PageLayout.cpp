#include "Ui/PageLayout.h"

#include <algorithm>

namespace Dingo
{

	using namespace UiMetrics;

	PageCell TextCell(std::string text, float width, float height, float size, PageTone tone, bool wrap)
	{
		PageCell cell;
		cell.Kind = CellKind::Text;
		cell.Label = std::move(text);
		cell.Width = width;
		cell.Height = height;
		cell.TextSize = size;
		cell.Tone = tone;
		cell.Wrap = wrap;
		return cell;
	}

	PageCell ButtonCell(std::string label, float width, float height, float size, PageAction action, int arg, bool enabled)
	{
		PageCell cell;
		cell.Kind = CellKind::Button;
		cell.Label = std::move(label);
		cell.Width = width;
		cell.Height = height;
		cell.TextSize = size;
		cell.Action = action;
		cell.Arg = arg;
		cell.Enabled = enabled;
		return cell;
	}

	PageCell SliderCell(std::string label, float width, float height, float value, float min, float max, float step, PageAction action, int arg)
	{
		PageCell cell;
		cell.Kind = CellKind::Slider;
		cell.Label = std::move(label);
		cell.Width = width;
		cell.Height = height;
		cell.Action = action;
		cell.Arg = arg;
		cell.Value = value;
		cell.Min = min;
		cell.Max = max;
		cell.Step = step;
		return cell;
	}

	PageRow MakeRow(std::vector<PageCell> cells, float indent)
	{
		PageRow row;
		row.Indent = indent;
		for (const PageCell& cell : cells)
			row.Height = (std::max)(row.Height, cell.Height);
		row.Cells = std::move(cells);
		return row;
	}

	float RowWidth(const std::vector<float>& widths)
	{
		float total = 0.0f;
		for (const float width : widths)
			total += width;
		return widths.empty() ? 0.0f : total + static_cast<float>(widths.size() - 1) * GAP;
	}

	int FindWidgetCell(const PageSpec& page, const std::string& label, int* rowOut)
	{
		for (size_t r = 0; r < page.Rows.size(); ++r)
		{
			for (size_t c = 0; c < page.Rows[r].Cells.size(); ++c)
			{
				const PageCell& cell = page.Rows[r].Cells[c];
				if (cell.Kind != CellKind::Text && cell.Label == label)
				{
					if (rowOut)
						*rowOut = static_cast<int>(r);
					return static_cast<int>(c);
				}
			}
		}
		return -1;
	}

	ScreenLayout LayoutPage(const PageSpec& page)
	{
		ScreenLayout layout;
		layout.Id = page.Id;
		layout.DefaultFocus = page.DefaultFocus;

		float y = PAD;
		if (!page.Title.empty())
		{
			layout.Texts.push_back({ page.Title, page.TitleSize, { PAD, y, PageMetrics::TITLE_W, page.HeaderH }, false });
			if (!page.Header.empty())
				layout.Texts.push_back({ page.Header, TAB_SIZE, { PAD + PageMetrics::TITLE_W + GAP, y, page.ContentW - PageMetrics::TITLE_W - GAP, page.HeaderH }, false });
			y += page.HeaderH + GAP;
		}

		for (const PageRow& row : page.Rows)
		{
			float x = PAD + row.Indent;
			for (const PageCell& cell : row.Cells)
			{
				switch (cell.Kind)
				{
				case CellKind::Text:
					if (!cell.Label.empty())
						layout.Texts.push_back({ cell.Label, cell.TextSize, { x, y, cell.Width, cell.Height }, cell.Wrap });
					break;
				case CellKind::Button:
				{
					ScreenWidget widget{ cell.Label, { x, y, cell.Width, cell.Height }, cell.TextSize };
					widget.Focusable = cell.Enabled;
					layout.Widgets.push_back(widget);
					break;
				}
				case CellKind::Slider:
				{
					const float trackH = PageMetrics::SLIDER_H;
					ScreenWidget widget{ cell.Label, { x + PageMetrics::SLIDER_RING, y + (cell.Height - trackH) * 0.5f, cell.Width - 2.0f * PageMetrics::SLIDER_RING, trackH }, 0.0f };
					widget.Focusable = cell.Enabled;
					widget.Adjusts = true;
					layout.Widgets.push_back(widget);
					break;
				}
				}
				x += cell.Width + GAP;
			}
			y += row.Height + GAP;
		}

		layout.Panel = { 0.0f, 0.0f, page.ContentW + 2.0f * PAD, y - GAP + PAD };
		return layout;
	}

}
