#include "Hud.h"
#include "BoutFlow.h"
#include "Fighter.h"
#include "GameTuning.h"
#include "Overlay.h"

#include <algorithm>
#include <format>

namespace
{
	using namespace Dingo;

	constexpr int k_CoverLayer = 100;

	float Fraction(const Fighter& fighter)
	{
		const float maximum = fighter.GetMaxHealth();
		return maximum > 0.0f ? std::clamp(fighter.GetHealth() / maximum, 0.0f, 1.0f) : 0.0f;
	}

	void Segment(float width, float height, const glm::vec4& color)
	{
		if (width < 0.5f)
			return;
		DingoUI::BeginBox(UIBoxParams().SetWidth(UISize::Fixed(width)).SetHeight(UISize::Fixed(height)).SetBackground(color));
		DingoUI::EndBox();
	}

	// The fill sits on the side the bars face each other on, so a hit eats a bar from its outer end; the trail is the
	// stretch the fill just lost, right behind it.
	void DrawBar(float width, float height, float inset, float fraction, float trail, const glm::vec4& fill, bool fillOnRight)
	{
		const float innerWidth = width - 2.0f * inset;
		const float innerHeight = height - 2.0f * inset;
		const float fillWidth = innerWidth * fraction;
		const float trailWidth = innerWidth * (trail - fraction);

		DingoUI::BeginBox(UIBoxParams().SetWidth(UISize::Fixed(width)).SetHeight(UISize::Fixed(height)).SetPadding(inset)
			.SetDirection(UIDirection::Row).SetJustify(fillOnRight ? UIJustify::End : UIJustify::Start).SetBackground(COLOR_BAR_BACK));
		if (fillOnRight)
		{
			Segment(trailWidth, innerHeight, COLOR_BAR_TRAIL);
			Segment(fillWidth, innerHeight, fill);
		}
		else
		{
			Segment(fillWidth, innerHeight, fill);
			Segment(trailWidth, innerHeight, COLOR_BAR_TRAIL);
		}
		DingoUI::EndBox();
	}

	void DeclareBar(std::string_view name, float fraction, float trail, const glm::vec4& fill, bool fillOnRight)
	{
		const float barTop = HUD_BAR_DROP - 0.5f * HUD_BAR_HEIGHT;
		DingoUI::BeginBox(UIBoxParams().SetAlign(UIAlign::Center));
		DingoUI::BeginBox(UIBoxParams().SetHeight(UISize::Fixed(UiPx(barTop))).SetJustify(UIJustify::End).SetAlign(UIAlign::Center)
			.SetPadding(0.0f, 0.0f, 0.0f, UiPx(barTop - HUD_NAME_DROP - 0.5f * HUD_NAME_SIZE)));
		DingoUI::Text(name, UITextParams().SetSize(UiPx(HUD_NAME_SIZE)).SetColor(COLOR_TEXT).SetWrap(false));
		DingoUI::EndBox();
		DrawBar(UiPx(HUD_BAR_WIDTH), UiPx(HUD_BAR_HEIGHT), UiPx(HUD_BAR_INSET), fraction, trail, fill, fillOnRight);
		DingoUI::EndBox();
	}

	void DeclareLine(const char* id, std::string_view text, float size, float y, const glm::vec4& color, const UITransition& transition)
	{
		if (text.empty() || !DingoUI::BeginRoot(id, Overlay::HudRoot(UIAnchor::Center).SetOffset({ 0.0f, -UiPx(y) }).SetTransition(transition)))
			return;
		DingoUI::Text(text, UITextParams().SetSize(UiPx(size)).SetColor(color).SetWrap(false));
		DingoUI::EndRoot();
	}

	bool DeclareCover(const char* id, const UITransition& transition)
	{
		if (!DingoUI::BeginRoot(id, UIRootParams().SetAnchor(UIAnchor::Stretch).SetLayer(k_CoverLayer).SetBlocksInput(false)
				.SetTransition(transition).SetLayout(UIBoxParams().SetBackground(COLOR_FADE))))
			return false;
		DingoUI::EndRoot();
		return true;
	}
}

namespace Dingo
{

