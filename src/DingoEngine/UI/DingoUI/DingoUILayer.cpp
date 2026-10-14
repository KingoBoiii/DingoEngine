#include "depch.h"
#include "DingoUILayer.h"
#include "DingoUIContext.h"

#include "DingoEngine/Core/Input.h"

namespace Dingo::Internal
{

	void DingoUILayer::OnEvent(Event& e)
	{
		const EventType type = e.GetEventType();
		if (type != EventType::MouseButtonPressed && type != EventType::MouseScrolled)
			return;

		// An input override measures the pointer in its own space, not the window's.
		if (m_Context.HasInputOverride() || Input::GetCursorMode() == CursorMode::Locked)
			return;

		if (m_Context.HitTestBlocking(Input::GetMousePosition()))
			e.Handled = true;
	}

}
