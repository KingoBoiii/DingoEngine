#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Dingo
{

	// Renderer3D's throughput through a Scene, the yardstick of v1.0's performance work. Every
	// renderable is an entity, drawn by Scene::RenderEntities3D in the test's own pass (BeginScene,
	// Clear, SubmitLights, RenderEntities3D, EndScene, as SceneRenderer does), so both halves are
	// timed apart.
	//
	// --throughput=static|repeat|mixed|skinned|shadow, --count=N (default 10000; skinned 64),
	// --seed=S. static: N meshes in a grid, each its own Mesh of distinct geometry, so nothing could
	// ever be instanced. repeat: N copies of five props. mixed: rooms of both, the camera flying a
	// fixed lap so about a tenth is in view. skinned: N walking Foxes. shadow: mixed under a casting
	// sun and 8 casting point lights.
	//
	// The layout comes from the seed and the camera from the frame index, never the clock, so two
	// runs draw the same frames. --perf logs one [PERF] line after 120 warm-up frames, the means of
	// the next 600; time it with the test app's --no-vsync. --frame=N holds the camera where frame N puts it.
	class ThroughputTest : public GraphicsTest
	{
	public:
		ThroughputTest() = default;
		virtual ~ThroughputTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	public:
		enum class Mode : int { Static, Repeat, Mixed, Skinned, Shadow };

		struct Prism
		{
			uint32_t Sides = 0;
			float BottomRadius = 0.0f;
			float TopRadius = 0.0f;
			float Height = 0.0f;
			float Twist = 0.0f;
		};

		struct Placement
		{
			glm::vec3 Position{ 0.0f };
			float Yaw = 0.0f;
			glm::vec3 Scale{ 1.0f };
			glm::vec4 Color{ 1.0f };
			int32_t Prop = -1;   // one of the repeated props, or -1 for Prisms[Unique]
			uint32_t Unique = 0;
		};

		struct Layout
		{
			std::vector<Placement> Placements;
			std::vector<Prism> Prisms;
		};

	private:
		void ReadArgs();
		void BuildScene();
		void PlaceCamera(PerspectiveCamera& camera, uint64_t frame) const;
		Transform3DComponent MakeTransform(const Placement& placement) const;
		float EstimateVisiblePercent() const;
		void RunChecks(const Renderer3D::Statistics& stats);
		void RecordPerf(float deltaTime, double updateMs, double renderEntitiesMs, double endSceneMs, const Renderer3D::Statistics& stats);

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

	private:
		TestChecks m_Checks;

		Mode     m_Mode = Mode::Static;
		uint32_t m_Count = 0;
		uint64_t m_Seed = 1;
		bool     m_Perf = false;
		bool     m_ArgsRead = false;
		int64_t  m_HeldFrame = -1; // --frame=N: the camera stays where frame N puts it

		Layout m_Layout;
		std::vector<Mesh*> m_Props;
		std::vector<Mesh*> m_UniqueMeshes;
		std::vector<Entity> m_Renderables;
		Model*    m_Fox = nullptr;
		Material* m_FoxMaterial = nullptr;
		float     m_FoxScale = 1.0f;
		glm::vec3 m_FoxOffset{ 0.0f };
		uint32_t  m_FoxSubMeshes = 0;
		Scene*    m_Scene = nullptr;
		float     m_Extent = 1.0f; // the layout's side in metres
		uint64_t  m_LayoutHash = 0;

		PerspectiveCamera m_Camera;
		uint64_t m_Frame = 0;
		bool     m_ChecksDone = false;

		Renderer3D::Statistics m_LastStats;
		double m_RollingFrameMs = 0.0;
		double m_RollingRenderMs = 0.0;
		double m_RollingEndSceneMs = 0.0;

		uint32_t m_PerfFrames = 0;
		bool     m_PerfDone = false;
		double   m_PerfFrameMs = 0.0;
		double   m_PerfUpdateMs = 0.0;
		double   m_PerfRenderEntitiesMs = 0.0;
		double   m_PerfEndSceneMs = 0.0;
		double   m_PerfDraws = 0.0;
		double   m_PerfVertices = 0.0;
		uint32_t m_PerfDropped = 0;
		std::map<std::string, double> m_PerfGpuMs;
		std::string m_PerfResult;
	};

}
