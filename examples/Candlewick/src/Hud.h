#pragma once
#include "BrazierPrompt.h"
#include "Overlay.h"

#include <DingoEngine.h>

#include <string>

namespace Dingo
{

	class KeepMap;
	class Lantern;

	enum class PauseChoice { None, Resume, Title };

	class Hud
	{
	public:
		Hud(const KeepMap& map, const Lantern& lantern, int startRoom);

		Hud(const Hud&) = delete;
		Hud& operator=(const Hud&) = delete;

		// Corridors (-1) keep the last room's name.
		void SetRoom(int room);
		// 0 = clear, 1 = black. Covers the world and the meter; text stays on top.
		void SetFade(float amount);
		// Progress is the hold's fill, 0 to 1; the bar shows only while a hold can begin.
		void SetBrazierPrompt(BrazierPrompt prompt, float progress);
		void ShowToast(const std::string& text);
		void SetPaused(bool paused);
		// Declares the whole HUD and, while paused, the pause menu; call it once a frame, paused or not.
		void Update(float deltaTime, const Lantern& lantern);
		// What the pause menu's buttons asked for in the last Update, once.
		PauseChoice TakePauseChoice();

	private:
		void DeclarePause();

	private:
		const KeepMap& m_Map;
		int m_Room = -1;
		float m_Fade = 0.0f;
		BrazierPrompt m_PromptKind = BrazierPrompt::None;
		float m_Progress = 0.0f;
		std::string m_ToastText;
		float m_ToastTime = 0.0f;
		bool m_Paused = false;
		PauseChoice m_PauseChoice = PauseChoice::None;
		Overlay::PadText m_SnuffHint;
		Overlay::PadText m_LightBrazierPrompt;
		Overlay::PadText m_LightAltarPrompt;
		Overlay::PadText m_RefuelPrompt;
		Overlay::PadText m_PauseHintText;
	};

}
