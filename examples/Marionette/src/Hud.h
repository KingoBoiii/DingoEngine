#pragma once
#include <DingoEngine.h>

#include <string>
#include <string_view>

namespace Dingo
{

	class BoutFlow;
	class Fighter;
	class GameAssets;

	// Where the lowest thing in the HUD's top band, the health bars, ends on the screen, in normalized device y (1 is
	// the top edge). The arena camera keeps the fighters below it.
	float GetHudBottomNdc();

	// Health bars, the bout banner and the controls hint, drawn through a second, orthographic camera in
	// the arena scene over the 3D world.
	class Hud
	{
	public:
		Hud(Scene& scene, const GameAssets& assets, int bout, bool fadeIn, bool showHint);
		~Hud();

		Hud(const Hud&) = delete;
		Hud& operator=(const Hud&) = delete;

		void Update(float deltaTime, const Fighter& player, const Fighter& opponent, const BoutFlow& flow);

	private:
		struct Bar
		{
			Entity Back;
			Entity Trail;
			Entity Fill;
			Entity Name;
			float TrailFraction = 1.0f;
			float LastFraction = 1.0f;
			float TrailDelay = 0.0f;
		};

		Bar MakeBar(Scene& scene, const char* name, const glm::vec4& fill);
		void UpdateBar(Bar& bar, float deltaTime, bool playerSide, float centerX, float width, float y, float fraction, std::string_view label);
		void SetLine(Entity entity, std::string_view text, float size, float y, const glm::vec4& color) const;
		void UpdateBanner(const Fighter& opponent, const BoutFlow& flow);
		void UpdateHint(float deltaTime, float halfHeight);

	private:
		// The assets own it.
		Font* m_Font = nullptr;
		int m_Bout = 1;
		bool m_ShowHint = false;
		float m_FadeInLeft = 0.0f;
		float m_HintClock = 0.0f;
		// The fade quad is a sprite and text is drawn over every sprite, so the text fades by hand.
		float m_TextVisible = 1.0f;
		std::string m_BoutLabelText;
		std::string m_IntroText;
		std::string m_NextText;
		Bar m_PlayerBar;
		Bar m_OpponentBar;
		Entity m_Fade;
		Entity m_BoutLabel;
		Entity m_BannerBig;
		Entity m_BannerTitle;
		Entity m_BannerSmall;
		Entity m_Hint;
	};

}
