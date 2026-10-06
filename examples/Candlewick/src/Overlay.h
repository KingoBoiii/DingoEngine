#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <functional>
#include <string>
#include <utility>

namespace Dingo::Overlay
{
	Font* LoadFont(const char* who);
	Entity MakeCamera(Scene& scene, const char* name, float orthoSize);
	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, bool centered);

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
