#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <functional>
#include <string>
#include <utility>

namespace Dingo::Overlay
{
	Font* LoadFont();
	// Makes `font` the one every DingoUI text uses and sets the theme; DropFont clears the font before it is destroyed.
	void UseFont(Font* font);
	void DropFont();

	// The scale of every root: it follows the screen's height, which UiPx is measured in.
	UIScale Scale();
	UIStyle MenuStyle();
	// A root for what is drawn over the game and never takes the mouse or a key.
	UIRootParams HudRoot(UIAnchor anchor);
	// A full-screen root that fades with its screen and whose buttons the keyboard and pad can reach.
	UIRootParams MenuRoot(const glm::vec4& background = glm::vec4(0.0f));

	// Design pixels from the top of the screen to the top of a text of `size` units whose baseline sits `baseline` units below it.
	float TextTop(float baseline, float size);
	// The same for a button, whose label is `size` units tall and starts that baseline.
	float ButtonTop(float baseline, float size);
	float ButtonHeight(float size);
	// A button of the menu theme with a label `size` units tall. True when it was clicked, or accepted from the keyboard or pad.
	// A bare button has no fill or border until it is hovered or focused, so it reads as plain text.
	bool Button(const std::string& label, float size, const UIWidgetParams& params = UIWidgetParams(), bool bare = false);

	// One item of a full-screen column, placed `top` design pixels from the screen's top by the gap it leaves after
	// the previous one. `bottom` is the column's running end, kept by the caller and starting at 0.
	class Band
	{
	public:
		Band(float& bottom, float top, float height);
		~Band();

		Band(const Band&) = delete;
		Band& operator=(const Band&) = delete;
	};

	// The window gained or lost focus: the click that focused it must not count as an input in the next frame.
	void NoteFocus(bool focused);
	// Once at the start of each update, before anything asks.
	void BeginFrame();

	// Esc, or the menu's back button once the focus ring is showing and the UI has taken the keys from Input.
	bool BackPressed();

	// The label of a face or menu button on the connected pad family, such as "(A)" or "(Square)".
	const char* PadLabel(GamepadButton button);

	// Confirm prompt matching the connected pad family (PlayStation labels south "X").
	std::string ConfirmPrompt(const char* action);

	// The title screen's one-line controls summary, naming the pad's buttons while one is connected.
	std::string ControlsSummary();

	bool ConfirmPressed();

	// A string built from the connected pad's labels. It is rebuilt only when a pad connects, leaves or
	// changes family, so reading it every frame allocates nothing.
	class PadText
	{
	public:
		explicit PadText(std::function<std::string()> build) : m_Build(std::move(build)) {}

		const std::string& Get();

	private:
		std::function<std::string()> m_Build;
		std::string m_Text;
		bool m_Built = false;
		bool m_Connected = false;
		GamepadType m_Type = GamepadType::Unknown;
	};
}
