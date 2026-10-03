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
	// --anim=events walks the Fox through the Speed blend and drops a footprint under each foot
	// as its step event (Fox.events) fires, with an event log in the Properties panel.
	//
	// --anim=bind|bindstatic|pose|clip|blend|layers|events|crowd, --anim-clip=Survey|Walk|Run (clip:
	// default Walk; crowd: animates every fox, out of step), --anim-time=S (clip, layers: freeze at
	// S seconds), --anim-speed=X (blend: the Speed parameter, Survey at 0, Walk at 1.5, Run at 4),
	// --anim-phase=F (blend: freeze at that fraction of the cycle), --anim-count=N (crowd size,
	// default 64), --anim-static (draw the crowd through MeshRendererComponent instead). Time the
	// crowd with the test app's --no-vsync.
	//
	// --anim=clip --anim-reload plays a managed copy of the Fox with hot-reload on, and edits it on
	// disk: after 1 s Walk and Run trade names, then Fox.events gains a mark. Each reload is a check.
	//
	// --anim-skeleton (or the checkbox) draws every bone and joint as boxes inside a see-through
	// Fox. F7 opens the engine's Animation tab on the same animators.
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
		enum class Mode { Bind, BindStatic, Pose, Clip, Blend, Layers, Events, Crowd };

		void Check(bool condition, const std::string& name);
		void RunLoadChecks();
		void RunAnimatorChecks();
		void RunBlendChecks();
		void RunEventChecks();
		void RecordEvent(Entity fox, const AnimationEvent& event);
		void RunSceneChecks();
		void RunReloadChecks();
		void UpdateLiveReload();
		void RunDrawChecks(const Renderer3D::Statistics& stats);
		void BuildScene();
		void DestroyScene();
		void SubmitPosedFox(Renderer3D& renderer);
		void SubmitSkeleton(Renderer3D& renderer);
		void RunSkinnedMaterialCheck(Renderer3D& renderer);
		bool SkeletonShown() const { return m_ShowSkeleton && m_Mode != Mode::Crowd && m_Mode != Mode::Pose; }
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
		Shader*   m_GhostShader = nullptr;
		Material* m_Ghost = nullptr;
		bool      m_SkinnedMaterialChecked = false;
		Scene*    m_Scene = nullptr;

		Mode        m_Mode = Mode::Bind;
		uint32_t    m_CrowdCount = 64;
		bool        m_CrowdStatic = false;
		std::string m_ClipName;
		float       m_FreezeTime = -1.0f;
		float       m_FreezePhase = -1.0f;
		float       m_BlendSpeed = 1.5f;
		float       m_LayerWeight = 1.0f;
		Entity      m_AnimatedFox;
		std::vector<Entity> m_Foxes; // all but a crowd's, for the skeleton overlay
		bool        m_ShowSkeleton = false;
		uint32_t    m_SkeletonBoxes = 0;

		float     m_FoxScale = 1.0f;
		glm::vec3 m_FoxOffset{ 0.0f };
		glm::vec3 m_FoxCenter{ 0.0f };
		float     m_FoxRadius = 1.0f;

		std::vector<glm::mat4> m_PosePalette;
		glm::mat4 m_PoseTransform{ 1.0f };

		struct LoggedEvent
		{
			std::string Clip;
			std::string Name;
			AnimationEventType Type;
			float Time;
			float At;
		};
		std::vector<LoggedEvent> m_EventLog;
		std::vector<glm::vec3> m_Footprints;

		// --anim-reload: the Fox is a managed copy, edited on disk while it plays.
		bool        m_LiveReload = false;
		AssetHandle m_FoxAsset = k_InvalidAsset;
		bool        m_HotReloadWas = false;
		uint32_t    m_ReloadStep = 0;
		uint32_t    m_ReloadGeneration = 0;
		float       m_ReloadStart = 0.0f;
		float       m_RunDuration = 0.0f;
		uint64_t    m_ReloadMeshId = 0;
		std::vector<JointPose> m_RunPose;

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
