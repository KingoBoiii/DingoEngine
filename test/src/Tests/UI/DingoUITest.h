#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"
#include "DingoEngine/UI/DingoUI/DingoUIContext.h"
#include "DingoEngine/UI/DingoUI/DingoUIDebug.h"

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// The game UI. The checks run over the first frames: font metrics and wrapping, world-to-screen
	// projection, the input consume mask DingoUI and a capturing ImGui hide presses with (driven by
	// simulated mouse and key presses), and the batcher drawing a fixed draw list into a check
	// target: one draw, the vertex cache, the texture-slot split and pixels read back. Then the
	// element API on a 960x540 target: anchors, row and column layout, grow, fit, wrap, percent,
	// min and max, scaling, recovery from unmatched Begin/End, the window-to-target pointer ratio
	// and one draw call for a menu. Then the widgets, driven by an input override and simulated
	// mouse buttons: hover, click, drag-out, a late widget, a slider drag, a scroll area, roots
	// that overlap, don't block or are modal, the consume mask, state eviction and id collisions.
	// Styles and 9-slices are checked by pixels: a pushed style changes a button's fill and its pop
	// restores it, a 9-slice keeps its corners at any size.
	// Tweens and transitions are checked with a simulated frame time: eased values at fixed steps, a
	// root fading, sliding and scaling in and out by pixels, an exiting root that takes no clicks, one
	// draw call throughout and a vertex cache hit once the animation has ended.
	// World roots are checked through a perspective camera with known matrices: a label lands at the
	// expected pixel, moves with its point and the camera, a point behind the camera or far off screen
	// is not declared, distance scaling and fading, depth order below a screen root, entities through
	// their hierarchy, the pointer passing through (or stopped by) a world root, and 50 bars in one
	// draw call.
	// The debug snapshot the F9 DingoUI tab reads is checked against the context's own numbers. A stress
	// builder runs for about 50 frames after the world checks: a 500-element static menu, then 2,000
	// widgets in scroll areas with nested styles, 200 world labels, tweens, ids that change every frame
	// and a root fading in and out, asserting one draw call, a bounded state map, and that the state, the
	// tweens and the transitions drain once the UI stops declaring. --dingoui=stress runs the same
	// builder for --dingoui-frames=N frames (default 600), logs the CPU averages and closes the app.
	// The view shows the wrapped text at its MaxWidth under a DingoUI panel built with the element
	// API, and a panel of live widgets to click by hand inside the viewport, with a themed panel and a
	// 9-slice panel beside them, and a panel that fades and slides in and out, and health bars orbiting in the world.
	class DingoUITest : public Renderer2DTest
	{
	public:
		DingoUITest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~DingoUITest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		void CheckFontMetrics();
		void CheckWorldToScreen();
		void StepConsumeMask();
		bool StepBatcher();
		bool StepLayout(Framebuffer* output);
		bool StepWidgets(Framebuffer* output);
		bool StepNavigation(Framebuffer* output);
		bool StepTransitions(Framebuffer* output);
		bool StepTextureFree();
		bool StepWorld(Framebuffer* output);
		bool StepStress(Framebuffer* output);
		void SampleStress(Internal::DingoUIContext& context);
		void FinishStress(Internal::DingoUIContext& context, Framebuffer* output);
		void DeclareStressMenu();
		void DeclareStressHeavy(uint32_t frame);
		void DeclareLiveWorld();
		void DeclareNavScene();
		void DeclareWidgetScene();
		void SetPointer(const glm::vec2& position, bool down = false, bool pressed = false, bool released = false, const glm::vec2& scroll = glm::vec2(0.0f));
		const Internal::UIElement* LastElement(const char* name) const;
		glm::vec2 CenterOf(const char* name) const;
		void UpdateLiveInput(Framebuffer* output);
		void DeclareLivePage();
		void DeclareDemo();
		void DeclareAnchors();
		void VerifyAnchors();
		void DeclareStacks();
		void VerifyStacks();
		void DeclareGrow();
		void VerifyGrow();
		void DeclareText();
		void VerifyText();
		void DeclareSizing();
		void VerifySizing();
		void DeclareScale();
		void VerifyScale();
		void DeclarePixels();
		void VerifyPixels();
		void DeclareMenu();
		void VerifyMenu();
		void DeclareUnmatched();
		void VerifyUnmatched();
		void DeclareStyle();
		void VerifyStyle();
		void DeclareNineSlice();
		void VerifyNineSlice();
		void DeclareStyledMenu();
		void VerifyStyledMenu();
		UINineSlice MakeSlice(bool point, float scale) const;
		void DeclareNoFont();
		void VerifyNoFont();
		void DeclarePointerBlocking();
		void VerifyPointerBlocking();
		void DeclarePointerPassive();
		void VerifyPointerPassive();
		void DeclarePointerRoots();
		bool RectIs(const char* name, float minX, float minY, float maxX, float maxY, float tolerance = 0.0f);
		bool SizeIs(const char* name, float width, float height, float tolerance = 0.0f);
		void SubmitBaseList(const glm::vec4& fill);
		void SubmitTextureList();
		void CheckPixels(const TexturePixels& pixels);
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

	private:
		enum class StressPhase : uint8_t { None, Menu, Heavy, Cooldown };

		struct StressTotals
		{
			uint32_t Samples = 0;
			double DingoUIMs = 0.0, DeclareMs = 0.0, BeginMs = 0.0, LayoutMs = 0.0, EmitMs = 0.0, BuildMs = 0.0, UploadMs = 0.0, DrawMs = 0.0;
			double Quads = 0.0, DrawCalls = 0.0, Elements = 0.0, States = 0.0, Uploads = 0.0;
		};

		Font* m_Font = nullptr;
		uint32_t m_Stage = 0;
		uint32_t m_DrawStage = 0;
		uint32_t m_LayoutStage = 0;
		Framebuffer* m_CheckTarget = nullptr;
		Framebuffer* m_LayoutTarget = nullptr;
		Internal::UIInputState m_PointerState;
		Internal::UIInputState m_WidgetInput;
		Internal::UIInputState m_LiveInput;
		uint32_t m_WidgetStage = 0;
		bool m_WidgetsDone = false;
		bool m_WWidgets = true;
		bool m_WOverlap = false;
		bool m_WGhost = false;
		bool m_WLate = false;
		bool m_WModal = false;
		bool m_WCollide = false;
		glm::vec2 m_WOverOffset{ 0.0f };
		UIInteraction m_WPlayResult, m_WOverResult, m_WGhostResult, m_WLateResult, m_WToggleResult, m_WSliderResult, m_WIntResult;
		UIInteraction m_WItemResult[2];
		bool m_WToggleValue = false;
		float m_WSliderValue = 0.5f;
		int m_WIntValue = 3;
		int m_WSelected = 0;
		uint32_t m_WPlayClicks = 0;
		bool m_LiveToggle = true;
		bool m_LivePanelOpen = true;
		float m_LiveSlider = 0.4f;
		int m_LiveInt = 3;
		int m_LiveSelected = 0;
		uint32_t m_LiveClicks = 0;
		uint32_t m_LiveBacks = 0;
		static constexpr uint32_t k_NavCount = 10;
		uint32_t m_NavStage = 0;
		uint32_t m_NavCounter = 0;
		bool m_NavDone = false;
		bool m_WNav = false;
		bool m_WNavModal = false;
		bool m_WNavHide = false;
		bool m_WNavHud = false;
		bool m_WNavInfo = false;
		bool m_WNavSoftModal = false;
		bool m_WNavToggle = false;
		std::string m_WNavFocus;
		float m_WNavSlider = 0.5f;
		float m_NavSliderBefore = 0.0f;
		uint32_t m_WNavClicks = 0;
		UIInteraction m_WNavResult[k_NavCount];
		uint64_t m_WNavIds[k_NavCount]{};
		uint32_t m_TransStage = 0;
		bool m_TransDone = false;
		uint32_t m_FreeStage = 0;
		bool m_FreeDone = false;
		Font* m_FreeFont = nullptr;
		Texture* m_FreeTexture = nullptr;
		bool m_LivePanel = true;
		Texture* m_ImageTexture = nullptr;
		Texture* m_SliceTexture = nullptr;
		std::vector<Texture*> m_SlotTextures;
		std::shared_ptr<int> m_Alive;
		Scene* m_WorldScene = nullptr;
		Entity m_WorldParent;
		Entity m_WorldChild;
		Entity m_WorldCamera;
		uint32_t m_WorldStage = 0;
		bool m_WorldDone = false;
		float m_LiveTime = 0.0f;
		bool m_StressMode = false;
		bool m_StressDone = false;
		uint32_t m_StressStep = 0;
		uint32_t m_StressMenuFrames = 0;
		uint32_t m_StressHeavyFrames = 0;
		StressPhase m_StressPrev = StressPhase::None;
		uint32_t m_StressPhaseFrame = 0;
		float m_StressDeclareMs = 0.0f;
		uint32_t m_StressStableWidgets = 0;
		uint32_t m_StressWorldDeclared = 0;
		uint32_t m_StressBadDraws = 0;
		uint32_t m_StressBadSnapshots = 0;
		uint32_t m_StressMaxStates = 0;
		uint32_t m_StressMaxExiting = 0;
		uint32_t m_StressMenuElements = 0;
		uint32_t m_StressMenuBadCache = 0;
		uint32_t m_StressMenuTextMisses = 0;
		uint64_t m_StressLastTextMisses = 0;
		StressTotals m_StressMenuTotals;
		StressTotals m_StressHeavyTotals;
		Internal::DingoUIDebug::Snapshot m_StressSnapshot;
		TestChecks m_Checks;
	};

}
