#include "HierarchyTest.h"

#include <glm/gtc/constants.hpp>

#include <chrono>
#include <cmath>
#include <format>

namespace
{
	using namespace Dingo;
	using Clock = std::chrono::steady_clock;

	// 10 roots x 10 x 10 x 9 = 10,110 entities, four levels deep.
	constexpr int k_Branching[] = { 10, 10, 10, 9 };
	constexpr float k_Radius[] = { 12.0f, 2.5f, 2.0f, 1.8f };
	constexpr float k_Lift[] = { 1.5f, 0.8f, 0.6f, 0.4f };
	constexpr float k_Scale[] = { 0.8f, 0.5f, 0.6f, 0.7f };
	constexpr int k_SpinningLevels = 3;
	constexpr float k_WarmupSeconds = 2.0f;
	constexpr uint32_t k_MeasuredFrames = 120;

	double Milliseconds(Clock::time_point from, Clock::time_point to)
	{
		return std::chrono::duration<double, std::milli>(to - from).count();
	}
}

namespace Dingo
{

	void HierarchyTest::BuildStressScene(bool parented)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		Mesh* box = renderer.GetBoxMesh();

		// The forest is always built parented, so the flat variant can copy its world transforms.
		Scene* forest = new Scene("Hierarchy Stress");
		std::vector<Entity> spinners;
		std::vector<Entity> all;
		std::vector<bool> spins; // parallel to `all`
		const glm::vec4 colors[] = { { 0.9f, 0.5f, 0.3f, 1.0f }, { 0.4f, 0.7f, 0.9f, 1.0f }, { 0.5f, 0.85f, 0.4f, 1.0f }, { 0.85f, 0.8f, 0.4f, 1.0f } };

		auto spawn = [&](auto& self, Entity parent, int level) -> void
		{
			for (int i = 0; i < k_Branching[level]; i++)
			{
				const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Branching[level]);
				Entity entity = forest->CreateEntity("Node");
				Transform3DComponent& transform = entity.AddComponent<Transform3DComponent>(Transform3DComponent(
					{ std::cos(angle) * k_Radius[level], k_Lift[level], std::sin(angle) * k_Radius[level] }, glm::vec3(k_Scale[level])));
				transform.SetRotationEuler({ 0.0f, glm::degrees(angle), 0.0f });
				entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(box, colors[level]));
				if (parent)
					entity.SetParent(parent, false);

				all.push_back(entity);
				spins.push_back(level < k_SpinningLevels);
				if (spins.back())
					spinners.push_back(entity);
				if (level + 1 < static_cast<int>(std::size(k_Branching)))
					self(self, entity, level + 1);
			}
		};
		spawn(spawn, Entity(), 0);

		forest->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
		m_StressEntityCount = static_cast<uint32_t>(all.size());

		if (parented)
		{
			m_StressScene = forest;
			m_StressSpinners = std::move(spinners);
			return;
		}

		m_StressScene = new Scene("Hierarchy Stress Flat");
		m_StressSpinners.clear();
		for (size_t i = 0; i < all.size(); i++)
		{
			Entity flat = m_StressScene->CreateEntity("Node");
			flat.AddComponent<Transform3DComponent>(Transform3DComponent(all[i].GetWorldPosition(), all[i].GetWorldScale())).Rotation = all[i].GetWorldRotation();
			flat.AddComponent<MeshRendererComponent>(all[i].GetComponent<MeshRendererComponent>());
			if (spins[i])
				m_StressSpinners.push_back(flat);
		}
		m_StressScene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
		delete forest;
	}

	void HierarchyTest::UpdateStress(float deltaTime)
	{
		const float step = (std::min)(deltaTime, 1.0f / 30.0f);
		if (m_Animate)
			m_Time += step;
		m_StressTime += deltaTime;

		// Both variants spin the same 1,110 entities (levels 0-2 of every tree) at the same speeds;
		// only in the parented one do their subtrees follow.
		if (m_AutoOrbit)
			m_OrbitAngle = std::fmod(m_OrbitAngle + step * 8.0f, 360.0f);
		for (size_t i = 0; i < m_StressSpinners.size(); i++)
		{
			const float speed = 20.0f + static_cast<float>(i % 7) * 10.0f;
			m_StressSpinners[i].GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(glm::radians(speed * m_Time), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		const Clock::time_point updateStart = Clock::now();
		m_StressScene->OnUpdate(step);
		const Clock::time_point updateEnd = Clock::now();

		const float orbit = glm::radians(m_OrbitAngle);
		m_Camera.SetPosition({ std::sin(orbit) * 30.0f, 22.0f, std::cos(orbit) * 30.0f });
		m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		m_StressScene->SubmitLights(renderer);
		const Clock::time_point renderStart = Clock::now();
		m_StressScene->RenderEntities3D(renderer);
		const Clock::time_point renderEnd = Clock::now();
		renderer.EndScene();
		const Clock::time_point endSceneEnd = Clock::now();
		m_StressDroppedMeshes = (std::max)(m_StressDroppedMeshes, renderer.GetStatistics().DroppedMeshes);

		if (m_StressTime < k_WarmupSeconds || m_StressFrames >= k_MeasuredFrames)
			return;

		m_StressFrameMs += deltaTime * 1000.0;
		m_StressUpdateMs += Milliseconds(updateStart, updateEnd);
		m_StressRenderMs += Milliseconds(renderStart, renderEnd);
		m_StressEndSceneMs += Milliseconds(renderEnd, endSceneEnd);
		if (++m_StressFrames < k_MeasuredFrames)
			return;

		const double frames = static_cast<double>(m_StressFrames);
		m_StressResult = std::format("{} entities, {}: frame {:.2f} ms, Scene::OnUpdate {:.3f} ms, RenderEntities3D {:.2f} ms, EndScene {:.2f} ms (mean of {} frames)",
			m_StressEntityCount, m_View == View::Stress ? "parented" : "flat", m_StressFrameMs / frames, m_StressUpdateMs / frames,
			m_StressRenderMs / frames, m_StressEndSceneMs / frames, m_StressFrames);
		DE_INFO("[Stress] {}", m_StressResult);
		Check(m_StressDroppedMeshes == 0, std::format("stress: Renderer3D drew every mesh ({} dropped)", m_StressDroppedMeshes));
	}

}
