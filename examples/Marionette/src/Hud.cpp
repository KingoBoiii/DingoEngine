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

	glm::vec4 WithAlpha(const glm::vec4& color, float alpha)
	{
		return glm::vec4(glm::vec3(color), color.a * std::clamp(alpha, 0.0f, 1.0f));
	}

	float Fraction(const Fighter& fighter)
	{
		const float maximum = fighter.GetMaxHealth();
		return maximum > 0.0f ? std::clamp(fighter.GetHealth() / maximum, 0.0f, 1.0f) : 0.0f;
	}
}

namespace Dingo
{

	float GetHudBottomNdc()
	{
		const float halfHeight = 0.5f * HUD_ORTHO_SIZE;
		return (halfHeight - HUD_BAR_DROP - 0.5f * HUD_BAR_HEIGHT) / halfHeight;
	}

	Hud::Hud(Scene& scene, const GameAssets& assets, int bout, bool fadeIn, bool showHint)
		: m_Font(assets.GetFont()), m_Bout(bout), m_ShowHint(showHint), m_FadeInLeft(fadeIn ? HUD_FADE_IN_SECONDS : 0.0f),
		m_BoutLabelText(std::format("BOUT {} / {}", bout, BOUT_COUNT)), m_IntroText(std::format("BOUT {}", bout))
	{
		if (bout < BOUT_COUNT)
			m_NextText = std::format("Next: {}", GetOpponentDef(bout + 1).Title);
		Overlay::MakeCamera(scene, "HudCamera", HUD_ORTHO_SIZE);

		m_PlayerBar = MakeBar(scene, "Player", COLOR_BAR_PLAYER);
		m_OpponentBar = MakeBar(scene, "Opponent", COLOR_BAR_OPPONENT);
		m_BoutLabel = Overlay::MakeText(scene, m_Font, "BoutLabel", HUD_BOUT_LABEL_SIZE, COLOR_TEXT_DIM, glm::vec3(0.0f), "");
		m_BannerBig = Overlay::MakeText(scene, m_Font, "BannerBig", HUD_BANNER_BIG_SIZE, COLOR_TITLE, glm::vec3(0.0f), "");
		m_BannerTitle = Overlay::MakeText(scene, m_Font, "BannerTitle", HUD_BANNER_TITLE_SIZE, COLOR_TEXT, glm::vec3(0.0f), "");
		m_BannerSmall = Overlay::MakeText(scene, m_Font, "BannerSmall", HUD_BANNER_SMALL_SIZE, COLOR_TEXT_DIM, glm::vec3(0.0f), "");
		m_Hint = Overlay::MakeText(scene, m_Font, "Hint", HUD_HINT_SIZE, COLOR_TEXT_DIM, glm::vec3(0.0f), "");

		m_Fade = scene.CreateEntity("Fade");
		m_Fade.AddComponent<SpriteRendererComponent>().Color = glm::vec4(glm::vec3(COLOR_FADE), 0.0f);
	}

	Hud::~Hud() = default;

	Hud::Bar Hud::MakeBar(Scene& scene, const char* name, const glm::vec4& fill)
	{
		Bar bar;
		bar.Back = scene.CreateEntity(std::format("{} bar", name));
		bar.Back.AddComponent<SpriteRendererComponent>().Color = COLOR_BAR_BACK;
		bar.Trail = scene.CreateEntity(std::format("{} trail", name));
		bar.Trail.AddComponent<SpriteRendererComponent>().Color = COLOR_BAR_TRAIL;
		bar.Fill = scene.CreateEntity(std::format("{} fill", name));
		bar.Fill.AddComponent<SpriteRendererComponent>().Color = fill;
		bar.Name = Overlay::MakeText(scene, m_Font, std::format("{} name", name).c_str(), HUD_NAME_SIZE, COLOR_TEXT, glm::vec3(0.0f), "");
		return bar;
	}

	void Hud::SetLine(Entity entity, std::string_view text, float size, float y, const glm::vec4& color) const
	{
		auto& component = entity.GetComponent<TextComponent>();
		if (component.Text != text)
			component.Text = text;
		component.Size = size;
		component.Color = WithAlpha(color, m_TextVisible);
		entity.GetComponent<TransformComponent>().Position = { 0.0f, y, 0.0f };
	}

	void Hud::UpdateBar(Bar& bar, float deltaTime, bool playerSide, float centerX, float width, float y, float fraction, std::string_view label)
	{
		if (fraction >= bar.TrailFraction)
		{
			bar.TrailFraction = fraction;
			bar.TrailDelay = 0.0f;
		}
		else if (fraction < bar.LastFraction)
		{
			bar.TrailDelay = HUD_TRAIL_DELAY;
		}
		else if (bar.TrailDelay > 0.0f)
		{
			bar.TrailDelay -= deltaTime;
		}
		else
		{
			bar.TrailFraction = std::max(fraction, bar.TrailFraction - HUD_TRAIL_RATE * deltaTime);
		}
		bar.LastFraction = fraction;

		const float innerWidth = width - 2.0f * HUD_BAR_INSET;
		const float innerHeight = HUD_BAR_HEIGHT - 2.0f * HUD_BAR_INSET;
		const float direction = playerSide ? -1.0f : 1.0f;
		const float edge = centerX - direction * (0.5f * width - HUD_BAR_INSET);

		auto& back = bar.Back.GetComponent<TransformComponent>();
		back.Position = { centerX, y, 0.0f };
		back.Size = { width, HUD_BAR_HEIGHT };

		auto place = [&](Entity entity, float amount, float lift)
		{
			auto& transform = entity.GetComponent<TransformComponent>();
			const float length = innerWidth * amount;
			transform.Position = { edge + direction * 0.5f * length, y, lift };
			transform.Size = { length, innerHeight };
		};
		place(bar.Trail, bar.TrailFraction, HUD_TRAIL_LIFT);
		place(bar.Fill, fraction, HUD_FILL_LIFT);

		SetLine(bar.Name, label, HUD_NAME_SIZE, y + (HUD_BAR_DROP - HUD_NAME_DROP), COLOR_TEXT);
		bar.Name.GetComponent<TransformComponent>().Position.x = centerX;
	}

