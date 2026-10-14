#include "DingoUITest.h"
#include "DingoEngine/Core/InputBackend.h"
#include "DingoEngine/UI/DingoUI/DingoUIContext.h"

#include <imgui.h>

#include <cmath>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_WrapText = "The quick brown fox jumps over the lazy dog";
		constexpr float k_WrapTextSize = 0.4f;
		constexpr float k_WrapWidth = 4.0f;
		constexpr KeyCode k_TestKey = Key::Pause;

		bool Near(float a, float b, float epsilon = 1e-4f)
		{
			return std::abs(a - b) <= epsilon;
		}
	}

	void DingoUITest::Initialize()
	{
		Renderer2DTest::Initialize();
		m_Font = Font::Create("fonts/Inter-Bold.ttf");
		m_Stage = 0;
		m_Checks.clear();

		Check(Internal::GetDingoUIContext() != nullptr, "DingoUI context exists in a windowed app");
		CheckFontMetrics();
		CheckWorldToScreen();
	}

	void DingoUITest::Cleanup()
	{
		Internal::InputBackend::SimulateMouseButton(MouseButton::Left, false);
		Internal::InputBackend::SimulateKey(k_TestKey, false);
		Internal::InputBackend::ClearConsumed();
		DestroyAndDelete(m_Font);
		Renderer2DTest::Cleanup();
	}

	void DingoUITest::CheckFontMetrics()
	{
		Check(m_Font && m_Font->IsValid(), "Font loads");
		if (!m_Font || !m_Font->IsValid())
			return;

		const float size = 0.5f;
		Check(m_Font->GetLineHeight(size) > 0.0f, "GetLineHeight is positive");
		Check(Near(m_Font->GetLineHeight(size, 0.2f), m_Font->GetLineHeight(size) + 0.2f * size), "GetLineHeight adds lineSpacing in units of the size");
		Check(m_Font->GetAscender(size) > 0.0f && m_Font->GetDescender(size) < 0.0f, "Ascender above the baseline, descender below");
		Check(Near(m_Font->GetAscender(size) - m_Font->GetDescender(size), size), "Ascender minus descender is the size");

		const TextMetrics single = m_Font->MeasureText(k_WrapText, k_WrapTextSize);
		Check(single.LineCount == 1, "Unwrapped text is one line");
		Check(single.Width == m_Font->GetStringWidth(k_WrapText, k_WrapTextSize), "MeasureText width matches GetStringWidth");
		Check(Near(single.Height, k_WrapTextSize), "One line is one size high");
		Check(m_Font->MeasureText(k_WrapText, k_WrapTextSize, single.Width * 2.0f).Width == single.Width, "A MaxWidth wider than the text changes nothing");
		Check(m_Font->MeasureText(k_WrapText, k_WrapTextSize, 0.0f, 0.05f).Width == m_Font->GetStringWidth(k_WrapText, k_WrapTextSize, 0.05f), "Kerning measures the same both ways");

		const TextMetrics wrapped = m_Font->MeasureText(k_WrapText, k_WrapTextSize, k_WrapWidth);
		Check(wrapped.LineCount > 1, "Text wider than MaxWidth wraps");
		Check(wrapped.Width <= k_WrapWidth, "Wrapped lines fit MaxWidth");

		const TextMetrics lines = m_Font->MeasureText("one\ntwo\n", size);
		Check(lines.LineCount == 3, "A trailing newline ends on an empty line");
		Check(Near(lines.Height, size + 2.0f * m_Font->GetLineHeight(size)), "Height adds a line height per extra line");
		Check(m_Font->MeasureText("", size).LineCount == 0, "Empty text has no lines");
	}

	void DingoUITest::CheckWorldToScreen()
	{
		const glm::vec2 viewport = { 1280.0f, 720.0f };
		PerspectiveCamera camera(45.0f, viewport.x / viewport.y, 0.1f, 100.0f);
		camera.SetPosition({ 0.0f, 0.0f, 5.0f });
		camera.SetTarget({ 0.0f, 0.0f, 0.0f });

		const std::optional<glm::vec2> center = camera.WorldToScreenPoint({ 0.0f, 0.0f, 0.0f }, viewport);
		Check(center && Near(center->x, viewport.x * 0.5f, 0.01f) && Near(center->y, viewport.y * 0.5f, 0.01f), "The camera's target projects to the viewport centre");

		const glm::vec3 point = { 1.0f, 0.5f, -1.0f };
		const std::optional<glm::vec2> screen = camera.WorldToScreenPoint(point, viewport);
		bool roundTrip = false;
		if (screen)
		{
			const Ray ray = camera.ScreenPointToRay(*screen, viewport);
			const glm::vec3 toPoint = point - ray.Origin;
			roundTrip = glm::length(toPoint - ray.Direction * glm::dot(toPoint, ray.Direction)) < 1e-3f;
		}
		Check(roundTrip, "ScreenPointToRay of WorldToScreenPoint passes through the point");
		Check(screen && screen->y < viewport.y * 0.5f, "Up in the world is up on screen (+Y down)");
		Check(!camera.WorldToScreenPoint({ 0.0f, 0.0f, 10.0f }, viewport), "A point behind the camera has no screen position");

		Scene scene("DingoUI Test");
		Check(!scene.WorldToScreenPoint({ 0.0f, 0.0f, 0.0f }, viewport), "Scene without a camera projects nothing");
		Entity cameraEntity = scene.CreateEntity("Camera");
		cameraEntity.AddComponent<Transform3DComponent>().Position = { 0.0f, 0.0f, 5.0f };
		cameraEntity.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;
		const std::optional<glm::vec2> sceneCenter = scene.WorldToScreenPoint({ 0.0f, 0.0f, 0.0f }, viewport);
		Check(sceneCenter && Near(sceneCenter->x, viewport.x * 0.5f, 0.01f) && Near(sceneCenter->y, viewport.y * 0.5f, 0.01f), "Scene::WorldToScreenPoint projects through the primary camera");
		Check(!scene.WorldToScreenPoint({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }), "Scene::WorldToScreenPoint of an empty viewport is nullopt");
	}

	// One step per frame: Input::Update between them moves current state into previous and
	// releases mask bits, which is what is under test.
	void DingoUITest::StepConsumeMask()
	{
		using Internal::InputBackend;
		const MouseButton left = MouseButton::Left;

		switch (m_Stage)
		{
		case 0:
			InputBackend::SimulateMouseButton(left, true);
			Check(Input::IsMouseButtonPressed(left), "Simulated press is visible");
			InputBackend::ConsumeMouseButton(left);
			Check(!Input::IsMouseButtonPressed(left) && !Input::IsMouseButtonDown(left), "Consumed press reads as up");
			Check(!Input::IsAnyMouseButtonDown(), "Consumed press hides from IsAnyMouseButtonDown");
			break;
		case 1:
			Check(!Input::IsMouseButtonDown(left), "Consumed button stays hidden while held");
			InputBackend::SimulateMouseButton(left, false);
			Check(!Input::IsMouseButtonReleased(left), "Release of a consumed button is hidden");
			break;
		case 2:
			InputBackend::SimulateMouseButton(left, true);
			Check(Input::IsMouseButtonPressed(left), "The press after a consumed release is visible");
			break;
		case 3:
			Check(Input::IsMouseButtonDown(left), "An unconsumed held button stays visible");
			InputBackend::ConsumeNewPresses(true, true);
			Check(Input::IsMouseButtonDown(left), "ConsumeNewPresses leaves a press that started earlier");
			InputBackend::SimulateKey(k_TestKey, true);
			InputBackend::ConsumeNewPresses(false, true);
			Check(!Input::IsKeyPressed(k_TestKey) && !Input::IsKeyDown(k_TestKey), "ConsumeNewPresses hides a new key press");
			Check(InputBackend::IsKeyPressedUnconsumed(k_TestKey), "Engine hotkeys still see a consumed key");
			break;
		case 4:
			InputBackend::SimulateMouseButton(left, false);
			Check(Input::IsMouseButtonReleased(left), "An unconsumed release is visible");
			InputBackend::SimulateKey(k_TestKey, false);
			Check(!Input::IsKeyReleased(k_TestKey), "Release of a consumed key is hidden");
			break;
		case 5:
			InputBackend::SimulateKey(k_TestKey, true);
			Check(Input::IsKeyPressed(k_TestKey), "The key press after a consumed release is visible");
			InputBackend::SimulateKey(k_TestKey, false);
			break;
		default:
			return;
		}
		++m_Stage;
	}

	void DingoUITest::Update(float deltaTime)
	{
		StepConsumeMask();

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		if (m_Font)
		{
			const glm::vec2 origin = { -k_WrapWidth * 0.5f, 1.5f };
			const TextMetrics metrics = m_Font->MeasureText(k_WrapText, k_WrapTextSize, k_WrapWidth);
			m_Renderer->DrawQuad(glm::vec2(0.0f, origin.y + m_Font->GetAscender(k_WrapTextSize) - metrics.Height * 0.5f), glm::vec2(k_WrapWidth, metrics.Height), glm::vec4(0.2f, 0.25f, 0.35f, 1.0f));

			Renderer2DTextParameters params;
			params.MaxWidth = k_WrapWidth;
			m_Renderer->DrawText(k_WrapText, m_Font, origin, k_WrapTextSize, params);
			m_Renderer->DrawText(k_WrapText, m_Font, glm::vec2(-4.5f, -1.5f), k_WrapTextSize * 0.75f);
		}
		m_Renderer->EndScene();
	}

	void DingoUITest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		ImGui::Separator();
		ImGui::Text("Pointer over UI: %s", DingoUI::IsPointerOverUI() ? "yes" : "no");
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