	float GetHudBottomNdc()
	{
		const float halfHeight = 0.5f * HUD_ORTHO_SIZE;
		return (halfHeight - HUD_BAR_DROP - 0.5f * HUD_BAR_HEIGHT) / halfHeight;
	}

	Hud::Hud(Scene& scene, int bout, bool fadeIn, bool showHint)
		: m_Scene(&scene), m_Bout(bout), m_ShowHint(showHint), m_FadeIn(fadeIn),
		m_BoutLabelText(std::format("BOUT {} / {}", bout, BOUT_COUNT)), m_IntroText(std::format("BOUT {}", bout))
	{
		if (bout < BOUT_COUNT)
			m_NextText = std::format("Next: {}", GetOpponentDef(bout + 1).Title);
	}

	Hud::~Hud() = default;

	void Hud::UpdateTrail(Trail& trail, float deltaTime, float fraction)
	{
		if (fraction >= trail.Fraction)
		{
			trail.Fraction = fraction;
			trail.Delay = 0.0f;
		}
		else if (fraction < trail.Last)
		{
			trail.Delay = HUD_TRAIL_DELAY;
		}
		else if (trail.Delay > 0.0f)
		{
			trail.Delay -= deltaTime;
		}
		else
		{
			trail.Fraction = std::max(fraction, trail.Fraction - HUD_TRAIL_RATE * deltaTime);
		}
		trail.Last = fraction;
	}

	void Hud::DeclareWorldBar(const char* id, const Fighter& fighter, const Trail& trail, float fraction, const glm::vec4& fill) const
	{
		if (fighter.GetHealth() <= 0.0f)
			return;

		const glm::vec3 head = fighter.GetPosition() + glm::vec3(0.0f, fighter.GetModelHeight() * fighter.GetDef().Scale + HUD_WORLD_BAR_LIFT, 0.0f);
		const UIWorldRootParams params = UIWorldRootParams().SetScale(UIScale::ScaleWithScreen()).SetDistanceScale(HUD_WORLD_REFERENCE_DISTANCE, 0.6f, 1.5f);
		if (!DingoUI::BeginWorldRoot(id, *m_Scene, head, params))
			return;
		DrawBar(UiPx(HUD_WORLD_BAR_WIDTH), UiPx(HUD_WORLD_BAR_HEIGHT), UiPx(HUD_WORLD_BAR_INSET), fraction, trail.Fraction, fill, false);
		DingoUI::EndRoot();
	}

	void Hud::DeclareBanner(const BoutFlow& flow, const Fighter& opponent) const
	{
		const FighterDef& def = opponent.GetDef();
		const UITransition appear = flow.GetRules().Frozen ? UITransition() : UITransition::Fade(HUD_BANNER_FADE).SetExitDuration(0.0f);

		switch (flow.GetPhase())
		{
			case BoutPhase::Intro:
				DeclareLine("intro_big", m_IntroText, HUD_BANNER_BIG_SIZE, HUD_BANNER_BIG_Y, COLOR_TITLE, appear);
				DeclareLine("intro_title", def.Title ? def.Title : def.Name, HUD_BANNER_TITLE_SIZE, HUD_BANNER_TITLE_Y, COLOR_TEXT, appear);
				DeclareLine("intro_note", def.Name, HUD_BANNER_SMALL_SIZE, HUD_BANNER_SMALL_Y, COLOR_TEXT_DIM, appear);
				break;

			case BoutPhase::Fight:
				if (flow.GetFightTime() < 0.5f * BOUT_FIGHT_BANNER_SECONDS)
				{
					const UITransition fade = UITransition::Fade(0.0f).SetExitDuration(0.5f * BOUT_FIGHT_BANNER_SECONDS).SetEase(UIEase::Linear);
					DeclareLine("fight", "FIGHT", HUD_FIGHT_SIZE, HUD_FIGHT_Y, COLOR_TITLE, fade);
				}
				break;

			case BoutPhase::Knockout:
			{
				std::string_view title;
				std::string_view note;
				glm::vec4 titleColor = COLOR_TEXT;
				switch (flow.GetWinner())
				{
					case BoutWinner::Player:
						if (m_Bout < BOUT_COUNT)
						{
							title = m_NextText;
						}
						else
						{
							title = "VICTORY";
							titleColor = COLOR_TITLE;
						}
						break;
					case BoutWinner::Opponent:
						title = "Defeated";
						note = "The bout starts again";
						break;
					case BoutWinner::Draw:
						title = "Double K.O.";
						note = "The bout starts again";
						break;
					default:
						break;
				}
				DeclareLine("ko_big", "K.O.", HUD_KO_SIZE, HUD_KO_Y, COLOR_TEXT_ALERT, appear);
				DeclareLine("ko_title", title, HUD_BANNER_TITLE_SIZE, HUD_BANNER_TITLE_Y, titleColor, appear);
				DeclareLine("ko_note", note, HUD_BANNER_SMALL_SIZE, HUD_BANNER_SMALL_Y, COLOR_TEXT_DIM, appear);
				break;
			}
		}
	}

