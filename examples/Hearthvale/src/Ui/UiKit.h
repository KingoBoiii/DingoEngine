#pragma once
#include <DingoEngine.h>

#include "Ui/ScreenLayouts.h"

namespace Dingo
{

	// The fonts and styles every game screen draws with. GPU-side: it holds a Font.
	struct UiKit
	{
		Font* TextFont = nullptr;
		UIStyle Theme;
		UIStyle Tab;
		UIStyle Cell;
	};

	UIStyle MakeTheme(Font* font);
	UiKit MakeKit(Font* font);

	// A button style filled with `fill`, a border when `border.a` is above zero, and hover and pressed fills derived from it.
	UIStyle TintedButton(const UIStyle& base, const glm::vec4& fill, float textSize, const glm::vec4& textColor, const glm::vec4& border);

	UITextParams LineText(const UiKit& kit, float size, const glm::vec4& color, bool wrap = false);

	// The root every full screen uses: dims the game, takes the pad's navigation, and centres one panel.
	UIRootParams ScreenRoot();
	UIBoxParams ScreenPanel(const UiRect& panel);

}
