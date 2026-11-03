#pragma once
#include "Ui/UiKit.h"

#include <string>

namespace Dingo
{

	// The header row, the tab row and the shared text boxes both full screens are made of.
	void DrawHeader(const UiKit& kit, const char* title, const std::string& resources, float contentW);
	void DrawCaption(const UiKit& kit, const std::string& text, float width, float height, float size, const glm::vec4& color, bool wrap);
	// A tab button; `current` marks the screen that is open. Applies a pending focus request for it.
	bool DrawTab(const UiKit& kit, const char* label, float width, bool current, const std::string& focusRequest);
	// DingoUI::SetFocus must run in the container that declares the widget.
	void ApplyFocusRequest(const std::string& request, const std::string& label);

}