	void Hud::DeclareHint(float deltaTime)
	{
		m_HintClock += deltaTime;
		if (!m_ShowHint || HUD_HINT_SECONDS - m_HintClock <= HUD_HINT_FADE)
			return;

		const UITransition fade = UITransition::Fade(0.0f).SetExitDuration(HUD_HINT_FADE).SetEase(UIEase::Linear);
		if (!DingoUI::BeginRoot("hint", Overlay::HudRoot(UIAnchor::Bottom).SetOffset({ 0.0f, -UiPx(HUD_HINT_RISE - 0.5f * HUD_HINT_SIZE) }).SetTransition(fade)))
			return;
		DingoUI::Text(Input::IsGamepadConnected() ? HUD_HINT_GAMEPAD : HUD_HINT_KEYBOARD, UITextParams().SetSize(UiPx(HUD_HINT_SIZE)).SetColor(COLOR_TEXT_DIM).SetWrap(false));
		DingoUI::EndRoot();
	}

	void Hud::DeclareCovers(const BoutFlow& flow)
	{
		const UITransition out = UITransition::Fade(0.0f).SetExitDuration(HUD_FADE_IN_SECONDS).SetEase(UIEase::Linear);
		if (m_FadeIn && DeclareCover("fade_in", out))
			m_FadeIn = false;

		const BoutRules& rules = flow.GetRules();
		if (flow.GetPhase() == BoutPhase::Knockout && !rules.Frozen && rules.KnockoutSeconds > HUD_FADE_OUT_SECONDS
			&& rules.KnockoutSeconds - flow.GetPhaseTime() < HUD_FADE_OUT_SECONDS)
		{
			DeclareCover("fade_out", UITransition::Fade(HUD_FADE_OUT_SECONDS).SetExitDuration(HUD_FADE_OUT_SECONDS).SetEase(UIEase::Linear));
		}
	}

	void Hud::Update(float deltaTime, const Fighter& player, const Fighter& opponent, const BoutFlow& flow)
	{
		const float playerFraction = Fraction(player);
		const float opponentFraction = Fraction(opponent);
		UpdateTrail(m_PlayerTrail, deltaTime, playerFraction);
		UpdateTrail(m_OpponentTrail, deltaTime, opponentFraction);

		if (DingoUI::BeginRoot("bars", Overlay::HudRoot(UIAnchor::Top).SetLayout(UIBoxParams().SetDirection(UIDirection::Row).SetGap(UiPx(HUD_BAR_CENTER_GAP)))))
		{
			DeclareBar(player.GetDef().Name, playerFraction, m_PlayerTrail.Fraction, COLOR_BAR_PLAYER, true);
			DeclareBar(opponent.GetDef().Name, opponentFraction, m_OpponentTrail.Fraction, COLOR_BAR_OPPONENT, false);
			DingoUI::EndRoot();
		}
		DeclareLine("bout", m_BoutLabelText, HUD_BOUT_LABEL_SIZE, 0.5f * HUD_ORTHO_SIZE - HUD_BOUT_LABEL_DROP, COLOR_TEXT_DIM, UITransition());

		DeclareWorldBar("player_hp", player, m_PlayerTrail, playerFraction, COLOR_BAR_PLAYER);
		DeclareWorldBar("opponent_hp", opponent, m_OpponentTrail, opponentFraction, COLOR_BAR_OPPONENT);
		DeclareBanner(flow, opponent);
		DeclareHint(deltaTime);
		DeclareCovers(flow);
	}

}
