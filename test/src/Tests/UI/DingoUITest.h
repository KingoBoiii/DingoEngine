#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"
#include "DingoEngine/UI/DingoUI/DingoUIContext.h"

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
	// and one draw call for a menu. The view shows the wrapped text at its MaxWidth under a
	// DingoUI panel built with the element API.
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
		Font* m_Font = nullptr;
		uint32_t m_Stage = 0;
		uint32_t m_DrawStage = 0;
		uint32_t m_LayoutStage = 0;
		Framebuffer* m_CheckTarget = nullptr;
		Framebuffer* m_LayoutTarget = nullptr;
		Internal::UIInputState m_PointerState;
		Texture* m_ImageTexture = nullptr;
		std::vector<Texture*> m_SlotTextures;
		std::shared_ptr<int> m_Alive;
		TestChecks m_Checks;
	};

}
