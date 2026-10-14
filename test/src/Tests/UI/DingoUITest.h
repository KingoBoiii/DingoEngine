#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// The game UI. The checks run over the first frames: font metrics and wrapping, world-to-screen
	// projection, the input consume mask DingoUI and a capturing ImGui hide presses with (driven by
	// simulated mouse and key presses), and the batcher drawing a fixed draw list into a check
	// target: one draw, the vertex cache, the texture-slot split and pixels read back. The view
	// shows the wrapped text at its MaxWidth under a DingoUI panel.
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
		void StepBatcher(Framebuffer* output);
		void SubmitBaseList(const glm::vec4& fill);
		void SubmitTextureList();
		void SubmitDemo(Framebuffer* output);
		void CheckPixels(const TexturePixels& pixels);
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

	private:
		Font* m_Font = nullptr;
		uint32_t m_Stage = 0;
		uint32_t m_DrawStage = 0;
		Framebuffer* m_CheckTarget = nullptr;
		Texture* m_ImageTexture = nullptr;
		std::vector<Texture*> m_SlotTextures;
		std::shared_ptr<int> m_Alive;
		TestChecks m_Checks;
	};

}
