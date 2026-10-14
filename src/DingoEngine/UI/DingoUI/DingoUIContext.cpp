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

		ResolveInteraction();

		m_Tree.Begin(m_TargetSize);
		m_ScrollStack.clear();
		m_InFrame = true;
	}

	void DingoUIContext::ResolveInteraction()
	{
		const UIInputState& input = m_Input;
		const bool available = input.PointerAvailable;

		m_Hit = available ? m_LastTree.HitTest(input.PointerPosition) : UIHit{};
		m_PointerOverUI = available && (m_ModalActive || m_Hit.OverBlocking);

		m_PressedId = 0;
		m_ClickedId = 0;
		m_ReleasedId = 0;

		if (available && input.PointerPressed[0] && m_ActiveId == 0 && m_Hit.HoverId != 0)
		{
			m_ActiveId = m_Hit.HoverId;
			m_PressedId = m_ActiveId;
		}

		if (m_ActiveId != 0 && (input.PointerReleased[0] || !input.PointerDown[0]))
		{
			m_ReleasedId = m_ActiveId;
			if (available && m_Hit.HoverId == m_ActiveId)
				m_ClickedId = m_ActiveId;
			m_ActiveId = 0;
		}

		m_HoverId = available && (m_ActiveId == 0 || m_Hit.HoverId == m_ActiveId) ? m_Hit.HoverId : 0;

		m_ScrollDelta = input.Scroll;
		m_ScrollTargetId = available ? m_Hit.ScrollId : 0;
		if (m_PointerOverUI && (input.Scroll.x != 0.0f || input.Scroll.y != 0.0f))
			InputBackend::ConsumeScroll();

		bool anyConsumed = false;
		for (size_t button = 0; button < UIInputState::k_PointerButtonCount; ++button)
		{
			if (!input.PointerDown[button] && !input.PointerPressed[button])
				m_ConsumedDown[button] = false;
			if (m_PointerOverUI && input.PointerPressed[button])
			{
				InputBackend::ConsumeMouseButton(static_cast<MouseButton>(button));
				m_ConsumedDown[button] = true;
			}
			anyConsumed = anyConsumed || m_ConsumedDown[button];
		}

		m_CapturingInput = m_ModalActive || anyConsumed;
	}

	UIInteraction DingoUIContext::Interact(uint64_t id, std::string_view label, bool enabled, UIWidgetState*& state)
	{
		const UIStateMap::TouchResult touch = m_States.Touch(id, m_FrameIndex);
		state = touch.State;
		if (touch.DeclaredAlready && m_WarnedCollisions.insert(id).second)
		{
			++m_IdCollisionCount;
#if defined(DE_DEBUG)
			DE_CORE_WARN("DingoUI: two widgets in one frame share the id of \"{}\"; add a ##suffix to one label or wrap them in PushId/PopId", label);
#else
			(void)label;
#endif
		}

		UIInteraction result;
		if (enabled)
		{
			result.Hovered = id == m_HoverId;
			result.Pressed = id == m_PressedId;
			result.Down = id == m_ActiveId;
			result.Clicked = id == m_ClickedId;
		}
		result.Focused = state->Focused;
		state->Hovered = result.Hovered;
		state->Active = result.Down;
		return result;
	}

	uint64_t DingoUIContext::WidgetId(std::string_view label)
	{
		if (!CanDeclare())
			return 0;
		const uint64_t id = m_Tree.MakeId(label);
		if (id == 0)
			WarnMismatch("a widget outside a root; the call was ignored");
		return id;
	}

	void DingoUIContext::BeginWidgetBox(uint64_t id, std::string_view label, const UIBoxParams& box, bool interactive, bool scroll, const glm::vec2& scrollOffset)
	{
		UIWidgetInfo info;
		info.Id = id;
		info.NameHash = UILayoutTree::HashName(label);
		info.Interactive = interactive;
		info.Scroll = scroll;
		info.ScrollOffset = scrollOffset;
		m_Tree.BeginBox(box, &info);
	}

	void DingoUIContext::PushId(std::string_view id)
	{
		if (m_InFrame)
			m_Tree.PushId(id);
	}

	void DingoUIContext::PushId(int id)
	{
		if (m_InFrame)
			m_Tree.PushId(id);
	}

	void DingoUIContext::PopId()
	{
		if (m_InFrame && !m_Tree.PopId())
			WarnMismatch("PopId without a PushId in this container; the call was ignored");
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

		for (const UIElement& element : m_Tree.GetElements())
		{
			if (!element.Interactive && !element.Scroll)
				continue;
			if (UIWidgetState* state = m_States.Find(element.Id))
			{
				state->LastRect = element.Rect;
				if (element.Scroll)
					state->ContentSize = element.ContentSize;
			}
		}

		m_RenderStatistics = {};
		const bool draw = render && !Renderer::IsFrameSkipped();
		if (draw)
			m_Tree.Emit(m_DrawList);
		std::swap(m_Tree, m_LastTree);
		m_ModalActive = m_LastTree.HasModal();

		if (draw && !m_DrawList.IsEmpty())
		{
			Framebuffer* target = m_Target ? m_Target : Renderer::GetSwapChainFramebuffer();
			RenderTargetScope scope(m_Target);
			m_Batcher->Draw(m_DrawList, target, m_RenderStatistics);
		}

		m_DrawList.Clear();
		m_States.Evict(m_FrameIndex);
		++m_FrameIndex;
	}

	bool DingoUIContext::HitTestBlocking(const glm::vec2& point) const
	{
		return m_ModalActive || m_LastTree.HitTest(point).OverBlocking;
	}

}
