#include "BackgroundTest.h"

#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

#include <cmath>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr float k_MinUpdateRate = 20.0f;
		constexpr float k_MaxDeltaTime = 0.1f;
		// The return's wait in Window::WaitEvents, before the event that ends the stretch arrived,
		// is in the summed delta times but not in the wall clock, and lasts up to two 15.6 ms ticks.
		constexpr float k_DeltaTimeTolerance = 0.05f;
		// Shorter stretches hold too few updates to rate, or none: leaving and coming back can land
		// in one event poll.
		constexpr float k_MinRatedSpan = 0.5f;
		// The second delta after a pause is one ordinary frame; a leak of the wait that ended the
		// pause would add up to the 0.1 s it sleeps on window events.
		constexpr float k_MaxSecondDeltaAfterPause = 0.05f;
	}

	void BackgroundTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Font = Font::Create("assets/fonts/ArialBD.ttf");

		m_AppUpdateInBackground = Application::Get().GetUpdateInBackground();
		if (auto value = Application::Get().GetCommandLineArgs().Get("update-in-background"))
			Application::Get().SetUpdateInBackground(*value != "off");

		m_Time = 0.0f;
		m_Updates = 0;
		m_SpacePresses = 0;
		m_SpaceReleases = 0;
		m_Minimized = false;
		m_Unfocused = false;
		m_ReturnPending = false;
		m_Stretches = 0;
		m_LastStretch.clear();
		m_Checks.clear();
		m_LastUpdate = Clock::now();

		BuildScene();
	}

	void BackgroundTest::BuildScene()
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_Scene = new Scene("Background Test");

		Entity camera = m_Scene->CreateEntity("Camera");
		Transform3DComponent& cameraTransform = camera.AddComponent<Transform3DComponent>();
		cameraTransform.Position = { 0.0f, 2.0f, 6.0f };
		cameraTransform.Rotation = glm::quat(glm::radians(glm::vec3(-18.0f, 0.0f, 0.0f)));
		camera.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;

		m_Scene->CreateEntity("Overlay Camera").AddComponent<CameraComponent>();

		DirectionalLightComponent& sun = m_Scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
		sun.Intensity = 0.5f;
		sun.Ambient = 0.2f;

		m_Light = m_Scene->CreateEntity("Light");
		m_Light.AddComponent<Transform3DComponent>();
		m_Light.AddComponent<PointLightComponent>(PointLightComponent({ 1.0f, 0.75f, 0.4f }, 2.0f, 6.0f));

		Entity floor = m_Scene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -1.0f, 0.0f }, { 8.0f, 0.2f, 8.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.45f, 0.48f, 0.44f, 1.0f }));

		m_Cube = m_Scene->CreateEntity("Cube");
		m_Cube.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 0.2f, 0.0f }, glm::vec3(1.2f)));
		m_Cube.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.85f, 0.45f, 0.2f, 1.0f }));

		m_Marker = m_Scene->CreateEntity("Marker");
		m_Marker.GetComponent<TransformComponent>() = TransformComponent({ 0.0f, -4.2f, 0.0f }, { 0.6f, 0.6f });
		m_Marker.AddComponent<SpriteRendererComponent>(SpriteRendererComponent({ 0.3f, 0.8f, 0.95f, 1.0f }));
	}

	void BackgroundTest::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& resize)
		{
			SetBackground(resize.GetWidth() == 0 || resize.GetHeight() == 0, m_Unfocused);
			return false;
		});
		dispatcher.Dispatch<WindowFocusEvent>([this](WindowFocusEvent& focus)
		{
			SetBackground(m_Minimized, !focus.IsFocused());
			return false;
		});
	}

	void BackgroundTest::SetBackground(bool minimized, bool unfocused)
	{
		const bool wasInBackground = m_Minimized || m_Unfocused;
		const bool inBackground = minimized || unfocused;
		m_Minimized = minimized;
		m_Unfocused = unfocused;

		if (inBackground && !wasInBackground)
		{
			m_StretchStart = m_LastUpdate;
			m_StretchUpdatingMode = Application::Get().GetUpdateInBackground();
			m_StretchMinimized = false;
			m_StretchUpdates = 0;
			m_StretchDeltaTime = 0.0f;
			m_StretchMaxDeltaTime = 0.0f;
			m_StretchRenderMismatch = false;
			m_StretchSpaceEdges = 0;
			m_StretchAspectKept = true;
			m_StretchMinimizedAspect = m_ShownAspect;
			m_ReturnPending = false;
			m_CheckSecondDelta = false;
		}
		m_StretchMinimized = m_StretchMinimized || minimized;

		if (!inBackground && wasInBackground)
			m_ReturnPending = true;
	}

	void BackgroundTest::Update(float deltaTime)
	{
		const Clock::time_point now = Clock::now();

		const uint32_t pressed = Input::IsKeyPressed(KeyCode::Space) ? 1 : 0;
		const uint32_t released = Input::IsKeyReleased(KeyCode::Space) ? 1 : 0;
		m_SpacePresses += pressed;
		m_SpaceReleases += released;

		if (m_CheckSecondDelta && !(m_Minimized || m_Unfocused))
		{
			m_CheckSecondDelta = false;
			Check(deltaTime <= k_MaxSecondDeltaAfterPause,
				std::format("the second delta after the pause leaves the paused time out too ({:.0f} ms)", deltaTime * 1000.0f));
		}

		if (m_Minimized || m_Unfocused)
		{
			++m_StretchUpdates;
			m_StretchDeltaTime += deltaTime;
			if (deltaTime > m_StretchMaxDeltaTime)
				m_StretchMaxDeltaTime = deltaTime;
			if (Renderer::IsFrameSkipped() != Application::Get().IsMinimized())
				m_StretchRenderMismatch = true;
			m_StretchSpaceEdges += pressed + released;
		}
		else if (m_ReturnPending)
		{
			m_ReturnPending = false;
			CheckStretch(deltaTime, now);
		}

		const float aspect = Application::Get().GetWindow().GetAspectRatio();
		if (!Application::Get().IsMinimized())
		{
			m_ShownAspect = aspect;
		}
		else
		{
			m_StretchMinimizedAspect = aspect;
			if (!(aspect == m_ShownAspect))
				m_StretchAspectKept = false;
		}

		m_LastUpdate = now;
		++m_Updates;
		m_Time += deltaTime;

		m_Cube.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(m_Time, glm::normalize(glm::vec3(0.3f, 1.0f, 0.0f)));
		m_Light.GetComponent<Transform3DComponent>().Position = { 2.5f * std::cos(m_Time * 0.7f), 1.5f, 2.5f * std::sin(m_Time * 0.7f) };
		m_Marker.GetComponent<TransformComponent>().Position.x = 6.0f * std::sin(m_Time * 0.5f);

		Application::Get().GetSceneRenderer().Render(*m_Scene);

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		if (m_Font)
		{
			const std::string lines[] = {
				std::format("Updates: {}, stretches in the background: {}", m_Updates, m_Stretches),
				std::format("Update in background: {}", Application::Get().GetUpdateInBackground() ? "on" : "off (pauses)"),
				m_LastStretch.empty() ? std::string("Minimize or click away, holding Space across it, then come back") : "Last: " + m_LastStretch,
				std::format("Space: {} presses, {} releases", m_SpacePresses, m_SpaceReleases),
			};

			constexpr float textSize = 0.16f;
			constexpr float lineHeight = 0.26f;
			glm::vec2 position(-2.6f, 2.1f);
			for (const std::string& line : lines)
			{
				m_Renderer->DrawText(line, m_Font, position, textSize);
				position.y -= lineHeight;
			}
		}
		m_Renderer->EndScene();
	}

	void BackgroundTest::CheckStretch(float returnDeltaTime, Clock::time_point now)
	{
		++m_Stretches;
		m_Checks.clear();

		const char* state = m_StretchMinimized ? "minimized" : "unfocused";
		const float wall = std::chrono::duration<float>(now - m_StretchStart).count();

		if (m_StretchUpdatingMode)
		{
			// The return's own delta covers the time since the last update in the background.
			const float summed = m_StretchDeltaTime + returnDeltaTime;
			const float rate = static_cast<float>(m_StretchUpdates) / wall;

			m_LastStretch = std::format("{} updates over {:.2f} s {} ({:.0f}/s), longest delta {:.0f} ms",
				m_StretchUpdates, wall, state, rate, m_StretchMaxDeltaTime * 1000.0f);
			DE_INFO("Background Test: {}", m_LastStretch);

			if (wall >= k_MinRatedSpan)
			{
				Check(rate >= k_MinUpdateRate,
					std::format("layers kept updating while {} ({:.0f} updates/s, at least {:.0f})", state, rate, k_MinUpdateRate));
			}
			Check(m_StretchMaxDeltaTime <= k_MaxDeltaTime,
				std::format("no update waited more than {:.0f} ms ({:.0f} ms)", k_MaxDeltaTime * 1000.0f, m_StretchMaxDeltaTime * 1000.0f));
			Check(std::abs(summed - wall) <= k_DeltaTimeTolerance,
				std::format("the delta times add up to the wall clock ({:.3f} s of {:.3f} s)", summed, wall));
			Check(!m_StretchRenderMismatch && !Renderer::IsFrameSkipped(),
				"updates rendered exactly while the window was not minimized, and the return renders");
			if (m_StretchMinimized && m_StretchUpdates > 0)
			{
				Check(m_StretchAspectKept,
					std::format("Window::GetAspectRatio kept the window's aspect while minimized ({:.3f}, before {:.3f})", m_StretchMinimizedAspect, m_ShownAspect));
			}
		}
		else
		{
			m_LastStretch = std::format("paused {:.2f} s {}, {} updates, first delta after it {:.0f} ms",
				wall, state, m_StretchUpdates, returnDeltaTime * 1000.0f);
			DE_INFO("Background Test: {}", m_LastStretch);

			Check(m_StretchUpdates == 0,
				std::format("the app paused while {} ({} updates ran)", state, m_StretchUpdates));
			if (wall >= k_MinRatedSpan)
			{
				Check(returnDeltaTime <= k_MaxDeltaTime,
					std::format("the first delta after the pause leaves the paused time out ({:.0f} ms)", returnDeltaTime * 1000.0f));
				m_CheckSecondDelta = true;
			}
		}

		const uint32_t held = Input::IsKeyDown(KeyCode::Space) ? 1 : 0;
		Check(m_SpacePresses == m_SpaceReleases + held,
			std::format("no Space edge lost or doubled ({} presses, {} releases, {} edges in the background)", m_SpacePresses, m_SpaceReleases, m_StretchSpaceEdges));
	}

	void BackgroundTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void BackgroundTest::Cleanup()
	{
		Application::Get().SetUpdateInBackground(m_AppUpdateInBackground);

		delete m_Scene;
		m_Scene = nullptr;
		DestroyAndDelete(m_Font);
		m_Checks.clear();

		Renderer2DTest::Cleanup();
	}

	void BackgroundTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		ImGui::Separator();
		bool updateInBackground = Application::Get().GetUpdateInBackground();
		if (ImGui::Checkbox("Update in background", &updateInBackground))
			Application::Get().SetUpdateInBackground(updateInBackground);
		ImGui::Text("Stretches in the background: %u", m_Stretches);
		if (!m_LastStretch.empty())
			ImGui::TextWrapped("%s", m_LastStretch.c_str());
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
