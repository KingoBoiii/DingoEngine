#include "depch.h"
#include "DingoUIContext.h"
#include "DingoUIBatcher.h"

#include "DingoEngine/Core/InputBackend.h"
#include "DingoEngine/Graphics/FullscreenPass.h"
#include "DingoEngine/Graphics/Renderer.h"

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
		: m_Params(params), m_Batcher(std::make_unique<DingoUIBatcher>())
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

		m_RenderStatistics = {};
		if (render && !Renderer::IsFrameSkipped() && !m_DrawList.IsEmpty())
		{
			Framebuffer* target = m_Target ? m_Target : Renderer::GetSwapChainFramebuffer();
			RenderTargetScope scope(m_Target);
			m_Batcher->Draw(m_DrawList, target, m_RenderStatistics);
		}

		m_DrawList.Clear();
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
