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
		constexpr uint32_t k_LayoutWidth = 960;
		constexpr uint32_t k_LayoutHeight = 540;
		constexpr float k_LayoutW = static_cast<float>(k_LayoutWidth);
		constexpr float k_LayoutH = static_cast<float>(k_LayoutHeight);
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

		m_LayoutTarget = Framebuffer::Create(FramebufferParams()
			.SetDebugName("DingoUITest_LayoutTarget")
			.SetWidth(static_cast<int32_t>(k_LayoutWidth))
			.SetHeight(static_cast<int32_t>(k_LayoutHeight))
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_LayoutStage = 0;
		DingoUI::SetDefaultFont(m_Font);

		Check(Internal::GetDingoUIContext() != nullptr, "DingoUI context exists in a windowed app");
		Check(DingoUI::GetDefaultFont() == m_Font, "SetDefaultFont sets the font Text uses");
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
			context->SetInputOverride(nullptr);
			context->GetDrawList().Clear();
		}
		DingoUI::SetDefaultFont(nullptr);
		m_Alive.reset();
		for (Texture*& texture : m_SlotTextures)
			DestroyAndDelete(texture);
		m_SlotTextures.clear();
		DestroyAndDelete(m_ImageTexture);
		DestroyAndDelete(m_CheckTarget);
		DestroyAndDelete(m_LayoutTarget);
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
	bool DingoUITest::StepBatcher()
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		if (!context || !m_CheckTarget || !m_Font || !m_Font->IsValid())
			return false;

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
			return false;
		}

		Renderer::Clear(m_CheckTarget, k_Background);
		context->SetTarget(m_CheckTarget);
		if (m_DrawStage == 3)
			SubmitTextureList();
		else
			SubmitBaseList(m_DrawStage == 2 ? glm::vec4(1.0f, 0.0f, 0.0f, 0.99f) : k_Red);
		++m_DrawStage;
		return true;
	}

	bool DingoUITest::RectIs(const char* name, float minX, float minY, float maxX, float maxY, float tolerance)
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		const Internal::UIElement* element = context ? context->GetLastLayout().FindByName(name) : nullptr;
		if (!element)
			return false;
		const Internal::UIRect& rect = element->Rect;
		return Near(rect.Min.x, minX, tolerance) && Near(rect.Min.y, minY, tolerance) && Near(rect.Max.x, maxX, tolerance) && Near(rect.Max.y, maxY, tolerance);
	}

	bool DingoUITest::SizeIs(const char* name, float width, float height, float tolerance)
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		const Internal::UIElement* element = context ? context->GetLastLayout().FindByName(name) : nullptr;
		if (!element)
			return false;
		const Internal::UIRect& rect = element->Rect;
		return Near(rect.Max.x - rect.Min.x, width, tolerance) && Near(rect.Max.y - rect.Min.y, height, tolerance);
	}

	void DingoUITest::DeclareAnchors()
	{
		struct Entry { const char* Name; UIAnchor Anchor; };
		const Entry entries[] = {
			{ "a_tl", UIAnchor::TopLeft }, { "a_t", UIAnchor::Top }, { "a_tr", UIAnchor::TopRight },
			{ "a_l", UIAnchor::Left }, { "a_c", UIAnchor::Center }, { "a_r", UIAnchor::Right },
			{ "a_bl", UIAnchor::BottomLeft }, { "a_b", UIAnchor::Bottom }, { "a_br", UIAnchor::BottomRight },
			{ "a_stretch", UIAnchor::Stretch } };
		for (const Entry& entry : entries)
		{
			DingoUI::BeginRoot(entry.Name, UIRootParams()
				.SetAnchor(entry.Anchor)
				.SetLayout(UIBoxParams().SetSize(UISize::Fixed(100.0f), UISize::Fixed(50.0f))));
			DingoUI::EndRoot();
		}
		DingoUI::BeginRoot("a_offset", UIRootParams()
			.SetOffset({ 10.0f, 20.0f })
			.SetLayout(UIBoxParams().SetSize(UISize::Fixed(100.0f), UISize::Fixed(50.0f))));
		DingoUI::EndRoot();
		DingoUI::BeginRoot("a_pivot", UIRootParams()
			.SetAnchor(UIAnchor::Center)
			.SetPivot({ 1.0f, 1.0f })
			.SetLayout(UIBoxParams().SetSize(UISize::Fixed(100.0f), UISize::Fixed(50.0f))));
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyAnchors()
	{
		const float cx = k_LayoutW * 0.5f;
		const float cy = k_LayoutH * 0.5f;
		Check(RectIs("a_tl", 0, 0, 100, 50), "Anchor TopLeft");
		Check(RectIs("a_t", cx - 50, 0, cx + 50, 50), "Anchor Top");
		Check(RectIs("a_tr", k_LayoutW - 100, 0, k_LayoutW, 50), "Anchor TopRight");
		Check(RectIs("a_l", 0, cy - 25, 100, cy + 25), "Anchor Left");
		Check(RectIs("a_c", cx - 50, cy - 25, cx + 50, cy + 25), "Anchor Center");
		Check(RectIs("a_r", k_LayoutW - 100, cy - 25, k_LayoutW, cy + 25), "Anchor Right");
		Check(RectIs("a_bl", 0, k_LayoutH - 50, 100, k_LayoutH), "Anchor BottomLeft");
		Check(RectIs("a_b", cx - 50, k_LayoutH - 50, cx + 50, k_LayoutH), "Anchor Bottom");
		Check(RectIs("a_br", k_LayoutW - 100, k_LayoutH - 50, k_LayoutW, k_LayoutH), "Anchor BottomRight");
		Check(RectIs("a_stretch", 0, 0, k_LayoutW, k_LayoutH), "Anchor Stretch fills the target");
		Check(RectIs("a_offset", 10, 20, 110, 70), "A root's Offset moves it from its anchor");
		Check(RectIs("a_pivot", cx - 100, cy - 50, cx, cy), "A Pivot replaces the anchor's own point");
	}

	void DingoUITest::DeclareStacks()
	{
		const UISize fixed20 = UISize::Fixed(20.0f);
		DingoUI::BeginRoot("s_row", UIRootParams().SetLayout(UIBoxParams().SetDirection(UIDirection::Row).SetGap(10.0f).SetPadding(5.0f)));
		for (const char* id : { "s_a", "s_b", "s_c" })
		{
			DingoUI::BeginBox(UIBoxParams().SetId(id).SetSize(UISize::Fixed(40.0f), fixed20));
			DingoUI::EndBox();
		}
		DingoUI::EndRoot();

		DingoUI::BeginRoot("s_col", UIRootParams().SetOffset({ 300.0f, 0.0f }).SetLayout(UIBoxParams().SetGap(8.0f).SetPadding(4.0f, 6.0f)));
		for (const char* id : { "s_d", "s_e" })
		{
			DingoUI::BeginBox(UIBoxParams().SetId(id).SetSize(UISize::Fixed(30.0f), UISize::Fixed(10.0f)));
			DingoUI::EndBox();
		}
		DingoUI::EndRoot();

		DingoUI::BeginRoot("s_between", UIRootParams().SetOffset({ 0.0f, 200.0f }).SetLayout(UIBoxParams()
			.SetDirection(UIDirection::Row).SetJustify(UIJustify::SpaceBetween).SetSize(UISize::Fixed(100.0f), UISize::Fixed(40.0f))));
		for (const char* id : { "s_s0", "s_s1", "s_s2" })
		{
			DingoUI::BeginBox(UIBoxParams().SetId(id).SetSize(fixed20, UISize::Fixed(10.0f)));
			DingoUI::EndBox();
		}
		DingoUI::EndRoot();

		DingoUI::BeginRoot("s_center", UIRootParams().SetOffset({ 200.0f, 200.0f }).SetLayout(UIBoxParams()
			.SetDirection(UIDirection::Row).SetAlign(UIAlign::Center).SetJustify(UIJustify::End).SetSize(UISize::Fixed(100.0f), UISize::Fixed(40.0f))));
		DingoUI::BeginBox(UIBoxParams().SetId("s_cc").SetSize(fixed20, UISize::Fixed(10.0f)));
		DingoUI::EndBox();
		DingoUI::EndRoot();

		DingoUI::BeginRoot("s_stretch", UIRootParams().SetOffset({ 400.0f, 200.0f }).SetLayout(UIBoxParams()
			.SetAlign(UIAlign::Stretch).SetSize(UISize::Fixed(100.0f), UISize::Fixed(40.0f))));
		DingoUI::BeginBox(UIBoxParams().SetId("s_st").SetHeight(UISize::Fixed(10.0f)));
		DingoUI::EndBox();
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyStacks()
	{
		Check(RectIs("s_row", 0, 0, 150, 30), "A row fits its children, gaps and padding");
		Check(RectIs("s_a", 5, 5, 45, 25) && RectIs("s_b", 55, 5, 95, 25) && RectIs("s_c", 105, 5, 145, 25), "A row places children left to right with the gap");
		Check(RectIs("s_col", 300, 0, 338, 40), "A column fits its children, gaps and padding");
		Check(RectIs("s_d", 304, 6, 334, 16) && RectIs("s_e", 304, 24, 334, 34), "A column places children top to bottom with the gap");
		Check(RectIs("s_s0", 0, 200, 20, 210) && RectIs("s_s1", 40, 200, 60, 210) && RectIs("s_s2", 80, 200, 100, 210), "SpaceBetween spreads the children to both ends");
		Check(RectIs("s_cc", 280, 215, 300, 225), "Justify End and Align Center place a child in the container");
		Check(RectIs("s_st", 400, 200, 500, 210), "Align Stretch widens a fit child across the direction");
	}

	void DingoUITest::DeclareGrow()
	{
		DingoUI::BeginRoot("g_row", UIRootParams().SetOffset({ 0.0f, 100.0f }).SetLayout(UIBoxParams()
			.SetDirection(UIDirection::Row).SetSize(UISize::Fixed(400.0f), UISize::Fixed(50.0f))));
		DingoUI::BeginBox(UIBoxParams().SetId("g_0").SetSize(UISize::Fixed(100.0f), UISize::Grow()));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("g_1").SetSize(UISize::Grow(1.0f), UISize::Grow()));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("g_2").SetSize(UISize::Grow(3.0f), UISize::Grow()));
		DingoUI::EndBox();
		DingoUI::EndRoot();

		DingoUI::BeginRoot("g_cap", UIRootParams().SetOffset({ 0.0f, 200.0f + 100.0f }).SetLayout(UIBoxParams()
			.SetDirection(UIDirection::Row).SetSize(UISize::Fixed(600.0f), UISize::Fixed(100.0f))));
		DingoUI::BeginBox(UIBoxParams().SetId("g_pc").SetSize(UISize::Percent(0.5f), UISize::Percent(0.25f)));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("g_mn").SetSize(UISize::Fixed(10.0f).SetMin(50.0f), UISize::Fixed(10.0f)));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("g_mx").SetSize(UISize::Grow().SetMax(80.0f), UISize::Fixed(10.0f)));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("g_gr").SetSize(UISize::Grow(), UISize::Fixed(10.0f)));
		DingoUI::EndBox();
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyGrow()
	{
		Check(RectIs("g_0", 0, 100, 100, 150), "A fixed child keeps its size next to grow children");
		Check(RectIs("g_1", 100, 100, 175, 150) && RectIs("g_2", 175, 100, 400, 150), "Grow children split the rest by weight (1:3 of 300)");
		Check(RectIs("g_pc", 0, 300, 300, 325), "Percent is a fraction of the parent's content area on each axis");
		Check(RectIs("g_mn", 300, 300, 350, 310), "MinSize lifts a fixed size");
		Check(RectIs("g_mx", 350, 300, 430, 310), "MaxSize caps a grow size");
		Check(RectIs("g_gr", 430, 300, 600, 310), "What a capped grow child leaves goes to the others");
	}

	void DingoUITest::DeclareText()
	{
		const std::string_view paragraph = "The quick brown fox jumps over the lazy dog and keeps running";
		DingoUI::BeginRoot("t_root", UIRootParams().SetLayout(UIBoxParams().SetGap(10.0f)));
		DingoUI::BeginBox(UIBoxParams().SetId("t_fit").SetPadding(10.0f));
		DingoUI::Text("Play", UITextParams().SetSize(20.0f).SetWrap(false));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("t_wrap").SetWidth(UISize::Fixed(120.0f)));
		DingoUI::Text(paragraph, UITextParams().SetSize(20.0f));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("t_max").SetWidth(UISize::Fixed(400.0f)));
		DingoUI::Text(paragraph, UITextParams().SetSize(20.0f).SetMaxWidth(100.0f));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("t_nowrap").SetWidth(UISize::Fixed(120.0f)));
		DingoUI::Text(paragraph, UITextParams().SetSize(20.0f).SetWrap(false));
		DingoUI::EndBox();
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyText()
	{
		const std::string paragraph = "The quick brown fox jumps over the lazy dog and keeps running";
		const TextMetrics play = m_Font->MeasureText("Play", 20.0f);
		const TextMetrics wrapped = m_Font->MeasureText(paragraph, 20.0f, 120.0f);
		const TextMetrics capped = m_Font->MeasureText(paragraph, 20.0f, 100.0f);
		const TextMetrics oneLine = m_Font->MeasureText(paragraph, 20.0f);
		Check(SizeIs("t_fit", play.Width + 20.0f, play.Height + 20.0f, 1.0f), "A fit box wraps around its text and padding");
		Check(wrapped.LineCount > 1 && SizeIs("t_wrap", 120.0f, wrapped.Height, 1.0f), "Text wraps at the parent's width and makes a taller box");
		Check(SizeIs("t_wrap", 120.0f, wrapped.Height, 1.0f) && wrapped.Height > play.Height * 1.5f, "A wrapped box is taller than a one-line box");
		Check(capped.LineCount > 1 && SizeIs("t_max", 400.0f, capped.Height, 1.0f), "MaxWidth wraps text narrower than its parent");
		Check(SizeIs("t_nowrap", 120.0f, oneLine.Height, 1.0f), "Wrap off keeps the text on one line");
	}

	void DingoUITest::DeclareSizing()
	{
		DingoUI::BeginRoot("z_root", UIRootParams().SetOffset({ 0.0f, 400.0f }).SetLayout(UIBoxParams()
			.SetDirection(UIDirection::Row).SetPadding(10.0f).SetGap(4.0f)));
		DingoUI::BeginBox(UIBoxParams().SetId("z_img").SetSize(UISize::Fixed(30.0f), UISize::Fixed(30.0f)));
		DingoUI::Image(m_ImageTexture, UIImageParams().SetSize(UISize::Fixed(30.0f), UISize::Fixed(30.0f)));
		DingoUI::EndBox();
		DingoUI::Image(m_ImageTexture, UIImageParams().SetSize(UISize::Fixed(60.0f), UISize::Fixed(20.0f)).SetPreserveAspect(false));
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifySizing()
	{
		Check(RectIs("z_root", 0, 400, 114, 450), "Images take their requested size in a fit container");
		Check(RectIs("z_img", 10, 410, 40, 440), "A box around an image keeps its fixed size");
	}

	void DingoUITest::DeclareScale()
	{
		DingoUI::BeginRoot("c_half", UIRootParams().SetScale(UIScale::ScaleWithScreen({ 1920.0f, 1080.0f }, 0.5f)).SetLayout(UIBoxParams()
			.SetPadding(10.0f).SetSize(UISize::Fixed(200.0f), UISize::Fixed(100.0f))));
		DingoUI::BeginBox(UIBoxParams().SetId("c_child").SetSize(UISize::Fixed(40.0f), UISize::Fixed(20.0f)));
		DingoUI::EndBox();
		DingoUI::EndRoot();

		DingoUI::BeginRoot("c_const", UIRootParams().SetOffset({ 100.0f, 0.0f }).SetScale(UIScale::ConstantPixel(2.0f)).SetLayout(UIBoxParams()
			.SetSize(UISize::Fixed(50.0f), UISize::Fixed(50.0f))));
		DingoUI::EndRoot();

		DingoUI::BeginRoot("c_h", UIRootParams().SetOffset({ 0.0f, 200.0f }).SetScale(UIScale::ScaleWithScreen({ 1920.0f, 540.0f }, 1.0f)).SetLayout(UIBoxParams()
			.SetSize(UISize::Fixed(100.0f), UISize::Fixed(10.0f))));
		DingoUI::EndRoot();

		DingoUI::BeginRoot("c_w", UIRootParams().SetOffset({ 0.0f, 300.0f }).SetScale(UIScale::ScaleWithScreen({ 1920.0f, 540.0f }, 0.0f)).SetLayout(UIBoxParams()
			.SetSize(UISize::Fixed(100.0f), UISize::Fixed(10.0f))));
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyScale()
	{
		Check(Near(Internal::ComputeUIScale(UIScale::ScaleWithScreen({ 1920.0f, 1080.0f }, 0.5f), { k_LayoutW, k_LayoutH }), 0.5f), "A 1920x1080 reference on 960x540 scales by 0.5");
		Check(RectIs("c_half", 0, 0, 100, 50), "ScaleWithScreen scales a root's fixed size");
		Check(RectIs("c_child", 5, 5, 25, 15), "ScaleWithScreen scales padding and children");
		Check(RectIs("c_const", 200, 0, 300, 100), "ConstantPixel scales size and Offset by its factor");
		Check(RectIs("c_h", 0, 200, 100, 210), "Match 1 follows the height ratio");
		Check(RectIs("c_w", 0, 150, 50, 155), "Match 0 follows the width ratio");
	}

	void DingoUITest::DeclarePixels()
	{
		Renderer::Clear(m_LayoutTarget, k_Background);
		DingoUI::BeginRoot("p_root", UIRootParams().SetOffset({ 10.0f, 10.0f }).SetLayout(UIBoxParams().SetDirection(UIDirection::Row).SetGap(10.0f)));
		DingoUI::BeginBox(UIBoxParams().SetId("p_red").SetSize(UISize::Fixed(100.0f), UISize::Fixed(50.0f)).SetBackground(k_Red));
		DingoUI::EndBox();
		DingoUI::Image(m_ImageTexture, UIImageParams().SetSize(UISize::Fixed(40.0f), UISize::Fixed(40.0f)));
		DingoUI::Text("Ag", UITextParams().SetSize(40.0f).SetWrap(false));
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyPixels()
	{
		const std::weak_ptr<int> alive = m_Alive;
		m_LayoutTarget->GetAttachment(0)->ReadPixels([this, alive](const TexturePixels& pixels)
		{
			if (alive.expired())
				return;
			Check(pixels.Width == k_LayoutWidth && pixels.Height == k_LayoutHeight, "The element check target reads back");
			Check(NearColor(pixels.GetPixel(60, 35), k_Red), "A box's background draws at its laid-out rect");
			Check(NearColor(pixels.GetPixel(115, 35), k_Background), "The gap between children is untouched");
			Check(NearColor(pixels.GetPixel(140, 30), k_Yellow), "An Image element draws its texture");
			Check(NearColor(pixels.GetPixel(300, 300), k_Background), "Outside the elements stays the background");
			bool text = false;
			for (uint32_t y = 10; y < 60 && !text; ++y)
				for (uint32_t x = 165; x < 260 && !text; ++x)
					text = pixels.GetPixel(x, y).r > 0.5f && pixels.GetPixel(x, y).g > 0.5f;
			Check(text, "A Text element draws glyphs");
		});
	}

	void DingoUITest::DeclareMenu()
	{
		DingoUI::BeginRoot("m_root", UIRootParams().SetAnchor(UIAnchor::Center).SetLayout(UIBoxParams()
			.SetWidth(UISize::Fixed(300.0f)).SetGap(8.0f).SetPadding(16.0f).SetAlign(UIAlign::Stretch)
			.SetBackground({ 0.1f, 0.1f, 0.15f, 0.9f }).SetBorder(2.0f, { 0.6f, 0.7f, 1.0f, 1.0f }).SetCornerRadius(8.0f)));
		DingoUI::Text("Main menu", UITextParams().SetSize(28.0f).SetAlign(UITextAlign::Center));
		const char* labels[] = { "Continue", "New game", "Load", "Options", "Credits", "Quit" };
		for (const char* label : labels)
		{
			DingoUI::BeginBox(UIBoxParams().SetPadding(8.0f).SetBackground({ 0.25f, 0.3f, 0.45f, 1.0f }).SetCornerRadius(4.0f).SetAlign(UIAlign::Center));
			DingoUI::Text(label, UITextParams().SetSize(18.0f).SetAlign(UITextAlign::Center));
			DingoUI::EndBox();
		}
		DingoUI::Image(m_ImageTexture, UIImageParams().SetSize(UISize::Fixed(16.0f), UISize::Fixed(16.0f)));
		DingoUI::Text("Version 1.0", UITextParams().SetSize(12.0f).SetColor({ 0.7f, 0.7f, 0.7f, 1.0f }));
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyMenu()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		const Internal::UIRenderStatistics& statistics = context->GetRenderStatistics();
		Check(context->GetLastLayout().GetElements().size() >= 10, "The menu has about ten or more elements");
		Check(statistics.DrawCalls == 1, "A ten-element menu with text and an image is one draw call");
		Check(statistics.Quads >= 10, "Every element's drawing reaches the batcher");
	}

	void DingoUITest::DeclareUnmatched()
	{
		DingoUI::EndBox();
		DingoUI::BeginRoot("u_root", UIRootParams().SetLayout(UIBoxParams().SetPadding(5.0f)));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetId("u_box").SetSize(UISize::Fixed(30.0f), UISize::Fixed(10.0f)));
		DingoUI::BeginBox(UIBoxParams().SetId("u_inner").SetSize(UISize::Fixed(10.0f), UISize::Fixed(5.0f)));
	}

	void DingoUITest::VerifyUnmatched()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		const Internal::UILayoutTree& tree = context->GetLastLayout();
		const Internal::UIElement* box = tree.FindByName("u_box");
		const Internal::UIElement* inner = tree.FindByName("u_inner");
		Check(box && inner && inner->Parent >= 0 && &tree.GetElements()[inner->Parent] == box, "Unclosed Begin calls are closed at frame end with the tree intact");
		Check(RectIs("u_root", 0, 0, 40, 20), "An unclosed root still lays out");
		Check(tree.GetOpenCount() == 0, "A mismatched frame leaves nothing open for the next one");
	}

	void DingoUITest::DeclareNoFont()
	{
		DingoUI::SetDefaultFont(nullptr);
		DingoUI::BeginRoot("f_root", UIRootParams().SetLayout(UIBoxParams().SetPadding(5.0f)));
		DingoUI::BeginBox(UIBoxParams().SetId("f_box").SetPadding(5.0f));
		DingoUI::Text("No font");
		DingoUI::EndBox();
		DingoUI::EndRoot();
		DingoUI::SetDefaultFont(m_Font);
	}

	void DingoUITest::VerifyNoFont()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		const Internal::UIElement* box = context->GetLastLayout().FindByName("f_box");
		Check(box && box->ChildCount == 0 && RectIs("f_box", 5, 5, 15, 15), "Text with no font is skipped");
		Check(context->GetRenderStatistics().DrawCalls == 0, "A frame with nothing to draw makes no draw call");
	}

	void DingoUITest::DeclarePointerBlocking()
	{
		m_PointerState = {};
		m_PointerState.ViewportSize = { 1920.0f, 1080.0f };
		m_PointerState.PointerPosition = { 400.0f, 300.0f };
		m_PointerState.PointerAvailable = true;
		Internal::GetDingoUIContext()->SetInputOverride(&m_PointerState);
		DeclarePointerRoots();
	}

	void DingoUITest::DeclarePointerRoots()
	{
		DingoUI::BeginRoot("n_block", UIRootParams().SetOffset({ 150.0f, 100.0f }).SetLayout(UIBoxParams()
			.SetSize(UISize::Fixed(100.0f), UISize::Fixed(100.0f)).SetBackground({ 0.3f, 0.3f, 0.3f, 1.0f })));
		DingoUI::EndRoot();
		DingoUI::BeginRoot("n_pass", UIRootParams().SetOffset({ 500.0f, 100.0f }).SetBlocksInput(false).SetLayout(UIBoxParams()
			.SetSize(UISize::Fixed(100.0f), UISize::Fixed(100.0f)).SetBackground({ 0.3f, 0.5f, 0.3f, 1.0f })));
		DingoUI::EndRoot();
	}

	void DingoUITest::VerifyPointerBlocking()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		const glm::vec2 mapped = context->WindowToTarget({ 400.0f, 300.0f });
		Check(Near(mapped.x, 200.0f, 0.01f) && Near(mapped.y, 150.0f, 0.01f), "A window point maps to target pixels by the size ratio");
		Check(Near(context->GetInput().PointerPosition.x, 200.0f, 0.01f) && Near(context->GetInput().PointerPosition.y, 150.0f, 0.01f), "The pointer is handed to the UI in target pixels");
		Check(DingoUI::IsPointerOverUI(), "A pointer over a blocking root is over the UI");
	}

	void DingoUITest::DeclarePointerPassive()
	{
		m_PointerState.PointerPosition = { 1100.0f, 300.0f };
		DeclarePointerRoots();
	}

	void DingoUITest::VerifyPointerPassive()
	{
		Check(!DingoUI::IsPointerOverUI(), "A pointer over a root that doesn't block input is not over the UI");
	}

	void DingoUITest::DeclareDemo()
	{
		DingoUI::BeginRoot("demo", UIRootParams().SetOffset({ 16.0f, 16.0f }).SetScale(UIScale::ScaleWithScreen()).SetLayout(UIBoxParams()
			.SetGap(8.0f).SetPadding(12.0f).SetBackground({ 0.08f, 0.1f, 0.16f, 0.9f }).SetBorder(2.0f, { 0.5f, 0.65f, 1.0f, 1.0f }).SetCornerRadius(8.0f)));
		DingoUI::Text("DingoUI element API", UITextParams().SetSize(28.0f));
		DingoUI::BeginBox(UIBoxParams().SetWidth(UISize::Fixed(360.0f)));
		DingoUI::Text("Roots anchor to the screen, boxes stack their children and text wraps at the width its parent gives it.", UITextParams().SetSize(20.0f).SetColor({ 0.8f, 0.85f, 0.95f, 1.0f }));
		DingoUI::EndBox();
		DingoUI::BeginBox(UIBoxParams().SetDirection(UIDirection::Row).SetGap(8.0f).SetAlign(UIAlign::Center));
		DingoUI::Image(m_ImageTexture, UIImageParams().SetSize(UISize::Fixed(24.0f), UISize::Fixed(24.0f)));
		DingoUI::Text("Image + text", UITextParams().SetSize(20.0f).SetWrap(false));
		DingoUI::EndBox();
		DingoUI::EndRoot();
	}

	bool DingoUITest::StepLayout(Framebuffer* output)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		if (!context || !m_LayoutTarget || !m_Font || !m_Font->IsValid())
			return false;

		struct Case
		{
			void (DingoUITest::*Declare)();
			void (DingoUITest::*Verify)();
		};
		static constexpr Case cases[] = {
			{ &DingoUITest::DeclareAnchors, &DingoUITest::VerifyAnchors },
			{ &DingoUITest::DeclareStacks, &DingoUITest::VerifyStacks },
			{ &DingoUITest::DeclareGrow, &DingoUITest::VerifyGrow },
			{ &DingoUITest::DeclareText, &DingoUITest::VerifyText },
			{ &DingoUITest::DeclareSizing, &DingoUITest::VerifySizing },
			{ &DingoUITest::DeclareScale, &DingoUITest::VerifyScale },
			{ &DingoUITest::DeclarePixels, &DingoUITest::VerifyPixels },
			{ &DingoUITest::DeclareMenu, &DingoUITest::VerifyMenu },
			{ &DingoUITest::DeclareUnmatched, &DingoUITest::VerifyUnmatched },
			{ &DingoUITest::DeclareNoFont, &DingoUITest::VerifyNoFont },
			{ &DingoUITest::DeclarePointerBlocking, &DingoUITest::VerifyPointerBlocking },
			{ &DingoUITest::DeclarePointerPassive, &DingoUITest::VerifyPointerPassive } };
		constexpr uint32_t count = static_cast<uint32_t>(sizeof(cases) / sizeof(cases[0]));

		const uint32_t stage = m_LayoutStage;
		if (stage > count * 2 + 1)
			return false;
		++m_LayoutStage;

		if (stage == 0)
			context->SetTarget(m_LayoutTarget);
		else if (stage <= count * 2)
		{
			const Case& current = cases[(stage - 1) / 2];
			if ((stage - 1) % 2 == 0)
				(this->*current.Declare)();
			else
				(this->*current.Verify)();
		}
		else
		{
			context->SetInputOverride(nullptr);
			context->SetTarget(output);
		}
		return true;
	}

	void DingoUITest::Update(float deltaTime)
	{
		StepConsumeMask();
		Framebuffer* output = Renderer::GetRenderTarget();
		if (!StepBatcher() && !StepLayout(output))
			DeclareDemo();

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
