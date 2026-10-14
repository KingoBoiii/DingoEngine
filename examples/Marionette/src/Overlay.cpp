#include "Overlay.h"
#include "GameTuning.h"

#include <cstdint>

namespace Dingo::Overlay
{

	namespace
	{
		constexpr uint16_t k_FirstKey = static_cast<uint16_t>(KeyCode::Space);
		constexpr uint16_t k_LastKey = static_cast<uint16_t>(KeyCode::Menu);
		constexpr uint16_t k_FirstFunctionKey = static_cast<uint16_t>(KeyCode::F1);
		constexpr uint16_t k_LastFunctionKey = static_cast<uint16_t>(KeyCode::F25);
		constexpr uint16_t k_FirstModifier = static_cast<uint16_t>(KeyCode::LeftShift);

		bool s_RefocusPending = false;
		bool s_RefocusFrame = false;

		bool IsMenuKey(uint16_t code)
		{
			const bool function = code >= k_FirstFunctionKey && code <= k_LastFunctionKey;
			const bool modifier = code >= k_FirstModifier;
			return !function && !modifier;
		}
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
		return UIRootParams().SetAnchor(anchor).SetScale(UIScale::ScaleWithScreen()).SetBlocksInput(false);
	}

	UIRootParams MenuRoot(const UIBoxParams& layout)
	{
		return UIRootParams()
			.SetAnchor(UIAnchor::Stretch)
			.SetScale(UIScale::ScaleWithScreen())
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

	bool AnyInputPressed()
	{
		for (uint16_t code = k_FirstKey; code <= k_LastKey; ++code)
		{
			if (IsMenuKey(code) && Input::IsKeyPressed(static_cast<KeyCode>(code)))
				return true;
		}

		if (!s_RefocusFrame && Input::IsAnyMouseButtonPressed())
			return true;

		for (uint32_t button = 0; button < k_GamepadButtonCount; ++button)
		{
			if (Input::IsGamepadButtonPressed(static_cast<GamepadButton>(button)))
				return true;
		}
		return false;
	}

}
