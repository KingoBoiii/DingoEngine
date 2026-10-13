#include "DisplayModeTest.h"

#include <imgui.h>

#include <charconv>
#include <format>

namespace Dingo
{

	namespace
	{
		// The resize reaches the swap chain after the render thread's next present.
		constexpr uint32_t k_SettleFrames = 3;
		constexpr uint32_t k_TimeoutFrames = 300;
		constexpr std::string_view k_SettingsSlot = "display-mode-test";

		std::string ToString(const DisplayMode& mode)
		{
			return std::format("{}x{}@{}", mode.Width, mode.Height, mode.RefreshRate);
		}

		std::optional<DisplayMode> ParseMode(std::string_view text)
		{
			DisplayMode mode;
			const char* end = text.data() + text.size();
			auto [afterWidth, widthError] = std::from_chars(text.data(), end, mode.Width);
			if (widthError != std::errc() || afterWidth == end || *afterWidth != 'x')
				return std::nullopt;
			auto [afterHeight, heightError] = std::from_chars(afterWidth + 1, end, mode.Height);
			if (heightError != std::errc())
				return std::nullopt;
			if (afterHeight != end && *afterHeight == '@')
				std::from_chars(afterHeight + 1, end, mode.RefreshRate);
			return mode.IsValid() ? std::optional<DisplayMode>(mode) : std::nullopt;
		}
	}

	void DisplayModeTest::Initialize()
	{
		Renderer2DTest::Initialize();
		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		Window& window = Application::Get().GetWindow();
		if (window.IsFullscreen())
			window.SetWindowMode(WindowMode::Windowed);

		m_Checks.clear();
		m_Step = 0;
		m_StepFrames = 0;
		m_StepStarted = false;
		m_Finished = false;
		m_InitialSize = { window.GetWindowedWidth(), window.GetWindowedHeight() };
		BuildSteps();
	}

	glm::ivec2 DisplayModeTest::GetSwapChainSize() const
	{
		const SwapChain* swapChain = Application::Get().GetSwapChain();
		const Framebuffer* framebuffer = swapChain ? swapChain->GetCurrentFramebuffer() : nullptr;
		return framebuffer ? glm::ivec2(framebuffer->GetWidth(), framebuffer->GetHeight()) : glm::ivec2(0);
	}

