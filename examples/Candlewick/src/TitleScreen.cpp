#include "TitleScreen.h"
#include "GameTuning.h"
#include "Overlay.h"

#include <format>

namespace
{
	using namespace Dingo;

	void AddCamera(Scene& scene, const char* name)
	{
		Entity entity = scene.CreateEntity(name);
		auto& camera = entity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.Primary = true;
	}

	void Line(float& bottom, float baseline, float size, const glm::vec4& color, const std::string& text)
	{
		const Overlay::Band band(bottom, Overlay::TextTop(ScreenTop(baseline), size), UiPx(size));
		DingoUI::Text(text, UITextParams().SetWrap(false).SetAlign(UITextAlign::Center).SetSize(UiPx(size)).SetColor(color));
	}

	bool ButtonLine(float& bottom, float baseline, float size, const std::string& label, bool prompt)
	{
		const Overlay::Band band(bottom, Overlay::ButtonTop(ScreenTop(baseline), size), Overlay::ButtonHeight(size));
		return Overlay::Button(label, size, prompt ? UIWidgetParams().SetDefaultFocus() : UIWidgetParams(), prompt);
	}

	std::string TimeLine(float seconds)
	{
		const int total = static_cast<int>(seconds);
		return std::format("Time  {}:{:02}", total / 60, total % 60);
	}

	std::string CatchesLine(int catches)
	{
		if (catches <= 0)
			return "Never caught";
		if (catches == 1)
			return "Caught once";
		return std::format("Caught {} times", catches);
	}
}

namespace Dingo
{

	void TitleControllerScript::OnStart()
	{
		AddCamera(GetScene(), "TitleCamera");
	}

	void TitleControllerScript::OnUpdate(float)
	{
		bool enter = false;
		bool quit = false;
		if (DingoUI::BeginRoot("title", Overlay::MenuRoot()))
		{
			float bottom = 0.0f;
			Line(bottom, TITLE_HEADING_Y, TITLE_HEADING_SIZE, COLOR_TITLE, "CANDLEWICK");
			Line(bottom, TITLE_TAGLINE_Y, TITLE_TAGLINE_SIZE, COLOR_TEXT_DIM, "A lantern, a keep, and the wardens who hunt by light.");
			enter = ButtonLine(bottom, TITLE_PROMPT_Y, TITLE_PROMPT_SIZE, m_PromptText.Get(), true);
			Line(bottom, TITLE_CONTROLS_Y, TITLE_CONTROLS_SIZE, COLOR_TEXT_DIM, m_ControlsText.Get());
			quit = ButtonLine(bottom, TITLE_QUIT_Y, TITLE_QUIT_SIZE, "Quit", false);
			DingoUI::EndRoot();
		}

		if (quit || Overlay::BackPressed())
			Application::Get().Close();
		else if (enter || Overlay::ConfirmPressed())
			RequestSceneTransition(SCENE_KEEP);
	}

	void EndControllerScript::OnStart()
	{
		AddCamera(GetScene(), "EndCamera");
	}

	void EndControllerScript::OnUpdate(float)
	{
		bool back = false;
		if (DingoUI::BeginRoot("end", Overlay::MenuRoot()))
		{
			float bottom = 0.0f;
			Line(bottom, END_HEADING_Y, END_HEADING_SIZE, COLOR_TITLE, "The altar burns.");
			Line(bottom, END_TIME_Y, END_TIME_SIZE, COLOR_TEXT, TimeLine(m_Result->Seconds));
			Line(bottom, END_CATCHES_Y, END_CATCHES_SIZE, COLOR_TEXT_DIM, CatchesLine(m_Result->Catches));
			back = ButtonLine(bottom, END_PROMPT_Y, END_PROMPT_SIZE, m_PromptText.Get(), true);
			DingoUI::EndRoot();
		}

		if (back || Overlay::BackPressed() || Overlay::ConfirmPressed())
			RequestSceneTransition(SCENE_TITLE);
	}

}
