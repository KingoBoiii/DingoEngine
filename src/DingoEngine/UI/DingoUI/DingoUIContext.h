#pragma once
#include "DingoEngine/UI/DingoUI.h"
#include "DingoUIDrawList.h"
#include "DingoUILayout.h"
#include "DingoUINav.h"
#include "DingoUIState.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Dingo
{
	class Framebuffer;
}

namespace Dingo::Internal
{

	class DingoUIBatcher;

	// One frame of pointer and keyboard input as the game UI sees it. The Application fills it
	// from Input after a capturing ImGui has taken its share; the test app passes its own.
	struct UIInputState
	{
		static constexpr size_t k_PointerButtonCount = 3; // left, right, middle

		glm::vec2 ViewportSize{ 0.0f };     // pixels the pointer position is measured against (the window)
		glm::vec2 PointerPosition{ 0.0f };  // pixels, origin top-left, +Y down
		glm::vec2 Scroll{ 0.0f };
		std::array<bool, k_PointerButtonCount> PointerDown{};
		std::array<bool, k_PointerButtonCount> PointerPressed{};
		std::array<bool, k_PointerButtonCount> PointerReleased{};
		// False while the cursor is Locked or ImGui holds the mouse: no hover, no presses.
		bool PointerAvailable = false;
		// False while ImGui holds the keyboard.
		bool KeyboardAvailable = false;
		UINavInput Nav;
		float DeltaTime = 0.0f;
	};

	// The state behind the static DingoUI calls. The Application creates one (never headless) and
	// drives BeginFrame before the layers' OnUpdate and EndFrame after them.
	class DingoUIContext
	{
	public:
		explicit DingoUIContext(const DingoUIParams& params);
		~DingoUIContext();

		DingoUIContext(const DingoUIContext&) = delete;
		DingoUIContext& operator=(const DingoUIContext&) = delete;

		void BeginFrame(const UIInputState& input);
		// Layout and state always run; drawing only when render and the frame isn't skipped.
		void EndFrame(bool render);

		// Test hooks. A null target draws to the swap chain; a null override uses the
		// Application's input. The override is read in BeginFrame and must outlive the frames it
		// is set for.
		void SetTarget(Framebuffer* target) { m_Target = target; }
		Framebuffer* GetTarget() const { return m_Target; }
		void SetInputOverride(const UIInputState* input) { m_InputOverride = input; }
		bool HasInputOverride() const { return m_InputOverride != nullptr; }

		// Commands drawn at this frame's EndFrame, then cleared, before the elements' own. The test
		// app fills it directly to check the batcher alone.
		UIDrawList& GetDrawList() { return m_DrawList; }
		// What the last EndFrame drew; zero when it drew nothing.
		const UIRenderStatistics& GetRenderStatistics() const { return m_RenderStatistics; }

		// The declarations. Valid between BeginFrame and EndFrame; false outside, with a warning once.
		bool BeginRoot(std::string_view name, const UIRootParams& params);
		void EndRoot();
		bool BeginBox(const UIBoxParams& params);
		void EndBox();
		void Text(std::string_view text, const UITextParams& params);
		void Image(Texture* texture, const UIImageParams& params);

		UIInteraction Button(std::string_view label, const UIWidgetParams& params);
		UIInteraction Toggle(std::string_view label, bool* value, const UIWidgetParams& params);
		UIInteraction Slider(std::string_view label, float* value, float min, float max, const UISliderParams& params);
		UIInteraction SliderInt(std::string_view label, int* value, int min, int max, const UISliderParams& params);
		void ProgressBar(std::string_view id, float value, const UIWidgetParams& params);
		UIInteraction Selectable(std::string_view label, bool selected, const UIWidgetParams& params);
		bool BeginScrollArea(std::string_view label, const UIScrollParams& params);
		void EndScrollArea();
		bool BeginPanel(const UIBoxParams& params);
		void EndPanel();
		void PushId(std::string_view id);
		void PushId(int id);
		void PopId();
		void PushStyle(const UIStyle& style);
		void PopStyle();
		const UIStyle& GetStyle() const { return m_StyleStack.back(); }
		void SetTheme(const UIStyle& style) { m_StyleStack.front() = style; }

		void SetDefaultFont(Font* font) { m_Params.DefaultFont = font; }
		Font* GetDefaultFont() const { return m_Params.DefaultFont; }

		// Last frame's elements with their final rects, in target pixels: what the next frame's
		// hit-testing reads. Replaced by EndFrame.
		const UILayoutTree& GetLastLayout() const { return m_LastTree; }
		// Pixels of the target the UI lays out on (the Application's swap chain, or SetTarget's).
		const glm::vec2& GetTargetSize() const { return m_TargetSize; }
		// A point in the window (what Input reports) in target pixels.
		glm::vec2 WindowToTarget(const glm::vec2& point) const { return point * m_WindowToTarget; }

		// The input as BeginFrame took it; the pointer is in target pixels.
		const UIInputState& GetInput() const { return m_Input; }
		const DingoUIParams& GetParams() const { return m_Params; }
		uint64_t GetFrameIndex() const { return m_FrameIndex; }

		// Against last frame's layout: over an element of a blocking root, or any modal root exists.
		bool HitTestBlocking(const glm::vec2& point) const;
		bool IsPointerOverUI() const { return m_PointerOverUI; }
		bool IsCapturingInput() const { return m_CapturingInput; }