	void DisplayModeTest::BuildSteps()
	{
		m_Steps.clear();

		m_Steps.push_back({ "the display-mode list", [] {}, nullptr, [this]
		{
			m_Modes = Application::Get().GetWindow().GetDisplayModes();
			m_Desktop = Application::Get().GetWindow().GetDesktopDisplayMode();
			DE_INFO("Display Mode Test: desktop {}, {} modes listed", ToString(m_Desktop), m_Modes.size());
			for (const DisplayMode& mode : m_Modes)
				DE_INFO("Display Mode Test:   {}", ToString(mode));

			Check(!m_Modes.empty(), std::format("the monitor lists display modes ({})", m_Modes.size()));
			Check(SortDisplayModes(m_Modes) == m_Modes, "the list has no duplicates and runs largest first");
			Check(m_Desktop.IsValid(), std::format("the desktop mode is known ({})", ToString(m_Desktop)));
			Check(std::find(m_Modes.begin(), m_Modes.end(), m_Desktop) != m_Modes.end(), "the desktop mode is in the list");

			m_Exclusive = m_Desktop;
			const std::optional<std::string_view> request = Application::Get().GetCommandLineArgs().Get("display-exclusive");
			if (request && *request == "next")
			{
				for (const DisplayMode& mode : m_Modes)
				{
					if (int64_t(mode.Width) * mode.Height < int64_t(m_Desktop.Width) * m_Desktop.Height)
					{
						m_Exclusive = mode;
						break;
					}
				}
			}
			else if (request)
			{
				if (const std::optional<DisplayMode> mode = ParseMode(*request))
					m_Exclusive = ChooseDisplayMode(m_Modes, *mode);
				else
					DE_WARN("Display Mode Test: --display-exclusive={} is not next or <W>x<H>[@<Hz>]; using the desktop's mode", *request);
			}
		} });

		m_Steps.push_back({ "borderless fullscreen", [] { Application::Get().GetWindow().SetWindowMode(WindowMode::BorderlessFullscreen); },
			[this] { return glm::ivec2(m_Desktop.Width, m_Desktop.Height); }, [this]
		{
			Check(Application::Get().GetWindow().GetWindowMode() == WindowMode::BorderlessFullscreen && Application::Get().GetWindow().IsFullscreen(), "borderless: the window is fullscreen");
			Check(!Application::Get().GetWindow().GetFullscreenDisplayMode().IsValid(), "borderless: no exclusive display mode");
			Check(Application::Get().GetWindow().GetWindowedWidth() == m_InitialSize.x && Application::Get().GetWindow().GetWindowedHeight() == m_InitialSize.y,
				std::format("borderless: the windowed size is kept ({}x{})", Application::Get().GetWindow().GetWindowedWidth(), Application::Get().GetWindow().GetWindowedHeight()));
		} });

		m_Steps.push_back({ "exclusive fullscreen", [this] { Application::Get().GetWindow().SetWindowMode(WindowMode::ExclusiveFullscreen, m_Exclusive); },
			[this] { return glm::ivec2(m_Exclusive.Width, m_Exclusive.Height); }, [this]
		{
			Check(Application::Get().GetWindow().GetWindowMode() == WindowMode::ExclusiveFullscreen && Application::Get().GetWindow().IsFullscreen(), "exclusive: the window is fullscreen");
			Check(Application::Get().GetWindow().GetFullscreenDisplayMode() == m_Exclusive,
				std::format("exclusive: runs at {} ({})", ToString(m_Exclusive), ToString(Application::Get().GetWindow().GetFullscreenDisplayMode())));
			Check(Application::Get().GetWindow().GetDesktopDisplayMode() == m_Desktop,
				std::format("exclusive: the desktop mode still reads {} ({})", ToString(m_Desktop), ToString(Application::Get().GetWindow().GetDesktopDisplayMode())));
		} });

		m_Steps.push_back({ "windowed", [] { Application::Get().GetWindow().SetWindowMode(WindowMode::Windowed); },
			[this] { return m_InitialSize; }, [this]
		{
			Check(Application::Get().GetWindow().GetWindowMode() == WindowMode::Windowed && !Application::Get().GetWindow().IsFullscreen(), "windowed: the window left fullscreen");
		} });

		m_Steps.push_back({ "SetSize while windowed", [this]
		{
			m_ResizedSize = (glm::max)(m_InitialSize - glm::ivec2(200, 100), glm::ivec2(320, 240));
			Application::Get().GetWindow().SetSize(m_ResizedSize.x, m_ResizedSize.y);
		}, [this] { return m_ResizedSize; }, [this]
		{
			Check(Application::Get().GetWindow().GetWindowedWidth() == m_ResizedSize.x && Application::Get().GetWindow().GetWindowedHeight() == m_ResizedSize.y, "SetSize: the windowed size reads back");
		} });

		m_Steps.push_back({ "SetFullscreen(true)", [] { Application::Get().GetWindow().SetFullscreen(true); },
			[this] { return glm::ivec2(m_Desktop.Width, m_Desktop.Height); }, [this]
		{
			Check(Application::Get().GetWindow().GetWindowMode() == WindowMode::BorderlessFullscreen, "SetFullscreen(true) is borderless");
		} });

		m_Steps.push_back({ "SetSize while fullscreen", [this]
		{
			m_RestoreSize = (glm::max)(m_InitialSize - glm::ivec2(300, 150), glm::ivec2(320, 240));
			Application::Get().GetWindow().SetSize(m_RestoreSize.x, m_RestoreSize.y);
		}, [this] { return glm::ivec2(m_Desktop.Width, m_Desktop.Height); }, [this]
		{
			Check(Application::Get().GetWindow().GetWindowedWidth() == m_RestoreSize.x && Application::Get().GetWindow().GetWindowedHeight() == m_RestoreSize.y,
				"SetSize while fullscreen keeps the window fullscreen and the size to restore to");
		} });

		m_Steps.push_back({ "SetFullscreen(false)", [] { Application::Get().GetWindow().SetFullscreen(false); },
			[this] { return m_RestoreSize; }, [this]
		{
			Check(!Application::Get().GetWindow().IsFullscreen(), "SetFullscreen(false) restores the size set while fullscreen");
		} });

		m_Steps.push_back({ "settings", [] {}, nullptr, [this]
		{
			Settings settings = Settings::Capture();
			Check(settings.Display.Mode == WindowMode::Windowed && settings.Display.Width == m_RestoreSize.x && settings.Display.Height == m_RestoreSize.y
				&& settings.Display.VSync == Application::Get().GetWindow().IsVSync(), "Settings::Capture reads the window");

			settings.Display.Mode = WindowMode::ExclusiveFullscreen;
			settings.Display.FullscreenMode = m_Exclusive;
			settings.Audio.MasterVolume = 0.5f;
			settings.Game["difficulty"] = 2;
			Check(settings.Save(k_SettingsSlot), "Settings::Save writes the slot");

			bool found = false;
			const Settings loaded = Settings::Load(k_SettingsSlot, &found);
			Check(found && loaded.Display.Mode == WindowMode::ExclusiveFullscreen && loaded.Display.FullscreenMode == m_Exclusive
				&& loaded.Display.Width == m_RestoreSize.x && loaded.Audio.MasterVolume == 0.5f && loaded.Game.Get("difficulty", 0) == 2,
				"Settings::Load reads back what Save wrote");
			Check(SaveSlots::Delete(k_SettingsSlot) && !SaveSlots::Exists(k_SettingsSlot), "the test's settings slot is deleted");
		} });

		m_Steps.push_back({ "restore the starting size", [this] { Application::Get().GetWindow().SetSize(m_InitialSize.x, m_InitialSize.y); },
			[this] { return m_InitialSize; }, [] {} });
	}

