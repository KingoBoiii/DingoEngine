#include "Overlay.h"
#include "GameTuning.h"

namespace Dingo::Overlay
{

	namespace
	{
		constexpr float k_HeightMatch = 1.0f;

		bool s_RefocusPending = false;
		bool s_RefocusFrame = false;
	}

	UIScale Scale()
	{
		return UIScale::ScaleWithScreen(glm::vec2(1920.0f, 1080.0f), k_HeightMatch);
	}

	UIStyle MenuStyle()
	{
		return UIStyle::Default()
			.SetTextSize(UiPx(MENU_BUTTON_SIZE))
			.SetTextColor(COLOR_TEXT)
			.SetBackground(COLOR_BAR_BACK)
			.SetHoverColor({ 0.22f, 0.16f, 0.09f, 0.95f })
			.SetPressedColor({ 0.12f, 0.09f, 0.06f, 0.95f })
			.SetFocusedColor(COLOR_TITLE)
			.SetBorder(2.0f, { COLOR_TITLE.r, COLOR_TITLE.g, COLOR_TITLE.b, 0.45f })
			.SetCornerRadius(8.0f)
			.SetPadding(48.0f, 14.0f)
			.SetFocusWidth(3.0f)
			.SetTransition(0.12f);
	}

	UIRootParams HudRoot(UIAnchor anchor)
	{
		return UIRootParams().SetAnchor(anchor).SetScale(Scale()).SetBlocksInput(false);
	}

	UIRootParams MenuRoot(const UIBoxParams& layout)
	{
		return UIRootParams()
			.SetAnchor(UIAnchor::Stretch)
			.SetScale(Scale())
			.SetNavigable(true)
			.SetTransition(UITransition::Fade(UI_FADE_SECONDS))
			.SetLayout(layout);
	}

	void NoteFocus(bool focused)
	{
		if (focused)
			s_RefocusPending = true;
	}

	void BeginFrame()
	{
		s_RefocusFrame = s_RefocusPending;
		s_RefocusPending = false;
	}

	bool IsRefocusFrame()
	{
		return s_RefocusFrame;
	}

}
