#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"

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
	//
	// The render-target groundwork of v0.9 is checked on the same first update, by reading back what
	// fullscreen passes drew: every colour format clears and reads back (values past 1 included), a
	// sampleable depth attachment is sampled and compared, a framebuffer resized under a material that
	// samples it keeps its Texture and is rebound, a viewport limits a draw, additive blending adds, and
	// a shader file #includes its neighbour.
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

		void CreateGroundworkResources();
		void RunGroundworkChecks();
		void DestroyGroundworkResources();
		void ReadBack(Framebuffer* target, std::function<void(const TexturePixels&)> check);
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

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

		struct FormatTarget
		{
			TextureFormat Format = TextureFormat::Unknown;
			const char* Name = "";
			Framebuffer* Target = nullptr;
		};
		std::vector<FormatTarget> m_FormatTargets;
		Framebuffer* m_DepthSource = nullptr;  // RGBA8 + a sampleable depth, drawn by Renderer3D
		Framebuffer* m_DepthCopy = nullptr;    // R32F: the depth, sampled
		Framebuffer* m_DepthCompare = nullptr; // R16F: the depth through a comparison sampler
		Framebuffer* m_ResizeTarget = nullptr;
		Framebuffer* m_ResizeCopy = nullptr;
		Framebuffer* m_ViewportTarget = nullptr;
		Framebuffer* m_BlendTarget = nullptr;
		Shader* m_FillShader = nullptr;
		Shader* m_SampleShader = nullptr;
		Shader* m_CompareShader = nullptr;
		Material* m_FillMaterial = nullptr;
		Material* m_AddMaterial = nullptr;
		Material* m_DepthCopyMaterial = nullptr;
		Material* m_CompareMaterial = nullptr;
		Material* m_ResizeCopyMaterial = nullptr;
		Sampler* m_CompareSampler = nullptr;

		float m_Time = 0.0f;
		bool m_ProbeRequested = false;
		std::vector<uint8_t> m_ProbePixels;
		std::filesystem::path m_TempDirectory;

		// ReadPixels runs its callbacks a frame later; they hold a weak_ptr to this, so a callback
		// for a test that has been closed meanwhile does nothing.
		std::shared_ptr<int> m_Alive;

		TestChecks m_Checks;
	};

}
