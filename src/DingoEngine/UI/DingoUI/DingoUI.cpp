#include "depch.h"
#include "DingoEngine/UI/DingoUI.h"

#include "DingoUIContext.h"

namespace Dingo
{

	bool DingoUI::IsPointerOverUI()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->IsPointerOverUI();
	}

	bool DingoUI::IsCapturingInput()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->IsCapturingInput();
	}

}