	void Hud::UpdateBanner(const Fighter& opponent, const BoutFlow& flow)
	{
		const FighterDef& def = opponent.GetDef();
		const BoutRules& rules = flow.GetRules();
		const float intro = rules.Frozen ? 1.0f : flow.GetPhaseTime() / HUD_BANNER_FADE;

		std::string_view big;
		std::string_view title;
		std::string_view note;
		float bigSize = HUD_BANNER_BIG_SIZE;
		float bigY = HUD_BANNER_BIG_Y;
		glm::vec4 bigColor = COLOR_TITLE;
		glm::vec4 titleColor = COLOR_TEXT;
		float alpha = 1.0f;

		switch (flow.GetPhase())
		{
			case BoutPhase::Intro:
				big = m_IntroText;
				title = def.Title ? def.Title : def.Name;
				note = def.Name;
				alpha = intro;
				break;

			case BoutPhase::Fight:
				if (flow.GetFightTime() < BOUT_FIGHT_BANNER_SECONDS)
				{
					big = "FIGHT";
					bigSize = HUD_FIGHT_SIZE;
					bigY = HUD_FIGHT_Y;
					alpha = 2.0f * (1.0f - flow.GetFightTime() / BOUT_FIGHT_BANNER_SECONDS);
				}
				break;

			case BoutPhase::Knockout:
				big = "K.O.";
				bigSize = HUD_KO_SIZE;
				bigY = HUD_KO_Y;
				bigColor = COLOR_TEXT_ALERT;
				alpha = intro;
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
				break;
		}

		SetLine(m_BannerBig, big, bigSize, bigY, WithAlpha(bigColor, alpha));
		SetLine(m_BannerTitle, title, HUD_BANNER_TITLE_SIZE, HUD_BANNER_TITLE_Y, WithAlpha(titleColor, alpha));
		SetLine(m_BannerSmall, note, HUD_BANNER_SMALL_SIZE, HUD_BANNER_SMALL_Y, WithAlpha(COLOR_TEXT_DIM, alpha));
	}

	void Hud::UpdateHint(float deltaTime, float halfHeight)
	{
		m_HintClock += deltaTime;
		const float left = HUD_HINT_SECONDS - m_HintClock;
		const bool visible = m_ShowHint && left > 0.0f;
		const char* text = Input::IsGamepadConnected() ? HUD_HINT_GAMEPAD : HUD_HINT_KEYBOARD;
		SetLine(m_Hint, visible ? std::string_view(text) : std::string_view(), HUD_HINT_SIZE, -halfHeight + HUD_HINT_RISE, WithAlpha(COLOR_TEXT_DIM, left / HUD_HINT_FADE));
	}

	void Hud::Update(float deltaTime, const Fighter& player, const Fighter& opponent, const BoutFlow& flow)
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		const float aspect = viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;
		const float halfHeight = 0.5f * HUD_ORTHO_SIZE;
		const float halfWidth = halfHeight * aspect;

		m_FadeInLeft = std::max(m_FadeInLeft - deltaTime, 0.0f);
		float fade = m_FadeInLeft / HUD_FADE_IN_SECONDS;
		const BoutRules& rules = flow.GetRules();
		if (flow.GetPhase() == BoutPhase::Knockout && !rules.Frozen && rules.KnockoutSeconds > HUD_FADE_OUT_SECONDS)
		{
			const float remaining = rules.KnockoutSeconds - flow.GetPhaseTime();
			fade = std::max(fade, std::clamp(1.0f - remaining / HUD_FADE_OUT_SECONDS, 0.0f, 1.0f));
		}
		m_TextVisible = 1.0f - std::clamp(fade, 0.0f, 1.0f);
		auto& cover = m_Fade.GetComponent<TransformComponent>();
		cover.Position = { 0.0f, 0.0f, HUD_FADE_Z };
		cover.Size = { 2.0f * halfWidth + HUD_FADE_MARGIN, 2.0f * halfHeight + HUD_FADE_MARGIN };
		m_Fade.GetComponent<SpriteRendererComponent>().Color = WithAlpha(COLOR_FADE, fade);

		const float gapHalf = 0.5f * HUD_BAR_CENTER_GAP;
		const float width = std::min(HUD_BAR_WIDTH, halfWidth - HUD_PADDING - gapHalf);
		const float y = halfHeight - HUD_BAR_DROP;
		UpdateBar(m_PlayerBar, deltaTime, true, -(gapHalf + 0.5f * width), width, y, Fraction(player), player.GetDef().Name);
		UpdateBar(m_OpponentBar, deltaTime, false, gapHalf + 0.5f * width, width, y, Fraction(opponent), opponent.GetDef().Name);

		SetLine(m_BoutLabel, m_BoutLabelText, HUD_BOUT_LABEL_SIZE, halfHeight - HUD_BOUT_LABEL_DROP, COLOR_TEXT_DIM);
		UpdateBanner(opponent, flow);
		UpdateHint(deltaTime, halfHeight);
	}

}
