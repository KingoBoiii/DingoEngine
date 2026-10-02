#pragma once
#include "Tests/GraphicsTest.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Dingo
{

	// Skinned meshes through a real Scene, with Fox.gltf: skinned on the GPU by a
	// SkinnedMeshRendererComponent, its rest pose drawn by a MeshRendererComponent to compare
	// against, a hand-posed Fox drawn with SubmitSkinnedMesh, a Fox played by an AnimatorComponent
	// with a box on its head joint, and a crowd for the skinned-draw budget and timing. Check
	// results show in the Properties panel and the log.
	//
	// --anim=bind|bindstatic|pose|clip|crowd, --anim-clip=Survey|Walk|Run (clip: default Walk;
	// crowd: animates every fox, out of step), --anim-time=S (clip: freeze at S seconds),
	// --anim-count=N (crowd size, default 64), --anim-static (draw the crowd through
	// MeshRendererComponent instead). Time the crowd with the test app's --no-vsync.
	class AnimationTest : public GraphicsTest
	{
	public:
		AnimationTest() = default;
		virtual ~AnimationTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		enum class Mode { Bind, BindStatic, Pose, Clip, Crowd };

		void Check(bool condition, const std::string& name);
		void RunLoadChecks();
		void RunAnimatorChecks();
		void RunSceneChecks();
		void RunDrawChecks(const Renderer3D::Statistics& stats);
		void BuildScene();
		void DestroyScene();
		void SubmitPosedFox(Renderer3D& renderer);
		void TrackTiming(float deltaTime, double updateMs, double renderMs, double endSceneMs);

	private:
		struct CheckResult
		{
			std::string Name;
			bool Passed;
		};
		std::vector<CheckResult> m_Checks;

		Model*    m_Fox = nullptr;
		Material* m_FoxMaterial = nullptr;
		Scene*    m_Scene = nullptr;

		Mode        m_Mode = Mode::Bind;
		uint32_t    m_CrowdCount = 64;
		bool        m_CrowdStatic = false;
		std::string m_ClipName;
		float       m_FreezeTime = -1.0f;

		float     m_FoxScale = 1.0f;
		glm::vec3 m_FoxOffset{ 0.0f };
		glm::vec3 m_FoxCenter{ 0.0f };
		float     m_FoxRadius = 1.0f;

		std::vector<glm::mat4> m_PosePalette;
		glm::mat4 m_PoseTransform{ 1.0f };

		bool m_ArgsRead = false;
		bool m_DrawChecksDone = false;

		float    m_Time = 0.0f;
		uint32_t m_TimedFrames = 0;
		double   m_FrameMs = 0.0;
		double   m_UpdateMs = 0.0;
		double   m_RenderMs = 0.0;
		double   m_EndSceneMs = 0.0;
		std::string m_TimingResult;

		Renderer3D::Statistics m_LastStats;

		PerspectiveCamera m_Camera;
	};

}
