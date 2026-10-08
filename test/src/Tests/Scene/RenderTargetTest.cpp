#include "RenderTargetTest.h"

#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

#include <climits>
#include <cmath>
#include <cstdlib>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_ProbeSize = 256;
		// The probe camera sees 10 world units top to bottom, so a unit is 25.6 px of the probe.
		constexpr float k_ProbeViewHeight = 10.0f;

		struct PixelBox
		{
			int MinX = INT_MAX;
			int MinY = INT_MAX;
			int MaxX = -1;
			int MaxY = -1;
			int Count = 0;

			int Width() const { return Count ? MaxX - MinX + 1 : 0; }
			int Height() const { return Count ? MaxY - MinY + 1 : 0; }
		};

		template<typename Match>
		PixelBox FindPixels(const TexturePixels& pixels, Match match)
		{
			PixelBox box;
			for (uint32_t y = 0; y < pixels.Height; ++y)
			{
				for (uint32_t x = 0; x < pixels.Width; ++x)
				{
					const uint8_t* rgba = &pixels.Data[(static_cast<size_t>(y) * pixels.Width + x) * 4];
					if (!match(rgba[0], rgba[1], rgba[2]))
						continue;

					box.MinX = (std::min)(box.MinX, static_cast<int>(x));
					box.MinY = (std::min)(box.MinY, static_cast<int>(y));
					box.MaxX = (std::max)(box.MaxX, static_cast<int>(x));
					box.MaxY = (std::max)(box.MaxY, static_cast<int>(y));
					box.Count++;
				}
			}
			return box;
		}

		std::vector<uint8_t> ReadImageFile(const std::filesystem::path& path, bool flipVertically)
		{
			uint32_t width = 0, height = 0, channels = 0;
			std::vector<uint8_t> bytes;
			if (const uint8_t* data = FileSystem::ReadImage(path, &width, &height, &channels, flipVertically, true))
			{
				bytes.assign(data, data + static_cast<size_t>(width) * height * 4);
				FileSystem::FreeImage(data);
			}
			return bytes;
		}

		Entity AddSprite(Scene& scene, const char* name, const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
		{
			Entity sprite = scene.CreateEntity(name);
			sprite.GetComponent<TransformComponent>() = TransformComponent(position, size);
			sprite.AddComponent<SpriteRendererComponent>(SpriteRendererComponent(color));
			return sprite;
		}

		Entity AddText(Scene& scene, const char* name, const std::string& text, Font* font, const glm::vec3& position, float size, const glm::vec4& color, float rotation = 0.0f)
		{
			Entity entity = scene.CreateEntity(name);
			TransformComponent& transform = entity.GetComponent<TransformComponent>();
			transform.Position = position;
			transform.Rotation = rotation;
			TextComponent& component = entity.AddComponent<TextComponent>();
			component.Text = text;
			component.Font = font;
			component.Size = size;
			component.Color = color;
			return entity;
		}
	}

	void RenderTargetTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void RenderTargetTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Checks.clear();
		m_Time = 0.0f;
		m_ProbeRequested = false;
		m_ProbePixels.clear();
		m_Alive = std::make_shared<int>(0);

		m_TempDirectory = std::filesystem::temp_directory_path() / "DingoRenderTargetTest";
		std::error_code error;
		std::filesystem::create_directories(m_TempDirectory, error);

		m_Font = Font::Create("assets/fonts/arialbd.ttf");

		auto makeTarget = [](const char* name, int32_t width, int32_t height)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(width)
				.SetHeight(height)
				.SetEnableDepth(true)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		};
		m_ProbeTarget = makeTarget("RenderTargetTest probe", k_ProbeSize, k_ProbeSize);
		m_TargetA = makeTarget("RenderTargetTest A", 640, 360);
		m_TargetB = makeTarget("RenderTargetTest B", 640, 360);

		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_SceneA = BuildShowScene("Render Target Test: A", { 0.32f, 0.20f, 0.14f, 1.0f }, renderer3D.GetBoxMesh(), { 0.95f, 0.55f, 0.20f, 1.0f }, m_SpinnerA);
		m_SceneB = BuildShowScene("Render Target Test: B", { 0.10f, 0.18f, 0.30f, 1.0f }, renderer3D.GetSphereMesh(), { 0.35f, 0.70f, 1.00f, 1.0f }, m_SpinnerB);
		BuildProbeScene();

		RunImageFileChecks();
	}

	Scene* RenderTargetTest::BuildShowScene(const char* name, const glm::vec4& clearColor, Mesh* mesh, const glm::vec4& color, Entity& spinner)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		Scene* scene = new Scene(name);
		scene->SetClearColor(clearColor);

		Entity camera = scene->CreateEntity("Camera");
		Transform3DComponent& cameraTransform = camera.AddComponent<Transform3DComponent>();
		cameraTransform.Position = { 0.0f, 1.2f, 3.6f };
		cameraTransform.Rotation = glm::quat(glm::radians(glm::vec3(-15.0f, 0.0f, 0.0f)));
		camera.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;

		DirectionalLightComponent& sun = scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
		sun.Intensity = 0.8f;
		sun.Ambient = 0.25f;

		Entity floor = scene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.65f, 0.0f }, { 6.0f, 0.1f, 6.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer3D.GetBoxMesh(), { 0.45f, 0.48f, 0.44f, 1.0f }));

		spinner = scene->CreateEntity("Spinner");
		spinner.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 0.05f, 0.0f }, glm::vec3(1.1f)));
		spinner.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color));
		return scene;
	}

	void RenderTargetTest::BuildProbeScene()
	{
		m_Probe = new Scene("Render Target Test: probe");
		m_Probe->SetClearColor({ 0.10f, 0.12f, 0.30f, 1.0f });

		Entity camera = m_Probe->CreateEntity("Camera");
		camera.AddComponent<CameraComponent>().OrthographicSize = k_ProbeViewHeight;

		// A square, read back to measure the aspect, and a marker in the top-left corner.
		AddSprite(*m_Probe, "Square", { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		AddSprite(*m_Probe, "Marker", { -4.0f, 4.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		if (!m_Font)
			return;

		// Yellow text under a cover with a higher z, magenta text over a panel with a lower one.
		AddText(*m_Probe, "Hidden", "HIDE", m_Font, { 1.2f, -3.3f, 0.0f }, 0.7f, { 1.0f, 1.0f, 0.0f, 1.0f });
		AddSprite(*m_Probe, "Cover", { 2.5f, -3.0f, 0.5f }, { 3.5f, 1.6f }, { 0.5f, 0.5f, 0.5f, 1.0f });
		AddSprite(*m_Probe, "Panel", { 3.0f, 3.2f, 0.8f }, { 3.2f, 1.4f }, { 0.2f, 0.2f, 0.2f, 1.0f });
		AddText(*m_Probe, "Shown", "TOP", m_Font, { 2.0f, 3.0f, 0.9f }, 0.7f, { 1.0f, 0.0f, 1.0f, 1.0f });

		// Cyan text turned to run up the screen.
		AddText(*m_Probe, "Turned", "IIIIIIIIIIII", m_Font, { -3.5f, -4.6f, 0.2f }, 0.7f, { 0.0f, 1.0f, 1.0f, 1.0f }, 90.0f);
	}

	void RenderTargetTest::RunImageFileChecks()
	{
		std::vector<uint8_t> pattern(3 * 2 * 4);
		for (size_t i = 0; i < pattern.size(); ++i)
			pattern[i] = static_cast<uint8_t>(i * 9 + 7);

		const std::filesystem::path straight = m_TempDirectory / "pattern.png";
		const std::filesystem::path flipped = m_TempDirectory / "pattern-flipped.png";
		const bool written = FileSystem::WriteImage(straight, 3, 2, 4, pattern.data()) && FileSystem::WriteImage(flipped, 3, 2, 4, pattern.data(), true);
		Check(written && ReadImageFile(straight, false) == pattern && ReadImageFile(flipped, true) == pattern,
			"FileSystem::WriteImage writes a PNG ReadImage reads back byte for byte, and its flip undoes ReadImage's");

		m_Container = Texture::CreateFromFile("assets/textures/container.jpg", "Container");
		if (!m_Container)
		{
			Check(false, "container.jpg loads for the SaveToFile check");
			return;
		}

		const std::filesystem::path saved = m_TempDirectory / "container.png";
		const std::weak_ptr<int> alive = m_Alive;
		m_Container->SaveToFile(saved, [this, alive, saved](bool ok)
		{
			if (alive.expired())
				return;

			Check(ok && ReadImageFile(saved, false) == ReadImageFile("assets/textures/container.jpg", false),
				"Texture::SaveToFile writes an image loaded from a file the way the file held it");
		});
	}

	void RenderTargetTest::CheckProbe(const TexturePixels& pixels)
	{
		m_ProbePixels = pixels.Data;
		Check(pixels.Width == k_ProbeSize && pixels.Height == k_ProbeSize && pixels.Data.size() == size_t(k_ProbeSize) * k_ProbeSize * 4,
			std::format("Texture::ReadPixels reads the probe framebuffer back ({}x{})", pixels.Width, pixels.Height));
		if (pixels.Data.empty())
			return;

		const uint8_t* edge = &pixels.Data[(static_cast<size_t>(k_ProbeSize / 2) * k_ProbeSize + (k_ProbeSize - 6)) * 4];
		Check(std::abs(edge[0] - 26) <= 2 && std::abs(edge[1] - 31) <= 2 && std::abs(edge[2] - 77) <= 2,
			std::format("SceneRenderer::Render(scene, target) clears the target to the scene's colour ({}, {}, {})", edge[0], edge[1], edge[2]));

		const PixelBox square = FindPixels(pixels, [](int r, int g, int b) { return r > 200 && g < 60 && b < 60; });
		Check(square.Count > 0 && std::abs(square.Width() - square.Height()) <= 2,
			std::format("a square sprite comes out square: the projection takes the target's aspect, not the window's ({}x{} px)", square.Width(), square.Height()));

		const PixelBox marker = FindPixels(pixels, [](int r, int g, int b) { return g > 200 && r < 60 && b < 60; });
		Check(marker.Count > 0 && marker.MaxY < int(k_ProbeSize / 4) && marker.MaxX < int(k_ProbeSize / 2),
			std::format("row 0 of a render target read back is its top: the top-left marker is in rows {}-{}", marker.MinY, marker.MaxY));

		const PixelBox hidden = FindPixels(pixels, [](int r, int g, int b) { return r > 170 && g > 170 && b < 100; });
		Check(hidden.Count == 0, std::format("text below a sprite with a higher z is hidden ({} px of it show)", hidden.Count));

		const PixelBox shown = FindPixels(pixels, [](int r, int g, int b) { return r > 170 && g < 90 && b > 170; });
		Check(shown.Count > 0, "text with a higher z than a sprite draws over it");

		const PixelBox turned = FindPixels(pixels, [](int r, int g, int b) { return r < 90 && g > 170 && b > 170; });
		Check(turned.Count > 0 && turned.Height() > 2 * turned.Width(),
			std::format("text turned 90 degrees runs up the screen ({}x{} px)", turned.Width(), turned.Height()));
	}

	void RenderTargetTest::Update(float deltaTime)
	{
		m_Time += deltaTime;
		m_SpinnerA.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(m_Time, glm::normalize(glm::vec3(0.3f, 1.0f, 0.0f)));
		m_SpinnerB.GetComponent<Transform3DComponent>().Rotation = glm::angleAxis(-0.7f * m_Time, glm::normalize(glm::vec3(1.0f, 0.4f, 0.0f)));

		SceneRenderer& sceneRenderer = Application::Get().GetSceneRenderer();
		sceneRenderer.Render(*m_SceneA, m_TargetA);
		sceneRenderer.Render(*m_SceneB, m_TargetB);
		sceneRenderer.Render(*m_Probe, m_ProbeTarget);

		if (!m_ProbeRequested && !Renderer::IsFrameSkipped())
		{
			m_ProbeRequested = true;
			Texture* probe = m_ProbeTarget->GetAttachment(0);
			const std::weak_ptr<int> alive = m_Alive;
			probe->ReadPixels([this, alive](const TexturePixels& pixels)
			{
				if (!alive.expired())
					CheckProbe(pixels);
			});

			const std::filesystem::path png = m_TempDirectory / "probe.png";
			probe->SaveToFile(png, [this, alive, png](bool saved)
			{
				if (alive.expired())
					return;
				Check(saved && !m_ProbePixels.empty() && ReadImageFile(png, false) == m_ProbePixels,
					"Texture::SaveToFile writes a render target as it reads back, its first row at the top of the PNG");
			});
		}

		// A render target's first row is its top, so the quads take a negative height to show it upright.
		const float fade = 0.5f + 0.5f * std::sin(0.8f * m_Time);
		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		m_Renderer->DrawQuad({ 0.0f, 0.75f }, { 5.0f, -2.8125f }, m_TargetA->GetAttachment(0));
		m_Renderer->DrawQuad({ 0.0f, 0.75f }, { 5.0f, -2.8125f }, m_TargetB->GetAttachment(0), { 1.0f, 1.0f, 1.0f, fade });
		m_Renderer->DrawQuad({ -1.5f, -1.6f }, { 1.7f, -1.7f }, m_ProbeTarget->GetAttachment(0));
		if (m_Font)
		{
			m_Renderer->DrawText("Two scenes rendered into textures, crossfading", m_Font, glm::vec2(-2.5f, 2.25f), 0.16f);
			m_Renderer->DrawText("The probe, read back for the checks", m_Font, glm::vec2(-0.5f, -1.6f), 0.16f);
		}
		m_Renderer->EndScene();
	}

	void RenderTargetTest::Cleanup()
	{
		m_Alive.reset();

		delete m_Probe;
		m_Probe = nullptr;
		delete m_SceneA;
		m_SceneA = nullptr;
		delete m_SceneB;
		m_SceneB = nullptr;

		DestroyAndDelete(m_ProbeTarget);
		DestroyAndDelete(m_TargetA);
		DestroyAndDelete(m_TargetB);
		DestroyAndDelete(m_Container);
		DestroyAndDelete(m_Font);

		std::error_code error;
		std::filesystem::remove_all(m_TempDirectory, error);
		m_Checks.clear();

		Renderer2DTest::Cleanup();
	}

	void RenderTargetTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
