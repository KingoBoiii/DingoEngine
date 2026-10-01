#include "TitleScreen.h"
#include "GameTuning.h"
#include "Overlay.h"

#include <format>

namespace
{
	using namespace Dingo;

	Entity AddLine(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, const glm::vec3& position, const std::string& text)
	{
		Entity entity = Overlay::MakeText(scene, font, name, size, color, true);
		entity.GetComponent<TextComponent>().Text = text;
		entity.GetComponent<TransformComponent>().Position = position;
		return entity;
	}

	void SetLineText(Entity& line, const std::string& text)
	{
		auto& lineText = line.GetComponent<TextComponent>();
		if (lineText.Text != text)
			lineText.Text = text;
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
		m_Font = Overlay::LoadFont("title");

		Scene& scene = GetScene();
		Overlay::MakeCamera(scene, "TitleCamera", HUD_ORTHO_SIZE);

		AddLine(scene, m_Font, "Title", TITLE_HEADING_SIZE, COLOR_TITLE, { 0.0f, TITLE_HEADING_Y, 0.0f }, "CANDLEWICK");
		AddLine(scene, m_Font, "Tagline", TITLE_TAGLINE_SIZE, COLOR_TEXT_DIM, { 0.0f, TITLE_TAGLINE_Y, 0.0f },
			"A lantern, a keep, and the wardens who hunt by light.");
		m_Prompt = AddLine(scene, m_Font, "Prompt", TITLE_PROMPT_SIZE, COLOR_TEXT, { 0.0f, TITLE_PROMPT_Y, 0.0f }, m_PromptText.Get());
		m_Controls = AddLine(scene, m_Font, "Controls", TITLE_CONTROLS_SIZE, COLOR_TEXT_DIM, { 0.0f, TITLE_CONTROLS_Y, 0.0f }, m_ControlsText.Get());
	}

	void TitleControllerScript::OnUpdate(float)
	{
		SetLineText(m_Prompt, m_PromptText.Get());
		SetLineText(m_Controls, m_ControlsText.Get());

		if (Input::IsKeyPressed(Key::Escape))
			Application::Get().Close();
		else if (Overlay::ConfirmPressed())
			RequestSceneTransition(SCENE_KEEP);
	}

	void TitleControllerScript::OnDestroy()
	{
		DestroyAndDelete(m_Font);
	}

	void EndControllerScript::OnStart()
	{
		m_Font = Overlay::LoadFont("end");

		Scene& scene = GetScene();
		Overlay::MakeCamera(scene, "EndCamera", HUD_ORTHO_SIZE);

		AddLine(scene, m_Font, "EndTitle", END_HEADING_SIZE, COLOR_TITLE, { 0.0f, END_HEADING_Y, 0.0f }, "The altar burns.");
		AddLine(scene, m_Font, "EndTime", END_TIME_SIZE, COLOR_TEXT, { 0.0f, END_TIME_Y, 0.0f }, TimeLine(m_Result->Seconds));
		AddLine(scene, m_Font, "EndCatches", END_CATCHES_SIZE, COLOR_TEXT_DIM, { 0.0f, END_CATCHES_Y, 0.0f }, CatchesLine(m_Result->Catches));
		m_Prompt = AddLine(scene, m_Font, "EndPrompt", END_PROMPT_SIZE, COLOR_TEXT, { 0.0f, END_PROMPT_Y, 0.0f }, m_PromptText.Get());
	}

	void EndControllerScript::OnUpdate(float)
	{
		SetLineText(m_Prompt, m_PromptText.Get());

		if (Input::IsKeyPressed(Key::Escape) || Overlay::ConfirmPressed())
			RequestSceneTransition(SCENE_TITLE);
	}

	void EndControllerScript::OnDestroy()
	{
		DestroyAndDelete(m_Font);
	}

}
