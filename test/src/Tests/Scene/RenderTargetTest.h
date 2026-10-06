#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Scenes rendered into framebuffers (SceneRenderer::Render with a target) and read back to the
	// CPU (Texture::ReadPixels, SaveToFile). Two lit 3D scenes render into textures every frame and
	// crossfade on screen. A 2D probe scene, in the corner, renders into a square framebuffer that the
	// first update reads back and checks: its projection takes the framebuffer's aspect, row 0 is its
	// top, text below a sprite in z is hidden, rotated text turns, and the PNG it saves holds the same
	// pixels. Check results show in the Properties panel and the log.
	class RenderTargetTest : public Renderer2DTest
	{
	public:
		RenderTargetTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~RenderTargetTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		void BuildProbeScene();
		Scene* BuildShowScene(const char* name, const glm::vec4& clearColor, Mesh* mesh, const glm::vec4& color, Entity& spinner);
		void RunImageFileChecks();
		void CheckProbe(const TexturePixels& pixels);
		void Check(bool condition, const std::string& name);

	private:
		Font* m_Font = nullptr;
		Scene* m_Probe = nullptr;
		Scene* m_SceneA = nullptr;
		Scene* m_SceneB = nullptr;
		Entity m_SpinnerA;
		Entity m_SpinnerB;
		Framebuffer* m_ProbeTarget = nullptr;
		Framebuffer* m_TargetA = nullptr;
		Framebuffer* m_TargetB = nullptr;
		Texture* m_Container = nullptr;

		float m_Time = 0.0f;
		bool m_ProbeRequested = false;
		std::vector<uint8_t> m_ProbePixels;
		std::filesystem::path m_TempDirectory;

		// ReadPixels runs its callbacks a frame later; they hold a weak_ptr to this, so a callback
		// for a test that has been closed meanwhile does nothing.
		std::shared_ptr<int> m_Alive;

		struct CheckResult
		{
			std::string Name;
			bool Passed = false;
		};
		std::vector<CheckResult> m_Checks;
	};

}
