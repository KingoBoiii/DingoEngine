#pragma once
#include "DingoEngine/UI/DingoUI.h"
#include "DingoUIDrawList.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
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

		glm::vec2 ViewportSize{ 0.0f };     // pixels the pointer position is measured against
		glm::vec2 PointerPosition{ 0.0f };  // pixels, origin top-left, +Y down
		glm::vec2 Scroll{ 0.0f };
		std::array<bool, k_PointerButtonCount> PointerDown{};
		std::array<bool, k_PointerButtonCount> PointerPressed{};
		std::array<bool, k_PointerButtonCount> PointerReleased{};
		// False while the cursor is Locked or ImGui holds the mouse: no hover, no presses.
		bool PointerAvailable = false;
		// False while ImGui holds the keyboard.
		bool KeyboardAvailable = false;
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

		// Commands drawn at this frame's EndFrame, then cleared. Until U2's elements build it, the
		// test app fills it directly.
		UIDrawList& GetDrawList() { return m_DrawList; }
		// What the last EndFrame drew; zero when it drew nothing.
		const UIRenderStatistics& GetRenderStatistics() const { return m_RenderStatistics; }

		const UIInputState& GetInput() const { return m_Input; }
		const DingoUIParams& GetParams() const { return m_Params; }
		uint64_t GetFrameIndex() const { return m_FrameIndex; }

		// Against last frame's layout.
		bool HitTestBlocking(const glm::vec2& point) const;
		bool IsPointerOverUI() const { return m_PointerOverUI; }
		bool IsCapturingInput() const { return m_CapturingInput; }

	private:
		DingoUIParams m_Params;
		Framebuffer* m_Target = nullptr;
		const UIInputState* m_InputOverride = nullptr;
		UIInputState m_Input;
		std::vector<UIRect> m_BlockingRects;
		UIDrawList m_DrawList;
		std::unique_ptr<DingoUIBatcher> m_Batcher;
		UIRenderStatistics m_RenderStatistics;
		uint64_t m_FrameIndex = 0;
		bool m_PointerOverUI = false;
		bool m_CapturingInput = false;
	};

	// The Application's context, or nullptr (headless, DingoUIParams::Enabled false, or outside
	// the application's lifetime).
	DingoUIContext* GetDingoUIContext();

}
