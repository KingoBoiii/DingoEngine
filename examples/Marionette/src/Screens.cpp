#include "Screens.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "MatchState.h"
#include "Overlay.h"
#include "Showcase.h"

#include <format>

namespace
{
	using namespace Dingo;

	const char* const k_BackLabel = "Back to title";

	UITextParams Heading(float size, const glm::vec4& color)
	{
		return UITextParams().SetSize(UiPx(size)).SetColor(color).SetWrap(false).SetAlign(UITextAlign::Center);
	}

	UIBoxParams Buttons()
	{
		return UIBoxParams().SetWidth(UISize::Fixed(UiPx(MENU_BUTTON_WIDTH))).SetGap(UiPx(0.2f)).SetAlign(UIAlign::Stretch);
	}

	float TopMargin(float centerY, float size)
	{
		return UiPx(0.5f * HUD_ORTHO_SIZE - centerY - 0.5f * size);
	}

	bool Clicked(const char* label, const UIWidgetParams& params = UIWidgetParams())
	{
		return DingoUI::Button(label, params).Clicked && !Overlay::IsRefocusFrame();
	}
}

namespace Dingo
{

	bool DeclareMissingAssetsScreen()
	{
		bool back = false;
		if (DingoUI::BeginRoot("missing", Overlay::MenuRoot(UIBoxParams().SetAlign(UIAlign::Center).SetJustify(UIJustify::Center).SetGap(UiPx(MISSING_GAP)))))
		{
			DingoUI::Text("assets missing (see log)", Heading(MISSING_HEADING_SIZE, COLOR_TEXT_ALERT));
			back = Clicked(k_BackLabel, UIWidgetParams().SetDefaultFocus());
			DingoUI::EndRoot();
		}
		return back || DingoUI::IsBackPressed();
	}

	TitleControllerScript::TitleControllerScript(const GameAssets* assets)
		: m_Assets(assets)
	{}

	TitleControllerScript::~TitleControllerScript() = default;

	void TitleControllerScript::OnStart()
	{
		if (!m_Assets)
			return;
		ShowcaseParams params;
		params.Kind = ShowcaseKind::Title;
		m_Showcase = std::make_unique<Showcase>(GetScene(), *m_Assets, params);
	}

	void TitleControllerScript::OnUpdate(float deltaTime)
	{
		if (m_Showcase)
			m_Showcase->Update(deltaTime);

		m_Age += deltaTime;
		bool play = false;
		bool quit = false;
		const UIBoxParams layout = UIBoxParams().SetAlign(UIAlign::Center).SetJustify(UIJustify::SpaceBetween)
			.SetPadding(0.0f, TopMargin(TITLE_HEADING_Y, TITLE_HEADING_SIZE), 0.0f, UiPx(MENU_BOTTOM_MARGIN));
		if (DingoUI::BeginRoot("title", Overlay::MenuRoot(layout)))
		{
			DingoUI::Text("MARIONETTE", Heading(TITLE_HEADING_SIZE, COLOR_TITLE));
			DingoUI::BeginBox(Buttons());
			play = Clicked("Play", UIWidgetParams().SetDefaultFocus());
			quit = Clicked("Quit");
			DingoUI::EndBox();
			DingoUI::EndRoot();
		}

		if (play)
			RequestSceneTransition(SCENE_ARENA);
		else if (quit || (m_Age >= TITLE_ESC_GRACE && (Input::IsKeyPressed(Key::Escape) || DingoUI::IsBackPressed())))
			Application::Get().Close();
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
		m_Won = m_Match && m_Match->Victory;
		if (m_Won)
		{
			const int total = static_cast<int>(m_Match->Seconds);
			m_StatsText = std::format("Time in the ring  {}:{:02}      Bouts lost  {}", total / 60, total % 60, m_Match->Retries);
		}
		if (!m_Assets)
			return;
		ShowcaseParams params;
		params.Kind = ShowcaseKind::Victory;
		params.Taunt = m_Won;
		m_Showcase = std::make_unique<Showcase>(GetScene(), *m_Assets, params);
	}

	void EndControllerScript::OnUpdate(float deltaTime)
	{
		if (m_Showcase)
			m_Showcase->Update(deltaTime);

		m_Age += deltaTime;
		const bool ready = m_Age >= END_INPUT_DELAY;
		bool back = false;
		const UIBoxParams layout = UIBoxParams().SetAlign(UIAlign::Center).SetJustify(UIJustify::SpaceBetween)
			.SetPadding(0.0f, TopMargin(END_HEADING_Y, END_HEADING_SIZE), 0.0f, UiPx(MENU_BOTTOM_MARGIN));
		if (DingoUI::BeginRoot("end", Overlay::MenuRoot(layout)))
		{
			DingoUI::BeginBox(UIBoxParams().SetAlign(UIAlign::Center).SetGap(UiPx(0.1f)));
			DingoUI::Text(m_Won ? "VICTORY" : "THE END", Heading(END_HEADING_SIZE, COLOR_TITLE));
			if (m_Won)
			{
				DingoUI::Text("All three opponents are down", Heading(END_SUBTITLE_SIZE, COLOR_TEXT));
				DingoUI::Text(m_StatsText, Heading(END_STATS_SIZE, COLOR_TEXT_DIM));
			}
			DingoUI::EndBox();
			if (ready)
			{
				DingoUI::BeginBox(Buttons());
				back = Clicked(k_BackLabel, UIWidgetParams().SetDefaultFocus());
				DingoUI::EndBox();
			}
			DingoUI::EndRoot();
		}

		if (back || (ready && DingoUI::IsBackPressed()))
			RequestSceneTransition(SCENE_TITLE);
	}

	void EndControllerScript::OnDestroy()
	{
		m_Showcase.reset();
	}

}
