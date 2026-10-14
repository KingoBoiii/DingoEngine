#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

namespace Dingo::Overlay
{
	// The scale of every root: it follows the screen's height, which UiPx and GetHudBottomNdc are measured in.
	UIScale Scale();
	// The theme of every button, set once by the layer.
	UIStyle MenuStyle();
	// A root for what is drawn over the game and never takes the mouse or a key.
	UIRootParams HudRoot(UIAnchor anchor);
	// A full-screen root that fades with the screen and whose buttons the keyboard and pad can reach.
	UIRootParams MenuRoot(const UIBoxParams& layout);

	// The window gained or lost focus: the click that focused it must not count as an input in the next frame.
	void NoteFocus(bool focused);
	// Once at the start of each update, before anything asks.
	void BeginFrame();
	bool IsRefocusFrame();
}