	void DisplayModeTest::Update(float deltaTime)
	{
		Window& window = Application::Get().GetWindow();

		if (!m_Finished && m_Step < m_Steps.size())
		{
			Step& step = m_Steps[m_Step];
			if (!m_StepStarted)
			{
				step.Action();
				m_StepStarted = true;
				m_StepFrames = 0;
			}

			++m_StepFrames;
			bool done = m_StepFrames > k_SettleFrames;
			glm::ivec2 expected(0);
			const glm::ivec2 windowSize(window.GetWidth(), window.GetHeight());
			const glm::ivec2 swapChainSize = GetSwapChainSize();
			if (step.ExpectedSize)
			{
				expected = step.ExpectedSize();
				done = done && windowSize == expected && swapChainSize == expected;
			}

			if (done || m_StepFrames >= k_TimeoutFrames)
			{
				if (step.ExpectedSize)
				{
					Check(windowSize == expected && swapChainSize == expected,
						std::format("{}: the window and swap chain are {}x{} (window {}x{}, swap chain {}x{}, {} frames)",
							step.Name, expected.x, expected.y, windowSize.x, windowSize.y, swapChainSize.x, swapChainSize.y, m_StepFrames));
				}
				step.Verify();

				m_StepStarted = false;
				if (++m_Step == m_Steps.size())
				{
					m_Finished = true;
					DE_INFO("Display Mode Test: done, {} checks, {} failed", m_Checks.size(), m_Checks.FailedCount());
				}
			}
		}

		m_Status = m_Finished ? std::format("Done: {} checks, {} failed", m_Checks.size(), m_Checks.FailedCount())
			: std::format("Step {}/{}: {}", m_Step + 1, m_Steps.size(), m_Step < m_Steps.size() ? m_Steps[m_Step].Name : std::string());

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		if (m_Font)
		{
			const std::string lines[] = {
				m_Status,
				std::format("Window {}x{}, {}", window.GetWidth(), window.GetHeight(), WindowModeToString(window.GetWindowMode())),
				std::format("Desktop {}, {} modes", ToString(window.GetDesktopDisplayMode()), m_Modes.size()),
			};
			glm::vec2 position(-2.6f, 2.1f);
			for (const std::string& line : lines)
			{
				m_Renderer->DrawText(line, m_Font, position, 0.16f);
				position.y -= 0.26f;
			}
		}
		m_Renderer->EndScene();
	}

	void DisplayModeTest::Cleanup()
	{
		Window& window = Application::Get().GetWindow();
		if (!m_Finished)
		{
			window.SetWindowMode(WindowMode::Windowed);
			window.SetSize(m_InitialSize.x, m_InitialSize.y);
		}

		DestroyAndDelete(m_Font);
		m_Checks.clear();
		Renderer2DTest::Cleanup();
	}

	void DisplayModeTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		Window& window = Application::Get().GetWindow();
		ImGui::Separator();
		ImGui::TextUnformatted(m_Status.c_str());
		if (ImGui::Button("Run again") && m_Finished)
		{
			Cleanup();
			Initialize();
		}

		ImGui::Separator();
		if (ImGui::Button("Windowed"))
			window.SetWindowMode(WindowMode::Windowed);
		ImGui::SameLine();
		if (ImGui::Button("Borderless"))
			window.SetWindowMode(WindowMode::BorderlessFullscreen);
		for (const DisplayMode& mode : m_Modes)
		{
			if (ImGui::Selectable(std::format("Exclusive {}", ToString(mode)).c_str(), window.GetFullscreenDisplayMode() == mode))
				window.SetWindowMode(WindowMode::ExclusiveFullscreen, mode);
		}

		ImGui::Separator();
		for (const CheckResult& result : m_Checks)
			ImGui::TextColored(result.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s %s", result.Passed ? "PASS" : "FAIL", result.Name.c_str());
	}

}
