#include "Screens.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "MatchState.h"
#include "Overlay.h"
#include "Showcase.h"

#include <format>

namespace Dingo
{

	TitleControllerScript::TitleControllerScript(const GameAssets* assets)
		: m_Assets(assets)
	{}

	TitleControllerScript::~TitleControllerScript() = default;

	void TitleControllerScript::OnStart()
	{
		Scene& scene = GetScene();
		Font* font = nullptr;
		if (m_Assets)
		{
			ShowcaseParams params;
			params.Kind = ShowcaseKind::Title;
			m_Showcase = std::make_unique<Showcase>(scene, *m_Assets, params);
			font = m_Assets->GetFont();
		}

		Overlay::MakeCamera(scene, "TitleCamera", HUD_ORTHO_SIZE);
		Overlay::MakeText(scene, font, "Title", TITLE_HEADING_SIZE, COLOR_TITLE, { 0.0f, TITLE_HEADING_Y, 0.0f }, "MARIONETTE");
		Overlay::MakeText(scene, font, "Prompt", TITLE_PROMPT_SIZE, COLOR_TEXT, { 0.0f, TITLE_PROMPT_Y, 0.0f }, "press any key / button");
	}

	void TitleControllerScript::OnUpdate(float deltaTime)
	{
		if (m_Showcase)
			m_Showcase->Update(deltaTime);

		m_Age += deltaTime;
		if (Input::IsKeyPressed(Key::Escape))
		{
			if (m_Age >= TITLE_ESC_GRACE)
				Application::Get().Close();
		}
		else if (Overlay::AnyInputPressed())
		{
			RequestSceneTransition(SCENE_ARENA);
		}
	}

	void TitleControllerScript::OnDestroy()
	{
		m_Showcase.reset();
	}

	EndControllerScript::EndControllerScript(const MatchState* match, const GameAssets* assets)
		: m_Match(match), m_Assets(assets)
	{}

	EndControllerScript::~EndControllerScript() = default;

	void EndControllerScript::OnStart()
	{
		const bool won = m_Match && m_Match->Victory;
		Scene& scene = GetScene();
		Font* font = nullptr;
		if (m_Assets)
		{
			ShowcaseParams params;
			params.Kind = ShowcaseKind::Victory;
			params.Taunt = won;
			m_Showcase = std::make_unique<Showcase>(scene, *m_Assets, params);
			font = m_Assets->GetFont();
		}

		Overlay::MakeCamera(scene, "EndCamera", HUD_ORTHO_SIZE);
		Overlay::MakeText(scene, font, "EndTitle", END_HEADING_SIZE, COLOR_TITLE, { 0.0f, END_HEADING_Y, 0.0f }, won ? "VICTORY" : "THE END");
		if (won)
		{
			const int total = static_cast<int>(m_Match->Seconds);
			Overlay::MakeText(scene, font, "EndSubtitle", END_SUBTITLE_SIZE, COLOR_TEXT, { 0.0f, END_SUBTITLE_Y, 0.0f }, "All three opponents are down");
			Overlay::MakeText(scene, font, "EndStats", END_STATS_SIZE, COLOR_TEXT_DIM, { 0.0f, END_STATS_Y, 0.0f },
				std::format("Time in the ring  {}:{:02}      Bouts lost  {}", total / 60, total % 60, m_Match->Retries).c_str());
		}
		Overlay::MakeText(scene, font, "EndPrompt", END_PROMPT_SIZE, COLOR_TEXT, { 0.0f, END_PROMPT_Y, 0.0f }, "press any key / button");
	}

	void EndControllerScript::OnUpdate(float deltaTime)
	{
		if (m_Showcase)
			m_Showcase->Update(deltaTime);

		m_Age += deltaTime;
		if (m_Age >= END_INPUT_DELAY && Overlay::AnyInputPressed())
			RequestSceneTransition(SCENE_TITLE);
	}

	void EndControllerScript::OnDestroy()
	{
		m_Showcase.reset();
	}

}
