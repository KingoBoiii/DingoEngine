#include "depch.h"
#include "DingoUIContext.h"

#include "DingoEngine/Core/InputBackend.h"

namespace Dingo::Internal
{

	namespace
	{
		DingoUIContext* s_Context = nullptr;
	}

	DingoUIContext* GetDingoUIContext()
	{
		return s_Context;
	}

	DingoUIContext::DingoUIContext(const DingoUIParams& params)
		: m_Params(params)
	{
		DE_CORE_ASSERT(!s_Context, "Only one DingoUIContext may exist at a time");
		s_Context = this;
	}

	DingoUIContext::~DingoUIContext()
	{
		InputBackend::ClearConsumed();
		if (s_Context == this)
			s_Context = nullptr;
	}

	void DingoUIContext::BeginFrame(const UIInputState& input)
	{
		DE_PROFILE_FUNCTION();

		m_Input = m_InputOverride ? *m_InputOverride : input;
		m_PointerOverUI = m_Input.PointerAvailable && HitTestBlocking(m_Input.PointerPosition);
		m_CapturingInput = false;
	}

	void DingoUIContext::EndFrame(bool render)
	{
		DE_PROFILE_FUNCTION();

		(void)render;
		++m_FrameIndex;
	}

	bool DingoUIContext::HitTestBlocking(const glm::vec2& point) const
	{
		for (const UIRect& rect : m_BlockingRects)
		{
			if (rect.Contains(point))
				return true;
		}
		return false;
	}

}
