#pragma once
#include "Ui/ScreenLayouts.h"

#include <string>
#include <vector>

namespace Dingo
{

	namespace PageMetrics
	{
		inline constexpr float MENU_W = 360.0f;
		inline constexpr float MENU_H = 52.0f;
		inline constexpr float MENU_SIZE = 20.0f;
		inline constexpr float ROW_H = 44.0f;
		inline constexpr float ROW_SIZE = 16.0f;
		inline constexpr float NOTE_SIZE = 15.0f;
		inline constexpr float NAV_W = 130.0f;
		inline constexpr float NAV_H = 40.0f;
		inline constexpr float TITLE_W = 300.0f;
		inline constexpr float BIG_TITLE_SIZE = 56.0f;
		inline constexpr float BIG_TITLE_H = 72.0f;
		inline constexpr float SLIDER_H = 24.0f;
		inline constexpr float SLIDER_RING = 4.0f;
		inline constexpr float VALUE_W = 80.0f;
	}

	enum class CellKind : uint8_t { Text, Button, Slider };
	enum class PageTone : uint8_t { Normal, Dim, Pick, Bad };

	// What a button or slider does when the player picks it; ScreenHost::Act and ScreenHost::SetSlider are the only
	// places that read it, so a page is plain data and the drawing code holds no rules.
	enum class PageAction : uint8_t
	{
		None, Back,
		TitleNew, TitleContinue, OpenOptions, OpenCredits, QuitGame,
		SlotPick,
		PauseResume, PauseSave,
		Tab, Row, CycleQuantity, PrevPage, NextPage,
		BoardGive, FestivalStart,
		DialogueNext, DialogueGift,
		OptionVSync, OptionFullscreen, OptionFullscreenKind, OptionHoldToPlace, OptionSlider
	};

	struct PageCell
	{
		CellKind Kind = CellKind::Text;
		// Text: the words. Button and slider: the DingoUI label, whose part before "##" is what is shown.
		std::string Label;
		float Width = 0.0f;
		float Height = 0.0f;
		float TextSize = 0.0f;
		PageTone Tone = PageTone::Normal;
		bool Wrap = false;
		bool Enabled = true;
		bool Highlight = false;
		PageAction Action = PageAction::None;
		int Arg = 0;
		float Value = 0.0f;
		float Min = 0.0f;
		float Max = 1.0f;
		float Step = 0.0f;
	};

	struct PageRow
	{
		std::vector<PageCell> Cells;
		float Height = 0.0f;
		float Indent = 0.0f;
	};

	// A screen as data: a header, rows of cells, and the widget that takes the focus first.
	struct PageSpec
	{
		ScreenId Id = ScreenId::None;
		std::string Title;
		std::string Header;
		float TitleSize = UiMetrics::TITLE_SIZE;
		float HeaderH = UiMetrics::HEADER_H;
		float ContentW = 0.0f;
		std::vector<PageRow> Rows;
		std::string DefaultFocus;
		int PageIndex = 0;
		int PageCount = 1;
	};

	PageCell TextCell(std::string text, float width, float height, float size, PageTone tone = PageTone::Normal, bool wrap = false);
	PageCell ButtonCell(std::string label, float width, float height, float size, PageAction action, int arg = 0, bool enabled = true);
	// `width` is the cell; the track DingoUI draws is inset by SLIDER_RING on each side.
	PageCell SliderCell(std::string label, float width, float height, float value, float min, float max, float step, PageAction action, int arg);
	PageRow MakeRow(std::vector<PageCell> cells, float indent = 0.0f);

	// The panel, widgets and texts of a page in the same panel-relative coordinates as the inventory's layout, so the
	// same overflow, overlap and reachability checks run over every screen.
	ScreenLayout LayoutPage(const PageSpec& page);

	// The width a row of `widths` takes with the page's gap between cells.
	float RowWidth(const std::vector<float>& widths);
	// Index of the widget with `label`, or -1.
	int FindWidgetCell(const PageSpec& page, const std::string& label, int* row = nullptr);

}
