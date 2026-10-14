#include "depch.h"
#include "DingoUIContext.h"
#include "DingoUIWorld.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoUIBatcher.h"
#include "DingoUITextureHook.h"

#include "DingoEngine/Core/InputBackend.h"
#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/FullscreenPass.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Texture.h"

#include <algorithm>
#include <limits>

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

	void DingoUIOnTextureDestroyed(const Texture* texture)
	{
		if (s_Context && texture)
			s_Context->OnTextureDestroyed(texture);
	}

	DingoUIContext::DingoUIContext(const DingoUIParams& params)
		: m_Params(params), m_Batcher(std::make_unique<DingoUIBatcher>())
	{
		m_StyleStack.push_back(params.Style);
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
		DE_PROFILE_SCOPE("DingoUI::BeginFrame");
		const UICpuTimer timer;

		m_Input = m_InputOverride ? *m_InputOverride : input;

		m_TargetSize = ComputeTargetSize();
		m_WindowToTarget = glm::vec2(1.0f);
		if (m_Input.ViewportSize.x > 0.0f && m_Input.ViewportSize.y > 0.0f && m_TargetSize.x > 0.0f && m_TargetSize.y > 0.0f)
			m_WindowToTarget = m_TargetSize / m_Input.ViewportSize;
		m_Input.PointerPosition *= m_WindowToTarget;
		m_Input.ViewportSize = m_TargetSize;

		ResolveInteraction();
		ResolveNavigation();

		m_Tree.Begin(m_TargetSize);
		m_ScrollStack.clear();
		m_InFrame = true;
		m_FrameCosts.BeginMs = timer.ElapsedMs();
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

	void DingoUIContext::SetFocus(std::string_view label)
	{
		if (!m_InFrame)
			return;
		const uint64_t id = m_Tree.MakeId(label);
		if (id == 0)
			return;
		m_PendingFocusId = id;
		m_PendingFocusFrames = 3;
	}

	void DingoUIContext::CollectNavCandidates()
	{
		m_NavCandidates.clear();
		m_NavElements.clear();
		m_NavRoots.clear();
		if (!m_Params.Nav.Enabled)
			return;

		const std::vector<UIElement>& elements = m_LastTree.GetElements();
		m_LastTree.GetRootOrder(m_RootOrder);
		int32_t modal = k_UINoElement;
		for (auto it = m_RootOrder.rbegin(); it != m_RootOrder.rend() && modal == k_UINoElement; ++it)
		{
			if (elements[*it].Modal)
				modal = *it;
		}

		int32_t root = k_UINoElement;
		for (int32_t i = 0; i < static_cast<int32_t>(elements.size()); ++i)
		{
			const UIElement& element = elements[i];
			if (element.Kind == UIElementKind::Root)
				root = i;
			if (!element.Focusable || root == k_UINoElement || element.Rect.IsEmpty())
				continue;
			const UIElement& owner = elements[root];
			if (!owner.Navigable || !owner.BlocksInput || (modal != k_UINoElement && root != modal))
				continue;
			m_NavCandidates.push_back({ element.Id, element.Rect });
			m_NavElements.push_back(i);
			m_NavRoots.push_back(root);
		}
	}

	int32_t DingoUIContext::NavIndexOf(uint64_t id) const
	{
		if (id == 0)
			return -1;
		for (int32_t i = 0; i < static_cast<int32_t>(m_NavCandidates.size()); ++i)
		{
			if (m_NavCandidates[i].Id == id)
				return i;
		}
		return -1;
	}

	int32_t DingoUIContext::EstablishFocus() const
	{
		int32_t topRoot = k_UINoElement;
		int32_t topRank = -1;
		for (const int32_t root : m_NavRoots)
		{
			const auto position = std::find(m_RootOrder.begin(), m_RootOrder.end(), root);
			const int32_t rank = static_cast<int32_t>(position - m_RootOrder.begin());
			if (rank > topRank)
			{
				topRank = rank;
				topRoot = root;
			}
		}

		const std::vector<UIElement>& elements = m_LastTree.GetElements();
		const auto remembered = m_RootFocus.find(elements[topRoot].Id);
		int32_t first = -1;
		int32_t preferred = -1;
		for (int32_t i = 0; i < static_cast<int32_t>(m_NavCandidates.size()); ++i)
		{
			if (m_NavRoots[i] != topRoot)
				continue;
			if (first < 0)
				first = i;
			if (remembered != m_RootFocus.end() && m_NavCandidates[i].Id == remembered->second)
				return i;
			if (preferred < 0 && elements[m_NavElements[i]].DefaultFocus)
				preferred = i;
		}
		return preferred >= 0 ? preferred : first;
	}

	void DingoUIContext::EnsureFocusVisible(int32_t candidate)
	{
		const std::vector<UIElement>& elements = m_LastTree.GetElements();
		const UIRect& rect = m_NavCandidates[candidate].Rect;
		for (int32_t parent = elements[m_NavElements[candidate]].Parent; parent != k_UINoElement; parent = elements[parent].Parent)
		{
			const UIElement& area = elements[parent];
			if (!area.Scroll)
				continue;
			UIWidgetState* state = m_States.Find(area.Id);
			if (!state)
				continue;
			if (rect.Min.y < area.Rect.Min.y)
				state->ScrollOffset.y -= area.Rect.Min.y - rect.Min.y;
			else if (rect.Max.y > area.Rect.Max.y)
				state->ScrollOffset.y += rect.Max.y - area.Rect.Max.y;
		}
	}

	void DingoUIContext::ResolveNavigation()
	{
		const UIInputState& input = m_Input;
		const UINavParams& nav = m_Params.Nav;
		m_NavAdjust = 0;
		m_NavAcceptId = 0;
		m_BackPressed = false;

		const bool pointerPressed = input.PointerPressed[0] || input.PointerPressed[1] || input.PointerPressed[2];
		if (input.PointerAvailable && ((m_HadPointer && input.PointerPosition != m_LastPointer) || pointerPressed))
			m_FocusVisible = false;
		m_HadPointer = input.PointerAvailable;
		m_LastPointer = input.PointerPosition;

		CollectNavCandidates();
		const std::vector<UIElement>& elements = m_LastTree.GetElements();
		if (m_NavCandidates.empty())
		{
			m_FocusId = 0;
			m_SeenDefault.clear();
			m_DirectionHeld = {};
			for (UINavRepeater& repeater : m_Repeaters)
				repeater.Step(false, 0.0f, 0.0f, 0.0f);
			return;
		}

		int32_t focus = NavIndexOf(m_FocusId);
		bool forced = false;

		if (m_PressedId != 0)
		{
			const int32_t pressed = NavIndexOf(m_PressedId);
			if (pressed >= 0)
			{
				focus = pressed;
				forced = true;
			}
		}
		if (m_PendingFocusId != 0)
		{
			const int32_t pending = NavIndexOf(m_PendingFocusId);
			if (pending >= 0)
			{
				focus = pending;
				forced = true;
				m_PendingFocusId = 0;
			}
			else if (--m_PendingFocusFrames == 0)
				m_PendingFocusId = 0;
		}

		if (focus < 0)
		{
			const bool hadFocus = m_FocusId != 0;
			int32_t restored = NavIndexOf(m_SavedFocusId);
			if (restored >= 0)
				m_SavedFocusId = 0;
			else if (hadFocus && m_SavedFocusId == 0 && m_LastTree.FindById(m_FocusId))
				m_SavedFocusId = m_FocusId;

			if (restored < 0 && hadFocus)
			{
				float best = 0.0f;
				for (int32_t i = 0; i < static_cast<int32_t>(m_NavCandidates.size()); ++i)
				{
					if (elements[m_NavRoots[i]].Id != m_FocusRootId)
						continue;
					const glm::vec2 delta = (m_NavCandidates[i].Rect.Min + m_NavCandidates[i].Rect.Max) * 0.5f - m_FocusCenter;
					const float distance = delta.x * delta.x + delta.y * delta.y;
					if (restored < 0 || distance < best)
					{
						restored = i;
						best = distance;
					}
				}
			}
			focus = restored >= 0 ? restored : EstablishFocus();
		}

		if (!forced)
		{
			std::unordered_set<uint64_t> defaults;
			int32_t fresh = -1;
			for (int32_t i = 0; i < static_cast<int32_t>(m_NavCandidates.size()); ++i)
			{
				if (!elements[m_NavElements[i]].DefaultFocus)
					continue;
				defaults.insert(m_NavCandidates[i].Id);
				if (!m_SeenDefault.contains(m_NavCandidates[i].Id))
					fresh = i;
			}
			for (uint64_t id : m_SeenDefault)
			{
				if (m_LastTree.FindById(id))
					defaults.insert(id);
			}
			m_SeenDefault = std::move(defaults);
			if (fresh >= 0)
				focus = fresh;
		}

		const UINavInput& in = input.Nav;
		std::array<bool, k_UINavDirectionCount> pulse{};
		UINavInput fired;
		fired.Gamepad = in.Gamepad;
		bool moving = false;
		for (size_t direction = 0; direction < k_UINavDirectionCount; ++direction)
		{
			pulse[direction] = m_Repeaters[direction].Step(in.Direction[direction], input.DeltaTime, nav.RepeatDelay, nav.RepeatInterval);
			fired.Direction[direction] = in.Direction[direction] && !m_DirectionHeld[direction];
			m_DirectionHeld[direction] = in.Direction[direction];
			moving = moving || pulse[direction];
		}
		fired.Accept = in.Accept;
		fired.Back = in.Back;
		fired.Next = in.Next;
		fired.Previous = in.Previous;

		m_BackPressed = in.Back;
		if (moving || in.Accept || in.Next || in.Previous)
		{
			if (!m_FocusVisible)
				m_FocusVisible = true;
			else
			{
				if (in.Accept)
					m_NavAcceptId = m_NavCandidates[focus].Id;
				constexpr UINavDirection k_Order[] = { UINavDirection::Up, UINavDirection::Down, UINavDirection::Left, UINavDirection::Right };
				for (const UINavDirection direction : k_Order)
				{
					if (!pulse[static_cast<size_t>(direction)])
						continue;
					const bool sideways = direction == UINavDirection::Left || direction == UINavDirection::Right;
					if (sideways && elements[m_NavElements[focus]].AdjustsHorizontally)
						m_NavAdjust += direction == UINavDirection::Right ? 1 : -1;
					else if (const int32_t target = FindNavTarget(m_NavCandidates, focus, direction); target >= 0)
						focus = target;
				}
				if (in.Next || in.Previous)
					focus = StepNavOrder(static_cast<int32_t>(m_NavCandidates.size()), focus, in.Next);
			}
		}
		if (fired.Any())
			ConsumeNavInput(fired, input.KeyboardAvailable, nav);

		const UINavCandidate& chosen = m_NavCandidates[focus];
		if (chosen.Id != m_FocusId)
			m_EnsureVisibleFrames = 2;
		m_FocusId = chosen.Id;
		m_FocusRootId = elements[m_NavRoots[focus]].Id;
		m_FocusCenter = (chosen.Rect.Min + chosen.Rect.Max) * 0.5f;
		m_RootFocus[m_FocusRootId] = m_FocusId;
		if (m_EnsureVisibleFrames > 0)
		{
			--m_EnsureVisibleFrames;
			EnsureFocusVisible(focus);
		}
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
		result.Focused = enabled && m_FocusVisible && id == m_FocusId;
		if (enabled && id == m_NavAcceptId)
			result.Clicked = true;
		state->Focused = result.Focused;
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

	void DingoUIContext::BeginWidgetBox(uint64_t id, std::string_view label, const UIBoxParams& box, bool interactive, bool scroll, const glm::vec2& scrollOffset, const UIStyle* thumbStyle)
	{
		UIWidgetInfo info;
		info.Id = id;
		info.NameHash = UILayoutTree::HashName(label);
		info.Interactive = interactive;
		info.Scroll = scroll;
		info.Focusable = m_WidgetFocus.Focusable;
		info.DefaultFocus = m_WidgetFocus.DefaultFocus;
		info.AdjustsHorizontally = m_WidgetFocus.AdjustsHorizontally;
		m_WidgetFocus = {};
		info.ScrollOffset = scrollOffset;
		if (scroll)
		{
			const UIStyle& style = Resolve(thumbStyle);
			info.ScrollThumbColor = style.ScrollThumbColor;
			info.ScrollThumbWidth = style.ScrollThumbWidth;
		}
		m_Tree.BeginBox(box, &info);
	}

	bool DingoUIContext::BeginPanel(const UIBoxParams& params)
	{
		if (!CanDeclare())
			return false;

		const UIStyle& style = m_StyleStack.back();
		UIBoxParams box = params;
		if (!box.NineSlice.IsValid())
			box.NineSlice = style.Panel;
		if (box.Background.a <= 0.0f && !box.NineSlice.IsValid())
			box.Background = style.PanelColor;
		if (box.BorderWidth <= 0.0f)
		{
			box.BorderWidth = style.BorderWidth;
			box.BorderColor = style.BorderColor;
		}
		if (box.CornerRadius <= 0.0f)
			box.CornerRadius = style.CornerRadius;
		if (box.Padding == glm::vec4(0.0f))
			box.Padding = glm::vec4(style.PanelPadding);
		return BeginBox(box);
	}

	void DingoUIContext::EndPanel()
	{
		EndBox();
	}

	void DingoUIContext::PushStyle(const UIStyle& style)
	{
		if (m_InFrame)
			m_StyleStack.push_back(style);
	}

	void DingoUIContext::PopStyle()
	{
		if (!m_InFrame)
			return;
		if (m_StyleStack.size() <= 1)
		{
			if (!m_WarnedStyleStack)
			{
				m_WarnedStyleStack = true;
				DE_CORE_WARN("DingoUI: PopStyle without a PushStyle; the call was ignored");
			}
			return;
		}
		m_StyleStack.pop_back();
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

	bool DingoUIContext::BeginWorldRoot(std::string_view name, const glm::mat4& viewProjection, const glm::vec3& worldPosition, const UIWorldRootParams& params)
	{
		if (!CanDeclare())
			return false;
		const UIWorldPlacement placement = ComputeWorldPlacement(viewProjection, worldPosition, m_TargetSize, params);
		if (!placement.Visible)
			return false;
		if (!m_Tree.BeginWorldRoot(name, params, placement.Point, placement.Depth, placement.Scale, placement.Alpha))
		{
			WarnMismatch("BeginWorldRoot inside another root or box; the call was ignored (a missing EndRoot or EndBox?)");
			return false;
		}
		return true;
	}

	bool DingoUIContext::BeginWorldRoot(std::string_view name, Scene& scene, const glm::vec3& worldPosition, const UIWorldRootParams& params)
	{
		if (!CanDeclare() || m_TargetSize.x <= 0.0f || m_TargetSize.y <= 0.0f || !scene.GetPrimaryCamera().IsValid())
			return false;
		return BeginWorldRoot(name, scene.GetActiveCameraViewProjection(m_TargetSize.x / m_TargetSize.y), worldPosition, params);
	}

	bool DingoUIContext::BeginWorldRoot(std::string_view name, Entity entity, const glm::vec3& worldOffset, const UIWorldRootParams& params)
	{
		if (!entity.IsValid())
			return false;
		return BeginWorldRoot(name, entity.GetScene(), entity.GetWorldPosition() + worldOffset, params);
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

		const Font* font = params.TextFont ? params.TextFont : (m_StyleStack.back().TextFont ? m_StyleStack.back().TextFont : m_Params.DefaultFont);
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
		DE_PROFILE_SCOPE("DingoUI::EndFrame");

		m_InFrame = false;
		if (m_Tree.CloseAll() > 0)
			WarnMismatch("a Begin call was never closed; the frame's elements were closed for you");

		if (m_StyleStack.size() > 1)
		{
			m_StyleStack.resize(1);
			if (!m_WarnedStyleStack)
			{
				m_WarnedStyleStack = true;
				DE_CORE_WARN("DingoUI: a PushStyle was never popped; the frame's styles were dropped for you");
			}
		}

		{
			DE_PROFILE_SCOPE("DingoUI::Layout");
			const UICpuTimer timer;
			m_Tree.Layout(m_Measure);
			m_FrameCosts.LayoutMs = timer.ElapsedMs();
		}

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
		{
			DE_PROFILE_SCOPE("DingoUI::Emit");
			const UICpuTimer timer;
			BuildDrawList(draw);
			m_FrameCosts.EmitMs = timer.ElapsedMs();
			m_FrameCosts.ExitingRoots = static_cast<uint32_t>(m_ExitingRoots.size());
		}
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
		m_Tweens.Evict(m_FrameIndex);
		++m_FrameIndex;
	}

	namespace
	{
		float SafeDelta(float dt)
		{
			return dt > 0.0f && dt < 1.0e6f ? dt : 0.0f;
		}
	}

	void DingoUIContext::BuildDrawList(bool draw)
	{
		const float dt = SafeDelta(m_Input.DeltaTime);
		const std::vector<UIElement>& elements = m_Tree.GetElements();

		m_Tree.GetRootOrder(m_RootOrder);
		std::vector<UIRootTransitions::Entry*> declared(m_RootOrder.size(), nullptr);
		for (size_t i = 0; i < m_RootOrder.size(); ++i)
		{
			const UIElement& root = elements[m_RootOrder[i]];
			declared[i] = m_RootTransitions.Declare(root.Id, root.Transition, root.Layer, m_FrameIndex, dt);
		}
		m_RootTransitions.AdvanceExiting(m_FrameIndex, dt, m_ExitingRoots);
		if (!draw)
			return;

		const auto emitExiting = [this](size_t& next, int layer)
		{
			while (next < m_ExitingRoots.size() && m_ExitingRoots[next]->Layer <= layer)
			{
				const UIRootTransitions::Entry& entry = *m_ExitingRoots[next++];
				const UIRootVisual visual = UIRootTransitions::VisualOf(entry);
				m_DrawList.AppendTransformed(entry.Kept, visual.Alpha, visual.Offset * entry.RootScale, visual.Scale, entry.Pivot);
			}
		};

		size_t nextExiting = 0;
		for (size_t i = 0; i < m_RootOrder.size(); ++i)
		{
			const int32_t index = m_RootOrder[i];
			const UIElement& root = elements[index];
			emitExiting(nextExiting, root.Layer);
			UIRootTransitions::Entry* entry = declared[i];
			if (!entry)
			{
				if (root.Alpha >= 1.0f)
				{
					m_Tree.EmitRoot(index, m_DrawList);
					continue;
				}
				m_RootScratch.Clear();
				m_Tree.EmitRoot(index, m_RootScratch);
				m_DrawList.AppendTransformed(m_RootScratch, root.Alpha, glm::vec2(0.0f), 1.0f, glm::vec2(0.0f));
				continue;
			}

			m_RootScratch.Clear();
			m_Tree.EmitRoot(index, m_RootScratch);
			const UIRootVisual visual = UIRootTransitions::VisualOf(*entry);
			if (visual.IsIdentity() && root.Alpha >= 1.0f)
				m_DrawList.Append(m_RootScratch);
			else
				m_DrawList.AppendTransformed(m_RootScratch, visual.Alpha * root.Alpha, visual.Offset * root.Scale, visual.Scale, (root.Rect.Min + root.Rect.Max) * 0.5f);
			entry->Pivot = (root.Rect.Min + root.Rect.Max) * 0.5f;
			entry->RootScale = root.Scale;
			std::swap(entry->Kept, m_RootScratch);
		}
		emitExiting(nextExiting, (std::numeric_limits<int>::max)());
	}

	void DingoUIContext::OnTextureDestroyed(const Texture* texture)
	{
		if (m_RootTransitions.Size() > 0)
		{
			m_RootTransitions.ForgetTexture(texture);
			m_ExitingRoots.clear();
		}
		m_Batcher->ForgetTexture(texture);
	}

	float DingoUIContext::Tween(std::string_view id, float target, float duration, UIEase ease)
	{
		return TweenColor(id, glm::vec4(target, 0.0f, 0.0f, 0.0f), duration, ease).x;
	}

	glm::vec4 DingoUIContext::TweenColor(std::string_view id, const glm::vec4& target, float duration, UIEase ease)
	{
		if (!m_InFrame)
			return target;
		uint64_t key = m_Tree.MakeId(id);
		if (key == 0)
			key = UILayoutTree::HashName(id);
		return m_Tweens.Update(key, target, SafeDelta(m_Input.DeltaTime), duration, ease, m_FrameIndex);
	}

	std::array<float, 4> DingoUIContext::StepLook(UIWidgetState& state, const UIStyle& style, bool hover, bool press, bool focus, bool on) const
	{
		const std::array<float, 4> targets = { hover ? 1.0f : 0.0f, press ? 1.0f : 0.0f, focus ? 1.0f : 0.0f, on ? 1.0f : 0.0f };
		const float duration = style.TransitionDuration;
		if (!state.AnimInit || duration <= 0.0f)
		{
			state.Anim = targets;
			state.AnimInit = true;
			return targets;
		}

		const float dt = SafeDelta(m_Input.DeltaTime);
		std::array<float, 4> eased{};
		for (size_t i = 0; i < 4; ++i)
		{
			if (state.Anim[i] != targets[i])
				state.Anim[i] = StepProgress(state.Anim[i], dt, duration, targets[i] > state.Anim[i]);
			eased[i] = EvaluateEase(style.TransitionEase, state.Anim[i]);
		}
		return eased;
	}

	bool DingoUIContext::HitTestBlocking(const glm::vec2& point) const
	{
		return m_ModalActive || m_LastTree.HitTest(point).OverBlocking;
	}

}
