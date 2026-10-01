#pragma once
#include "BrazierPrompt.h"
#include "Overlay.h"

#include <DingoEngine.h>

#include <string>

namespace Dingo
{

	class KeepMap;
	class Lantern;

	class Hud
	{
	public:
		Hud(Scene& scene, const KeepMap& map, const Lantern& lantern, int startRoom);
		~Hud();

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
		void Update(float deltaTime, const Lantern& lantern);

	private:
		const KeepMap& m_Map;
		Font* m_Font = nullptr;
		Entity m_Fade;
		Entity m_RoomLabel;
		Entity m_OilBackground;
		Entity m_OilFill;
		Entity m_LanternState;
		Entity m_KeyHint;
		Entity m_Prompt;
		Entity m_ProgressBackground;
		Entity m_ProgressFill;
		Entity m_Toast;
		Entity m_PauseDim;
		Entity m_PauseTitle;
		Entity m_PauseHint;
		int m_Room = -1;
		BrazierPrompt m_PromptKind = BrazierPrompt::None;
		float m_Progress = 0.0f;
		std::string m_ToastText;
		float m_ToastTime = 0.0f;
		bool m_Paused = false;
		Overlay::PadText m_SnuffHint;
		Overlay::PadText m_LightBrazierPrompt;
		Overlay::PadText m_LightAltarPrompt;
		Overlay::PadText m_RefuelPrompt;
		Overlay::PadText m_PauseHintText;
	};

}
