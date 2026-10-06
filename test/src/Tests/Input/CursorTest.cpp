#include "CursorTest.h"

#include <imgui.h>

#include <format>

namespace Dingo
{

	void CursorTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Font = Font::Create("assets/fonts/ArialBD.ttf");

		Input::SetCursorMode(CursorMode::Normal);
		m_LookYaw = 0.0f;
		m_LookPitch = 0.0f;
	}

	void CursorTest::Update(float deltaTime)
	{
		if (Input::IsKeyPressed(KeyCode::D1))
			Input::SetCursorMode(CursorMode::Normal);
		if (Input::IsKeyPressed(KeyCode::D2))
			Input::SetCursorMode(CursorMode::Hidden);
		if (Input::IsKeyPressed(KeyCode::D3))
			Input::SetCursorMode(CursorMode::Locked);
		if (Input::IsKeyPressed(KeyCode::R))
			Input::SetRawMouseMotion(!Input::IsRawMouseMotionEnabled());
		if (Input::IsKeyPressed(KeyCode::Escape))
			Input::SetCursorMode(CursorMode::Normal);

		const glm::vec2 delta = Input::GetMouseDelta();

		// Only accumulate look while Locked -- Normal/Hidden leave the OS cursor
		// free to leave the window, so its delta isn't a meaningful look input.
		if (Input::GetCursorMode() == CursorMode::Locked)
		{
			m_LookYaw += delta.x * m_LookSensitivity;
			m_LookPitch += delta.y * m_LookSensitivity;
		}

		const char* modeName = "Normal";
		switch (Input::GetCursorMode())
		{
			case CursorMode::Normal: modeName = "Normal"; break;
			case CursorMode::Hidden: modeName = "Hidden"; break;
			case CursorMode::Locked: modeName = "Locked"; break;
		}

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);

		if (m_Font)
		{
			const bool focused = Application::Get().GetWindow().IsFocused();
			const std::string lines[] = {
				std::format("Mode [1/2/3]: {}", modeName),
				std::format("Focused: {}", focused ? "yes" : "no"),
				std::format("Raw [R]: {} (supported: {})", Input::IsRawMouseMotionEnabled() ? "on" : "off", Input::IsRawMouseMotionSupported() ? "yes" : "no"),
				std::format("Delta: {:+.1f}, {:+.1f}", delta.x, delta.y),
				std::format("Look: {:+.1f}, {:+.1f}", m_LookYaw, m_LookPitch),
				"Escape: back to Normal",
			};

			constexpr float textSize = 0.2f;
			constexpr float lineHeight = 0.32f;
			glm::vec2 position(-2.6f, 1.6f);
			for (const std::string& line : lines)
			{
				m_Renderer->DrawText(line, m_Font, position, textSize);
				position.y -= lineHeight;
			}
		}

		m_Renderer->EndScene();
	}

	void CursorTest::Cleanup()
	{
		// Leaving the cursor Locked would strand the test picker's own UI.
		Input::SetCursorMode(CursorMode::Normal);

		DestroyAndDelete(m_Font);

		Renderer2DTest::Cleanup();
	}

	void CursorTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		ImGui::Separator();
		ImGui::TextUnformatted("1 Normal | 2 Hidden | 3 Locked | R toggle raw motion | Escape -> Normal");
	}

}
