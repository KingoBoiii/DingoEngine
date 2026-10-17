#include "Ui/ScreenParts.h"
#include "GameTuning.h"

namespace Dingo
{

	using namespace UiMetrics;

	void ApplyFocusRequest(const std::string& request, const std::string& label)
	{
		if (!request.empty() && request == label)
			DingoUI::SetFocus(label);
	}

	void DrawHeader(const UiKit& kit, const char* title, const std::string& resources, float contentW)
	{
		if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(contentW), UISize::Fixed(HEADER_H)).SetDirection(UIDirection::Row).SetGap(GAP).SetAlign(UIAlign::Center)))
			return;
		DingoUI::Text(title, LineText(kit, TITLE_SIZE, UI_TEXT_PICK_COLOR).SetWidth(UISize::Fixed(220.0f)));
		DingoUI::Text(resources, LineText(kit, TAB_SIZE, UI_TEXT_COLOR).SetWidth(UISize::Fixed(contentW - 220.0f - GAP)));
		DingoUI::EndBox();
	}

	void DrawCaption(const UiKit& kit, const std::string& text, float width, float height, float size, const glm::vec4& color, bool wrap)
	{
		if (!DingoUI::BeginBox(UIBoxParams().SetSize(UISize::Fixed(width), UISize::Fixed(height))))
			return;
		DingoUI::Text(text, LineText(kit, size, color, wrap).SetWidth(UISize::Fixed(width)));
		DingoUI::EndBox();
	}

	bool DrawTab(const UiKit& kit, const char* label, float width, bool current, const std::string& focusRequest)
	{
		ApplyFocusRequest(focusRequest, label);
		const UIStyle style = TintedButton(kit.Tab, current ? glm::vec4(0.4f, 0.3f, 0.12f, 1.0f) : UI_BUTTON_COLOR, TAB_SIZE,
			current ? UI_TEXT_PICK_COLOR : UI_TEXT_COLOR, current ? UI_SELECTED_COLOR : glm::vec4(0.0f));
		return DingoUI::Button(label, UIWidgetParams().SetSize(UISize::Fixed(width), UISize::Fixed(TAB_H)).SetStyle(&style)).Clicked;
	}

}
