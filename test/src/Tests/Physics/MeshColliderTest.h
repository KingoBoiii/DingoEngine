#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <glm/glm.hpp>

#include <random>
#include <string>
#include <vector>

namespace Dingo
{

	// Drives MeshCollider3DComponent through a real Scene: a static triangle-mesh terrain
	// bowl, a kinematic triangle-mesh lift rising through it, and a stream of dynamic
	// bodies (convex-hull pebbles, spheres, boxes) landing on both. Check results show
	// in the Properties panel and the log.
	class MeshColliderTest : public GraphicsTest
	{
	public:
		MeshColliderTest() = default;
		virtual ~MeshColliderTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void SpawnBody();
		void UpdateLift(float deltaTime);
		void RunSettledChecks();
		// Query filters, sensors, controllers blocking each other and body-to-entity lookup, in
		// worlds of their own.
		void RunQueryChecks();

	private:
		TestChecks m_Checks;

		Scene* m_Scene = nullptr;
		Mesh*  m_TerrainMesh = nullptr;
		Mesh*  m_PebbleMesh = nullptr;
		Mesh*  m_LiftMesh = nullptr;

		Entity m_Terrain;
		Entity m_Lift;
		std::vector<Entity> m_Bodies;

		std::mt19937 m_Random{ 7 };
		uint32_t m_SpawnTarget = 30;
		uint32_t m_Spawned = 0;
		uint32_t m_Escaped = 0;
		float m_SpawnTimer = 0.0f;
		float m_SettleTimer = 0.0f;
		bool m_SettledChecksDone = false;
		bool m_PebbleChecked = false;

		bool  m_LiftEnabled = true;
		float m_LiftTime = 0.0f;

		PerspectiveCamera m_Camera;
		float m_OrbitAngle = 30.0f;
		bool  m_AutoOrbit = true;
	};

}
