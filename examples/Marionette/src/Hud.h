#pragma once
#include <DingoEngine.h>

#include <string>
#include <string_view>

namespace Dingo
{

	class BoutFlow;
	class Fighter;

	// Where the lowest thing in the HUD's top band, the health bars, ends on the screen, in normalized device y (1 is
	// the top edge). The arena camera keeps the fighters below it.
	float GetHudBottomNdc();

	// Health bars, the bout banner, the controls hint and the fades, declared once per rendered frame through DingoUI
	// over the 3D world, plus a small bar floating over each fighter's head. Update runs once per simulation step and
	// only moves the clocks; Declare is the only call that touches DingoUI.
	class Hud
	{
	public:
		Hud(Scene& scene, int bout, bool fadeIn, bool showHint);
		~Hud();

		Hud(const Hud&) = delete;
		Hud& operator=(const Hud&) = delete;

		void Update(float deltaTime, const Fighter& player, const Fighter& opponent);
		// From a layer's OnUpdate, once per frame, after the last step.
		void Declare(const Fighter& player, const Fighter& opponent, const BoutFlow& flow);

	private:
		// The pale part of a bar: what the last hit took, held for a moment and then drained.
		struct Trail
		{
			float Fraction = 1.0f;
			float Last = 1.0f;
			float Delay = 0.0f;
		};

		static void UpdateTrail(Trail& trail, float deltaTime, float fraction);
		void DeclareWorldBar(const char* id, const Fighter& fighter, const Trail& trail, float fraction, const glm::vec4& fill) const;
		void DeclareBanner(const BoutFlow& flow, const Fighter& opponent) const;
		void DeclareHint();
		void DeclareCovers(const BoutFlow& flow);

	private:
		Scene* m_Scene;
		int m_Bout = 1;
		bool m_ShowHint = false;
		bool m_FadeIn = false;
		float m_HintClock = 0.0f;
		std::string m_BoutLabelText;
		std::string m_IntroText;
		std::string m_NextText;
		Trail m_PlayerTrail;
		Trail m_OpponentTrail;
	};

}
