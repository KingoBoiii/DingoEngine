#include "TitleScreen.h"
#include "GameTuning.h"
#include "Overlay.h"

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

	void RefreshPrompt(Entity& prompt, const char* action)
	{
		const std::string text = Overlay::ConfirmPrompt(action);
		auto& promptText = prompt.GetComponent<TextComponent>();
		if (promptText.Text != text)
			promptText.Text = text;
	}
}

namespace Dingo
{

	void TitleControllerScript::OnStart()
	{
		m_Font = Overlay::LoadFont("title");

		Scene& scene = GetScene();
		Overlay::MakeCamera(scene, "TitleCamera", HUD_ORTHO_SIZE);

		AddLine(scene, m_Font, "Title", 1.5f, COLOR_TITLE, { 0.0f, 2.4f, 0.0f }, "CANDLEWICK");
		AddLine(scene, m_Font, "Tagline", 0.42f, COLOR_TEXT_DIM, { 0.0f, 0.5f, 0.0f },
			"A lantern, a keep, and the wardens who hunt by light.");
		m_Prompt = AddLine(scene, m_Font, "Prompt", 0.5f, COLOR_TEXT, { 0.0f, -2.2f, 0.0f }, Overlay::ConfirmPrompt("enter"));
	}

	void TitleControllerScript::OnUpdate(float)
	{
		RefreshPrompt(m_Prompt, "enter");

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

		AddLine(scene, m_Font, "EndTitle", 1.2f, COLOR_TITLE, { 0.0f, 1.8f, 0.0f }, "The altar burns.");
		m_Prompt = AddLine(scene, m_Font, "EndPrompt", 0.45f, COLOR_TEXT, { 0.0f, -1.2f, 0.0f }, Overlay::ConfirmPrompt("return to the title"));
	}

	void EndControllerScript::OnUpdate(float)
	{
		RefreshPrompt(m_Prompt, "return to the title");

		if (Input::IsKeyPressed(Key::Escape) || Overlay::ConfirmPressed())
			RequestSceneTransition(SCENE_TITLE);
	}

	void EndControllerScript::OnDestroy()
	{
		DestroyAndDelete(m_Font);
	}

}
