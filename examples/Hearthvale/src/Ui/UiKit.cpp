#include "Ui/UiKit.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		glm::vec4 Mix(const glm::vec4& color, float toward, float amount)
		{
			return glm::vec4(glm::mix(glm::vec3(color), glm::vec3(toward), amount), color.a);
		}
	}

	UIStyle MakeTheme(Font* font)
	{
		UIStyle style = UIStyle::Default();
		style.SetTextFont(font).SetTextColor(UI_TEXT_COLOR).SetDisabledTextColor(UI_TEXT_DIM_COLOR)
			.SetBackground(UI_BUTTON_COLOR).SetHoverColor(UI_BUTTON_HOVER).SetPressedColor(UI_BUTTON_PRESSED)
			.SetFocusedColor(UI_FOCUS_COLOR).SetFocusWidth(3.0f).SetPanelColor(UI_PANEL_COLOR).SetCornerRadius(6.0f)
			.SetAccent(UI_SELECTED_COLOR, UI_TEXT_PICK_COLOR).SetProgressFillColor(UI_SELECTED_COLOR).SetTrackColor(glm::vec4(0.05f, 0.04f, 0.03f, 0.8f))
			.SetPadding(UiMetrics::BUTTON_PAD_X, UiMetrics::BUTTON_PAD_Y);
		return style;
	}

	UiKit MakeKit(Font* font)
	{
		UiKit kit;
		kit.TextFont = font;
		kit.Theme = MakeTheme(font);
		kit.Tab = kit.Theme;
		kit.Tab.SetTextSize(UiMetrics::TAB_SIZE);
		kit.Cell = kit.Theme;
		kit.Cell.SetTextSize(UiMetrics::CELL_SIZE);
		return kit;
	}

	UIStyle TintedButton(const UIStyle& base, const glm::vec4& fill, float textSize, const glm::vec4& textColor, const glm::vec4& border)
	{
		UIStyle style = base;
		style.SetBackground(fill).SetHoverColor(Mix(fill, 1.0f, 0.18f)).SetPressedColor(Mix(fill, 0.0f, 0.3f))
			.SetTextSize(textSize).SetTextColor(textColor);
		if (border.a > 0.0f)
			style.SetBorder(3.0f, border);
		return style;
	}

	UITextParams LineText(const UiKit& kit, float size, const glm::vec4& color, bool wrap)
	{
		Font* font = kit.HeadingFont && size >= UiMetrics::TITLE_SIZE ? kit.HeadingFont : kit.TextFont;
		return UITextParams().SetSize(size).SetColor(color).SetWrap(wrap).SetFont(font);
	}

	UIRootParams ScreenRoot()
	{
		return UIRootParams().SetAnchor(UIAnchor::Stretch).SetLayer(UI_SCREEN_LAYER).SetModal(true).SetNavigable(true)
			.SetScale(UIScale::ScaleWithScreen())
			.SetLayout(UIBoxParams().SetJustify(UIJustify::Center).SetAlign(UIAlign::Center).SetBackground(UI_BACKDROP_COLOR));
	}

	UIBoxParams ScreenPanel(const UiRect& panel)
	{
		return UIBoxParams().SetSize(UISize::Fixed(panel.W), UISize::Fixed(panel.H)).SetDirection(UIDirection::Column)
			.SetGap(UiMetrics::GAP).SetPadding(UiMetrics::PAD).SetBackground(UI_PANEL_COLOR).SetCornerRadius(10.0f);
	}

}
