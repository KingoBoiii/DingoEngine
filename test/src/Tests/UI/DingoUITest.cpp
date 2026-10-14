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

		constexpr uint32_t k_CheckTargetSize = 128;
		constexpr uint32_t k_SlotTextureCount = 33;
		const glm::vec4 k_Background = { 0.1f, 0.1f, 0.1f, 1.0f };
		const glm::vec4 k_Red = { 1.0f, 0.0f, 0.0f, 1.0f };
		const glm::vec4 k_Green = { 0.0f, 1.0f, 0.0f, 1.0f };
		const glm::vec4 k_Blue = { 0.0f, 0.0f, 1.0f, 1.0f };
		const glm::vec4 k_Yellow = { 1.0f, 1.0f, 0.0f, 1.0f };

		bool NearColor(const glm::vec4& a, const glm::vec4& b, float epsilon = 0.02f)
		{
			return std::abs(a.r - b.r) <= epsilon && std::abs(a.g - b.g) <= epsilon && std::abs(a.b - b.b) <= epsilon;
		}

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
		m_DrawStage = 0;
		m_Checks.clear();
		m_Alive = std::make_shared<int>(0);

		m_CheckTarget = Framebuffer::Create(FramebufferParams()
			.SetDebugName("DingoUITest_CheckTarget")
			.SetWidth(static_cast<int32_t>(k_CheckTargetSize))
			.SetHeight(static_cast<int32_t>(k_CheckTargetSize))
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));

		const uint8_t yellow[4] = { 255, 255, 0, 255 };
		m_ImageTexture = Texture::CreateFromData(1, 1, yellow, TextureFormat::RGBA8_UNORM, "DingoUITest_Image");
		for (uint32_t i = 0; i < k_SlotTextureCount; ++i)
		{
			const uint8_t pixel[4] = { static_cast<uint8_t>(i * 7), static_cast<uint8_t>(255 - i * 7), 128, 255 };
			m_SlotTextures.push_back(Texture::CreateFromData(1, 1, pixel, TextureFormat::RGBA8_UNORM, "DingoUITest_Slot"));
		}

		Check(Internal::GetDingoUIContext() != nullptr, "DingoUI context exists in a windowed app");
		CheckFontMetrics();
		CheckWorldToScreen();
	}

	void DingoUITest::Cleanup()
	{
		Internal::InputBackend::SimulateMouseButton(MouseButton::Left, false);
		Internal::InputBackend::SimulateKey(k_TestKey, false);
		Internal::InputBackend::ClearConsumed();
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
		{
			context->SetTarget(nullptr);
			context->GetDrawList().Clear();
		}
		m_Alive.reset();
		for (Texture*& texture : m_SlotTextures)
			DestroyAndDelete(texture);
		m_SlotTextures.clear();
		DestroyAndDelete(m_ImageTexture);
		DestroyAndDelete(m_CheckTarget);
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

	void DingoUITest::SubmitBaseList(const glm::vec4& fill)
	{
		Internal::UIDrawList& list = Internal::GetDingoUIContext()->GetDrawList();
		list.AddRect({ { 10.0f, 10.0f }, { 60.0f, 40.0f } }, fill, 6.0f, 2.0f, k_Green);
		list.AddRect({ { 70.0f, 10.0f }, { 120.0f, 60.0f } }, k_Blue, 0.0f, 0.0f, glm::vec4(0.0f), { { 70.0f, 10.0f }, { 95.0f, 60.0f } });
		list.AddImage({ { 10.0f, 70.0f }, { 40.0f, 100.0f } }, m_ImageTexture);
		list.AddText({ 50.0f, 70.0f }, m_Font, "UI", 16.0f, glm::vec4(1.0f));
	}

	void DingoUITest::SubmitTextureList()
	{
		Internal::UIDrawList& list = Internal::GetDingoUIContext()->GetDrawList();
		for (uint32_t i = 0; i < k_SlotTextureCount; ++i)
		{
			const float x = static_cast<float>(i % 11) * 10.0f;
			const float y = static_cast<float>(i / 11) * 10.0f;
			list.AddImage({ { x, y }, { x + 8.0f, y + 8.0f } }, m_SlotTextures[i]);
		}
	}

	void DingoUITest::SubmitDemo(Framebuffer* output)
	{
		if (!output)
			return;

		Internal::UIDrawList& list = Internal::GetDingoUIContext()->GetDrawList();
		const glm::vec2 size = { static_cast<float>(output->GetWidth()), static_cast<float>(output->GetHeight()) };
		const glm::vec2 panelMin = { 24.0f, size.y * 0.5f - 80.0f };
		const glm::vec2 panelMax = panelMin + glm::vec2(320.0f, 160.0f);
		list.AddRect({ panelMin, panelMax }, { 0.08f, 0.09f, 0.12f, 0.85f }, 12.0f, 2.0f, { 0.45f, 0.55f, 0.85f, 1.0f });
		list.AddText(panelMin + glm::vec2(16.0f, 12.0f), m_Font, "DingoUI batcher", 24.0f, glm::vec4(1.0f));

		const Internal::UIRect bar = { panelMin + glm::vec2(16.0f, 52.0f), panelMin + glm::vec2(304.0f, 72.0f) };
		list.AddRect(bar, { 0.2f, 0.2f, 0.25f, 1.0f }, 10.0f);
		list.AddRect({ bar.Min, { bar.Min.x + 190.0f, bar.Max.y } }, { 0.3f, 0.8f, 0.4f, 1.0f }, glm::vec4(10.0f, 0.0f, 0.0f, 10.0f), 0.0f, glm::vec4(0.0f));

		for (uint32_t i = 0; i < 4; ++i)
		{
			const glm::vec2 min = panelMin + glm::vec2(16.0f + i * 56.0f, 88.0f);
			list.AddImage({ min, min + glm::vec2(48.0f) }, m_SlotTextures[i * 8]);
		}
		const Internal::UIRect clip = { panelMin + glm::vec2(240.0f, 88.0f), panelMin + glm::vec2(304.0f, 136.0f) };
		list.AddRect({ clip.Min - glm::vec2(16.0f), clip.Max + glm::vec2(16.0f) }, { 0.9f, 0.4f, 0.2f, 1.0f }, 24.0f, 0.0f, glm::vec4(0.0f), clip);
	}

	void DingoUITest::CheckPixels(const TexturePixels& pixels)
	{
		Check(pixels.Width == k_CheckTargetSize && pixels.Height == k_CheckTargetSize, "DingoUI check target reads back");
		Check(NearColor(pixels.GetPixel(35, 25), k_Red), "Inside a rect is its fill colour");
		Check(NearColor(pixels.GetPixel(11, 25), k_Green), "A rect's border is inset from its edge");
		Check(NearColor(pixels.GetPixel(10, 10), k_Background), "A rounded corner leaves the background");
		Check(NearColor(pixels.GetPixel(80, 30), k_Blue), "Inside the clip rect draws");
		Check(NearColor(pixels.GetPixel(110, 30), k_Background), "Outside the clip rect is the background");
		Check(NearColor(pixels.GetPixel(25, 85), k_Yellow), "An image draws its texture");

		bool text = false;
		for (uint32_t y = 70; y < 90 && !text; ++y)
			for (uint32_t x = 50; x < k_CheckTargetSize && !text; ++x)
				text = pixels.GetPixel(x, y).r > 0.5f;
		Check(text, "Text draws glyphs");
	}

	// One stage per frame: each reads the statistics and pixels of the frame before it, whose
	// EndFrame drew after that frame's Update.
	void DingoUITest::StepBatcher(Framebuffer* output)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		if (!context || !m_CheckTarget || !m_Font || !m_Font->IsValid())
			return;

		const Internal::UIRenderStatistics& statistics = context->GetRenderStatistics();
		switch (m_DrawStage)
		{
		case 0:
			break;
		case 1:
		{
			Check(statistics.DrawCalls == 1, "A frame of UI is one draw call");
			Check(statistics.Uploads == 1 && statistics.CacheHits == 0, "The first frame uploads its vertices");
			Check(statistics.Quads > 4 && statistics.CulledQuads == 0, "Every command builds quads");
			const std::weak_ptr<int> alive = m_Alive;
			m_CheckTarget->GetAttachment(0)->ReadPixels([this, alive](const TexturePixels& pixels)
			{
				if (!alive.expired())
					CheckPixels(pixels);
			});
			break;
		}
		case 2:
			Check(statistics.CacheHits == 1 && statistics.Uploads == 0, "An identical frame draws the cached vertices");
			Check(statistics.DrawCalls == 1, "A cached frame is still one draw call");
			break;
		case 3:
			Check(statistics.Uploads == 1 && statistics.CacheHits == 0, "A colour change uploads again");
			break;
		case 4:
			Check(statistics.DrawCalls == 2, "33 textures split into two draws");
			Check(statistics.Quads == k_SlotTextureCount, "Every image is one quad");
			break;
		default:
			context->SetTarget(output);
			SubmitDemo(output);
			return;
		}

		Renderer::Clear(m_CheckTarget, k_Background);
		context->SetTarget(m_CheckTarget);
		if (m_DrawStage == 3)
			SubmitTextureList();
		else
			SubmitBaseList(m_DrawStage == 2 ? glm::vec4(1.0f, 0.0f, 0.0f, 0.99f) : k_Red);
		++m_DrawStage;
	}

	void DingoUITest::Update(float deltaTime)
	{
		StepConsumeMask();
		StepBatcher(Renderer::GetRenderTarget());

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
		if (const Internal::DingoUIContext* context = Internal::GetDingoUIContext())
		{
			const Internal::UIRenderStatistics& statistics = context->GetRenderStatistics();
			ImGui::Text("UI draws %u, quads %u, culled %u, vertices %u", statistics.DrawCalls, statistics.Quads, statistics.CulledQuads, statistics.Vertices);
			ImGui::Text("UI cache hit %u, upload %u (%llu bytes)", statistics.CacheHits, statistics.Uploads, static_cast<unsigned long long>(statistics.UploadBytes));
		}
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
