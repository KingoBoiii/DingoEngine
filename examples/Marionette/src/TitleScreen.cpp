#include "TitleScreen.h"
#include "GameTuning.h"
#include "MatchState.h"
#include "Overlay.h"

#include <format>

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

		const bool won = m_Match && m_Match->Victory;
		Overlay::MakeText(scene, m_Font, "EndTitle", END_HEADING_SIZE, COLOR_TITLE, { 0.0f, END_HEADING_Y, 0.0f }, won ? "VICTORY" : "THE END");
		if (won)
		{
			const int total = static_cast<int>(m_Match->Seconds);
			Overlay::MakeText(scene, m_Font, "EndSubtitle", END_SUBTITLE_SIZE, COLOR_TEXT, { 0.0f, END_SUBTITLE_Y, 0.0f }, "All three opponents are down");
			Overlay::MakeText(scene, m_Font, "EndStats", END_STATS_SIZE, COLOR_TEXT_DIM, { 0.0f, END_STATS_Y, 0.0f },
				std::format("Time in the ring  {}:{:02}      Bouts lost  {}", total / 60, total % 60, m_Match->Retries).c_str());
		}
		Overlay::MakeText(scene, m_Font, "EndPrompt", END_PROMPT_SIZE, COLOR_TEXT, { 0.0f, END_PROMPT_Y, 0.0f }, "press any key / button");
	}

	void EndControllerScript::OnUpdate(float deltaTime)
	{
		m_Age += deltaTime;
		if (m_Age >= END_INPUT_DELAY && Overlay::AnyInputPressed())
			RequestSceneTransition(SCENE_TITLE);
	}

	void EndControllerScript::OnDestroy()
	{
		DestroyAndDelete(m_Font);
	}

}
