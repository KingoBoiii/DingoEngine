#include "depch.h"
#include "DingoUIContext.h"
#include "DingoUIBatcher.h"

#include "DingoEngine/Core/InputBackend.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/FullscreenPass.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Texture.h"

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

		m_Measure = [this](const Font* font, std::string_view text, float size, float maxWidth) -> glm::vec2
		{
			if (!font || !font->IsValid() || text.empty())
				return glm::vec2(0.0f);
			m_MeasureScratch.assign(text);
			const TextMetrics metrics = font->MeasureText(m_MeasureScratch, size, maxWidth);
			return { metrics.Width, metrics.Height };
		};
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

		m_TargetSize = ComputeTargetSize();
		m_WindowToTarget = glm::vec2(1.0f);
		if (m_Input.ViewportSize.x > 0.0f && m_Input.ViewportSize.y > 0.0f && m_TargetSize.x > 0.0f && m_TargetSize.y > 0.0f)
			m_WindowToTarget = m_TargetSize / m_Input.ViewportSize;
		m_Input.PointerPosition *= m_WindowToTarget;
		m_Input.ViewportSize = m_TargetSize;

		m_PointerOverUI = m_Input.PointerAvailable && HitTestBlocking(m_Input.PointerPosition);
		m_CapturingInput = false;

		m_Tree.Begin(m_TargetSize);
		m_InFrame = true;
	}

	glm::vec2 DingoUIContext::ComputeTargetSize() const
	{
		const Framebuffer* target = m_Target ? m_Target : Renderer::GetSwapChainFramebuffer();
		if (!target)
			return glm::vec2(0.0f);
		return { static_cast<float>(target->GetWidth()), static_cast<float>(target->GetHeight()) };
	}

	bool DingoUIContext::CanDeclare()
	{
		if (m_InFrame)
			return true;
		if (!m_WarnedOutsideFrame)
		{
			m_WarnedOutsideFrame = true;
			DE_CORE_WARN("DingoUI: elements can only be declared from a layer's OnUpdate; the call was ignored");
		}
		return false;
	}

	void DingoUIContext::WarnMismatch(const char* what)
	{
		if (m_WarnedMismatch)
			return;
		m_WarnedMismatch = true;
		DE_CORE_WARN("DingoUI: {}", what);
	}

	bool DingoUIContext::BeginRoot(std::string_view name, const UIRootParams& params)
	{
		if (!CanDeclare())
			return false;
		if (!m_Tree.BeginRoot(name, params))
		{
			WarnMismatch("BeginRoot inside another root or box; the call was ignored (a missing EndRoot or EndBox?)");
			return false;
		}
		return true;
	}

	void DingoUIContext::EndRoot()
	{
		if (m_InFrame && !m_Tree.EndRoot())
			WarnMismatch("EndRoot without a matching BeginRoot, or with a box still open; the call was ignored");
	}

	bool DingoUIContext::BeginBox(const UIBoxParams& params)
	{
		if (!CanDeclare())
			return false;
		if (!m_Tree.BeginBox(params))
		{
			WarnMismatch("BeginBox outside a root; the call was ignored");
			return false;
		}
		return true;
	}

	void DingoUIContext::EndBox()
	{
		if (m_InFrame && !m_Tree.EndBox())
			WarnMismatch("EndBox without a matching BeginBox; the call was ignored");
	}

	void DingoUIContext::Text(std::string_view text, const UITextParams& params)
	{
		if (!CanDeclare())
			return;

		const Font* font = params.TextFont ? params.TextFont : m_Params.DefaultFont;
		if (!font || !font->IsValid())
		{
			if (!m_WarnedNoFont)
			{
				m_WarnedNoFont = true;
				DE_CORE_WARN("DingoUI: Text has no usable font (set DingoUIParams::DefaultFont, DingoUI::SetDefaultFont or UITextParams::TextFont); text is skipped");
			}
			return;
		}
		if (!m_Tree.AddText(text, params, font))
			WarnMismatch("Text outside a root; the call was ignored");
	}

	void DingoUIContext::Image(Texture* texture, const UIImageParams& params)
	{
		if (!CanDeclare())
			return;

		const glm::vec2 size = texture ? glm::vec2(static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())) : glm::vec2(0.0f);
		if (!m_Tree.AddImage(texture, size, params))
			WarnMismatch("Image outside a root; the call was ignored");
	}

	void DingoUIContext::EndFrame(bool render)
	{
		DE_PROFILE_FUNCTION();

		m_InFrame = false;
		if (m_Tree.CloseAll() > 0)
			WarnMismatch("a Begin call was never closed; the frame's elements were closed for you");

		m_Tree.Layout(m_Measure);
		m_Tree.CollectBlockingRects(m_BlockingRects);

		m_RenderStatistics = {};
		const bool draw = render && !Renderer::IsFrameSkipped();
		if (draw)
			m_Tree.Emit(m_DrawList);
		std::swap(m_Tree, m_LastTree);

		if (draw && !m_DrawList.IsEmpty())
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
