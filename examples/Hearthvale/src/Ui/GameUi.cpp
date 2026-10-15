#include "Ui/GameUi.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_FontPath = "fonts/Inter-Bold.ttf";
	}

	GameUi::GameUi(ValleyDirector& director, Inventory& inventory, BuildController& controller, ScreenHost& host)
		: m_Director(director), m_Inventory(inventory), m_Controller(controller), m_Host(host)
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		Font* font = assets.GetFont(assets.Load(k_FontPath));
		if (!font)
		{
			DE_ERROR("Hearthvale: failed to load font '{}'", k_FontPath);
			return;
		}
		m_Kit = MakeKit(font);
		DingoUI::SetDefaultFont(font);
		DingoUI::SetTheme(m_Kit.Theme);
		m_SeenControllerSerial = m_Controller.GetMessageSerial();
		m_SeenHostSerial = m_Host.GetToastSerial();
	}

	GameUi::~GameUi()
	{
		if (m_Kit.TextFont && DingoUI::GetDefaultFont() == m_Kit.TextFont)
			DingoUI::SetDefaultFont(nullptr);
	}

	void GameUi::UpdateToast(float deltaTime)
	{
		if (m_Controller.GetMessageSerial() != m_SeenControllerSerial)
		{
			m_SeenControllerSerial = m_Controller.GetMessageSerial();
			m_ToastText = m_Controller.GetMessage();
			m_ToastLeft = UI_TOAST_SECONDS;
		}
		if (m_Host.GetToastSerial() != m_SeenHostSerial)
		{
			m_SeenHostSerial = m_Host.GetToastSerial();
			m_ToastText = m_Host.GetToast();
			m_ToastLeft = UI_TOAST_SECONDS;
		}
		m_ToastLeft = (std::max)(0.0f, m_ToastLeft - deltaTime);
	}

	void GameUi::Draw(float deltaTime)
	{
		if (!IsUsable())
			return;

		UpdateToast(deltaTime);
		const HudContent content = BuildHudContent(m_Director.GetCalendar(), m_Inventory, m_Controller);
		m_Hud.Draw(m_Kit, content, m_ToastLeft > 0.0f ? m_ToastText : std::string(), !m_Host.IsOpen());

		const std::string focusRequest = m_Host.TakeFocusRequest();
		switch (m_Host.Top())
		{
		case ScreenId::Inventory:
			DrawInventoryScreen(m_Kit, m_Host, m_Inventory, focusRequest);
			break;
		case ScreenId::BuildPalette:
			DrawBuildPaletteScreen(m_Kit, m_Host, m_Inventory, m_Controller, focusRequest);
			break;
		case ScreenId::None:
			break;
		}
	}

}
