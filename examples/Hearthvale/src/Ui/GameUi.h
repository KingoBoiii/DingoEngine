#pragma once
#include "Ui/Hud.h"
#include "Ui/ScreenHost.h"
#include "Ui/Screens.h"
#include "Ui/UiKit.h"

namespace Dingo
{

	// The UI font file, relative to the asset root.
	const char* GameUiFontPath();
	const char* GameUiHeadingFontPath();

	// Owns the UI font and theme and declares the HUD and the open screens each frame. GPU-side: build it only when
	// the application has a renderer.
	class GameUi
	{
	public:
		GameUi(ValleyDirector& director, Inventory& inventory, BuildController& controller, ScreenHost& host);
		~GameUi();
		GameUi(const GameUi&) = delete;
		GameUi& operator=(const GameUi&) = delete;

		bool IsUsable() const { return m_Kit.TextFont != nullptr; }
		const UiKit& GetKit() const { return m_Kit; }
		const std::string& GetToastText() const { return m_ToastText; }
		float GetToastLeft() const { return m_ToastLeft; }

		void Draw(float deltaTime);

	private:
		void UpdateToast(float deltaTime);

	private:
		ValleyDirector& m_Director;
		Inventory& m_Inventory;
		BuildController& m_Controller;
		ScreenHost& m_Host;
		UiKit m_Kit;
		Hud m_Hud;
		std::string m_ToastText;
		float m_ToastLeft = 0.0f;
		uint32_t m_SeenControllerSerial = 0;
		uint32_t m_SeenHostSerial = 0;
	};

}
