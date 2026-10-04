#include "TitleScreen.h"
#include "GameTuning.h"
#include "Overlay.h"

namespace Dingo
{

	void TitleControllerScript::OnStart()
	{
		m_Font = Overlay::LoadFont("title");

		Scene& scene = GetScene();
		Overlay::MakeCamera(scene, "TitleCamera", HUD_ORTHO_SIZE);
		Overlay::MakeText(scene, m_Font, "Title", TITLE_HEADING_SIZE, COLOR_TITLE, { 0.0f, TITLE_HEADING_Y, 0.0f }, "MARIONETTE");
		Overlay::MakeText(scene, m_Font, "Prompt", TITLE_PROMPT_SIZE, COLOR_TEXT, { 0.0f, TITLE_PROMPT_Y, 0.0f }, "press any key / button");
	}

	void TitleControllerScript::OnUpdate(float)
	{
		if (Input::IsKeyPressed(Key::Escape))
			Application::Get().Close();
		else if (Overlay::AnyInputPressed())
			RequestSceneTransition(SCENE_ARENA);
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
		Overlay::MakeText(scene, m_Font, "EndTitle", END_HEADING_SIZE, COLOR_TITLE, { 0.0f, END_HEADING_Y, 0.0f }, "THE END");
		Overlay::MakeText(scene, m_Font, "EndPrompt", END_PROMPT_SIZE, COLOR_TEXT, { 0.0f, END_PROMPT_Y, 0.0f }, "press any key / button");
	}

	void EndControllerScript::OnUpdate(float)
	{
		if (Overlay::AnyInputPressed())
			RequestSceneTransition(SCENE_TITLE);
	}

	void EndControllerScript::OnDestroy()
	{
		DestroyAndDelete(m_Font);
	}

}