		// Interaction as BeginFrame resolved it; 0 for none.
		uint64_t GetHoverId() const { return m_HoverId; }
		uint64_t GetActiveId() const { return m_ActiveId; }
		uint64_t GetScrollTargetId() const { return m_ScrollTargetId; }
		const UIStateMap& GetStates() const { return m_States; }
		// Widgets declared with an id another widget already used in the same frame, since the start.
		uint32_t GetIdCollisionCount() const { return m_IdCollisionCount; }

		// Focus as BeginFrame resolved it; 0 for none. The ring and UIInteraction::Focused show only
		// while IsFocusVisible.
		uint64_t GetFocusId() const { return m_FocusId; }
		bool IsFocusVisible() const { return m_FocusVisible; }
		bool IsBackPressed() const { return m_BackPressed; }
		// Applied in the next BeginFrame once the widget exists.
		void SetFocus(std::string_view label);

	private:
		glm::vec2 ComputeTargetSize() const;
		bool CanDeclare();
		void WarnMismatch(const char* what);
		void ResolveInteraction();
		void ResolveNavigation();
		void CollectNavCandidates();
		int32_t NavIndexOf(uint64_t id) const;
		int32_t EstablishFocus() const;
		void EnsureFocusVisible(int32_t candidate);
		// Touches the state of `id` for this frame (reporting a reuse of the id) and reads what
		// the pointer did to it.
		UIInteraction Interact(uint64_t id, std::string_view label, bool enabled, UIWidgetState*& state);
		// 0 (with a warning once) when no root is open.
		uint64_t WidgetId(std::string_view label);
		void BeginWidgetBox(uint64_t id, std::string_view label, const UIBoxParams& box, bool interactive, bool scroll = false, const glm::vec2& scrollOffset = glm::vec2(0.0f), const UIStyle* thumbStyle = nullptr);
		const UIStyle& Resolve(const UIStyle* override) const { return override ? *override : m_StyleStack.back(); }
		UIInteraction SliderImpl(std::string_view label, double* value, double min, double max, double step, bool integer, const UISliderParams& params);

	private:
		DingoUIParams m_Params;
		Framebuffer* m_Target = nullptr;
		const UIInputState* m_InputOverride = nullptr;
		UIInputState m_Input;
		UILayoutTree m_Tree;
		UILayoutTree m_LastTree;
		UITextMeasureFn m_Measure;
		std::string m_MeasureScratch;
		glm::vec2 m_TargetSize{ 0.0f };
		glm::vec2 m_WindowToTarget{ 1.0f };
		bool m_InFrame = false;
		bool m_WarnedOutsideFrame = false;
		bool m_WarnedMismatch = false;
		bool m_WarnedNoFont = false;
		UIDrawList m_DrawList;
		std::unique_ptr<DingoUIBatcher> m_Batcher;
		UIRenderStatistics m_RenderStatistics;
		uint64_t m_FrameIndex = 0;
		bool m_PointerOverUI = false;
		bool m_CapturingInput = false;
		bool m_ModalActive = false;

		UIStateMap m_States;
		UIHit m_Hit;
		uint64_t m_HoverId = 0;
		uint64_t m_ActiveId = 0;
		uint64_t m_PressedId = 0;
		uint64_t m_ClickedId = 0;
		uint64_t m_ReleasedId = 0;
		uint64_t m_ScrollTargetId = 0;
		glm::vec2 m_ScrollDelta{ 0.0f };
		std::array<bool, UIInputState::k_PointerButtonCount> m_ConsumedDown{};
		std::unordered_set<uint64_t> m_WarnedCollisions;
		uint32_t m_IdCollisionCount = 0;
		std::vector<uint64_t> m_ScrollStack;
		std::vector<UIStyle> m_StyleStack;
		bool m_WarnedStyleStack = false;

		struct WidgetFocus
		{
			bool Focusable = false;
			bool DefaultFocus = false;
			bool AdjustsHorizontally = false;
		};
		WidgetFocus m_WidgetFocus;
		std::vector<UINavCandidate> m_NavCandidates;
		std::vector<int32_t> m_NavElements;
		std::vector<int32_t> m_NavRoots;
		std::vector<int32_t> m_RootOrder;
		std::array<UINavRepeater, k_UINavDirectionCount> m_Repeaters;
		std::array<bool, k_UINavDirectionCount> m_DirectionHeld{};
		std::unordered_map<uint64_t, uint64_t> m_RootFocus;
		std::unordered_set<uint64_t> m_SeenDefault;
		uint64_t m_FocusId = 0;
		uint64_t m_FocusRootId = 0;
		uint64_t m_SavedFocusId = 0;
		uint64_t m_PendingFocusId = 0;
		uint32_t m_PendingFocusFrames = 0;
		uint32_t m_EnsureVisibleFrames = 0;
		glm::vec2 m_FocusCenter{ 0.0f };
		glm::vec2 m_LastPointer{ 0.0f };
		bool m_HadPointer = false;
		bool m_FocusVisible = false;
		bool m_BackPressed = false;
		int m_NavAdjust = 0;
		uint64_t m_NavAcceptId = 0;
	};

	// The Application's context, or nullptr (headless, DingoUIParams::Enabled false, or outside
	// the application's lifetime).
	DingoUIContext* GetDingoUIContext();

}
