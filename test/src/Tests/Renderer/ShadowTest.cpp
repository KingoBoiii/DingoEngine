#include "ShadowTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_FoxPath = "assets/models/Fox/Fox.gltf";
		constexpr float k_FoxLength = 1.6f;

		const glm::vec3 k_SunDirection = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
		// 10 degrees above the horizon: 80 degrees from the plane's normal.
		const glm::vec3 k_GrazingDirection = glm::normalize(glm::vec3(-1.0f, -std::tan(glm::radians(10.0f)), 0.0f));
		const glm::vec3 k_FoxSunDirection = glm::normalize(glm::vec3(-1.0f, -0.8f, 0.0f));

		const glm::vec4 k_FloorColor{ 0.6f, 0.6f, 0.6f, 1.0f };

		// The sun scene: a 1 x 2 x 1 box at the origin, a pillar nearby and one 35 m away.
		const glm::vec3 k_BoxSize{ 1.0f, 2.0f, 1.0f };
		const glm::vec3 k_NearPillar{ 10.0f, 2.0f, 4.0f };
		const glm::vec3 k_FarPillar{ 20.0f, 2.0f, 24.0f };
		const glm::vec3 k_PillarSize{ 1.0f, 4.0f, 1.0f };

		// Where on the floor a point of a caster at `height` throws its shadow.
		glm::vec3 ShadowOnFloor(const glm::vec3& base, float height, const glm::vec3& light)
		{
			return base + glm::vec3(light.x, 0.0f, light.z) * (height / -light.y);
		}

		glm::mat4 Box(const glm::vec3& center, const glm::vec3& size)
		{
			return glm::scale(glm::translate(glm::mat4(1.0f), center), size);
		}

		Framebuffer* MakeTarget(const char* name)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(static_cast<int32_t>(320))
				.SetHeight(static_cast<int32_t>(240))
				.SetEnableDepth(true)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		}

		glm::ivec2 ToPixel(const glm::mat4& viewProjection, const glm::vec3& point, uint32_t width, uint32_t height)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			return { static_cast<int>((ndc.x * 0.5f + 0.5f) * width), static_cast<int>((0.5f - ndc.y * 0.5f) * height) };
		}

		glm::vec4 PixelAt(const TexturePixels& pixels, const glm::ivec2& pixel)
		{
			return pixels.GetPixel(static_cast<uint32_t>(std::clamp(pixel.x, 0, static_cast<int>(pixels.Width) - 1)),
				static_cast<uint32_t>(std::clamp(pixel.y, 0, static_cast<int>(pixels.Height) - 1)));
		}

		glm::vec4 PixelAt(const std::vector<uint8_t>& data, uint32_t width, const glm::ivec2& pixel)
		{
			TexturePixels pixels;
			pixels.Width = width;
			pixels.Height = static_cast<uint32_t>(data.size() / (static_cast<size_t>(width) * 4));
			pixels.Data = data;
			return PixelAt(pixels, pixel);
		}
	}

	void ShadowTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_Alive = std::make_shared<int>(0);
		m_Time = 0.0f;

		const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
		if (auto mode = args.Get("shadow"))
		{
			if (*mode == "acne")
				m_Mode = Mode::Acne;
			else if (*mode == "skinned")
				m_Mode = Mode::Skinned;
			else
				m_Mode = Mode::Sun;
		}
		m_Pan = args.Get("shadow-pan").has_value();

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_DefaultSettings = renderer.GetShadowSettings();
		Renderer3DShadowSettings settings = m_DefaultSettings;
		settings.DebugCascades = args.Get("shadow-cascades").has_value();
		renderer.SetShadowSettings(settings);

		m_Camera = CameraFor(m_Mode, m_AspectRatio);

		LoadFox();
		BuildEntityScene();

		m_SunPair = { MakeTarget("ShadowTest sun on"), MakeTarget("ShadowTest sun off") };
		m_AcnePair = { MakeTarget("ShadowTest acne on"), MakeTarget("ShadowTest acne off") };
		m_FoxPair = { MakeTarget("ShadowTest fox on"), MakeTarget("ShadowTest fox off") };
		m_EntityTarget = MakeTarget("ShadowTest entities");
	}

	void ShadowTest::LoadFox()
	{
		m_Fox = Model::LoadFromFile(k_FoxPath);
		if (!m_Fox || !m_Fox->IsSkinned())
			return;

		glm::vec3 low((std::numeric_limits<float>::max)());
		glm::vec3 high((std::numeric_limits<float>::lowest)());
		for (const SubMesh& submesh : m_Fox->GetSubMeshes())
		{
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
			{
				low = (glm::min)(low, vertex.Position);
				high = (glm::max)(high, vertex.Position);
			}
		}

		// Standing on the floor at the origin, its length along z, so the low sun from +x throws its
		// shadow sideways where the camera can see it.
		const glm::vec3 extent = high - low;
		const float scale = k_FoxLength / (std::max)({ extent.x, extent.y, extent.z });
		const glm::vec3 center = 0.5f * (low + high);
		const bool alongX = extent.x > extent.z;
		m_FoxTransform = glm::rotate(glm::mat4(1.0f), alongX ? glm::radians(90.0f) : 0.0f, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(scale))
			* glm::translate(glm::mat4(1.0f), -glm::vec3(center.x, low.y, center.z));

		m_FoxMin = glm::vec3((std::numeric_limits<float>::max)());
		m_FoxMax = glm::vec3((std::numeric_limits<float>::lowest)());
		for (int corner = 0; corner < 8; ++corner)
		{
			const glm::vec3 local((corner & 1) ? high.x : low.x, (corner & 2) ? high.y : low.y, (corner & 4) ? high.z : low.z);
			const glm::vec3 world = glm::vec3(m_FoxTransform * glm::vec4(local, 1.0f));
			m_FoxMin = (glm::min)(m_FoxMin, world);
			m_FoxMax = (glm::max)(m_FoxMax, world);
		}

		m_FoxMaterial = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams().SetDebugName("ShadowTestFox"));
		for (const SubMesh& submesh : m_Fox->GetSubMeshes())
		{
			if (submesh.DiffuseTexture)
			{
				m_FoxMaterial->SetTexture(0, submesh.DiffuseTexture);
				break;
			}
		}

		m_FoxAnimator = Animator(m_Fox->GetSkeleton());
		if (const AnimationClip* walk = m_Fox->FindAnimation("Walk"))
			m_FoxAnimator.Play(walk);
		m_FoxPalette.assign(m_Fox->GetSkeleton()->GetRestPalette().begin(), m_Fox->GetSkeleton()->GetRestPalette().end());
	}

	void ShadowTest::BuildEntityScene()
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_EntityScene = new Scene("Shadow Test: entities");
		m_EntityScene->SetClearColor(m_ClearColor);

		const PerspectiveCamera camera = CameraFor(Mode::Sun, 1.0f);
		Entity cameraEntity = m_EntityScene->CreateEntity("Camera");
		Transform3DComponent& cameraTransform = cameraEntity.AddComponent<Transform3DComponent>();
		cameraTransform.Position = camera.GetPosition();
		cameraTransform.Rotation = glm::quatLookAt(glm::normalize(glm::vec3(2.0f, 0.0f, 3.0f) - camera.GetPosition()), glm::vec3(0.0f, 1.0f, 0.0f));
		CameraComponent& cameraComponent = cameraEntity.AddComponent<CameraComponent>();
		cameraComponent.Type = CameraComponent::ProjectionType::Perspective;
		cameraComponent.FOV = 45.0f;
		cameraComponent.PerspNear = 0.1f;
		cameraComponent.PerspFar = 100.0f;

		DirectionalLightComponent& sun = m_EntityScene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
		sun.Direction = k_SunDirection;
		sun.Intensity = 1.0f;
		sun.Ambient = 0.3f;
		sun.CastShadows = true;

		Entity floor = m_EntityScene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.05f, 0.0f }, { 60.0f, 0.1f, 60.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), k_FloorColor));

		Entity box = m_EntityScene->CreateEntity("Hidden box");
		box.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 1.0f, 0.0f }, k_BoxSize));
		MeshRendererComponent& boxRenderer = box.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 1.0f, 0.0f, 0.0f, 1.0f }));
		boxRenderer.Shadows = ShadowCasting::ShadowsOnly;
	}

	PerspectiveCamera ShadowTest::CameraFor(Mode mode, float aspect) const
	{
		PerspectiveCamera camera(45.0f, aspect, 0.1f, 100.0f);
		switch (mode)
		{
			case Mode::Acne:
				camera.SetPosition({ 0.0f, 6.0f, 6.0f });
				camera.SetTarget({ 0.0f, 0.0f, 0.0f });
				break;
			case Mode::Skinned:
				camera.SetPosition({ -3.0f, 4.0f, 0.5f });
				camera.SetTarget({ -0.4f, 0.0f, 0.0f });
				break;
			case Mode::Sun:
			default:
				camera.SetPosition({ -5.0f, 4.0f, -6.0f });
				camera.SetTarget({ 2.0f, 0.0f, 3.0f });
				break;
		}
		return camera;
	}

	void ShadowTest::DrawMode(Renderer3D& renderer, Mode mode, bool shadows, const std::vector<glm::mat4>* palette) const
	{
		DirectionalLight sun;
		sun.Intensity = 1.0f;
		sun.CastShadows = shadows;
		sun.Direction = mode == Mode::Acne ? k_GrazingDirection : mode == Mode::Skinned ? k_FoxSunDirection : k_SunDirection;
		renderer.SubmitLight(sun);
		renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);

		Mesh* box = renderer.GetBoxMesh();
		switch (mode)
		{
			case Mode::Acne:
				renderer.SubmitMesh(box, Box({ 0.0f, -0.025f, 0.0f }, { 8.0f, 0.05f, 8.0f }), k_FloorColor);
				break;

			case Mode::Skinned:
				renderer.SubmitMesh(box, Box({ 0.0f, -0.05f, 0.0f }, { 20.0f, 0.1f, 20.0f }), k_FloorColor);
				if (m_Fox && palette)
				{
					for (const SubMesh& submesh : m_Fox->GetSubMeshes())
					{
						if (submesh.MeshData->HasSkin())
							renderer.SubmitSkinnedMesh(submesh.MeshData, m_FoxTransform, *palette, glm::vec4(1.0f), m_FoxMaterial);
					}
				}
				break;

			case Mode::Sun:
			default:
				renderer.SubmitMesh(box, Box({ 0.0f, -0.05f, 0.0f }, { 60.0f, 0.1f, 60.0f }), k_FloorColor);
				renderer.SubmitMesh(box, Box({ 0.0f, 1.0f, 0.0f }, k_BoxSize), { 0.8f, 0.4f, 0.3f, 1.0f });
				renderer.SubmitMesh(box, Box(k_NearPillar, k_PillarSize), { 0.4f, 0.5f, 0.8f, 1.0f });
				renderer.SubmitMesh(box, Box(k_FarPillar, k_PillarSize), { 0.4f, 0.8f, 0.5f, 1.0f });
				renderer.SubmitMesh(renderer.GetSphereMesh(), Box({ 2.5f, 0.6f, -1.0f }, glm::vec3(1.2f)), { 0.9f, 0.9f, 0.4f, 1.0f });
				break;
		}
	}

	void ShadowTest::DrawInto(Framebuffer* target, Mode mode, bool shadows, const PerspectiveCamera& camera)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		Framebuffer* previous = Renderer::GetRenderTarget();
		if (target)
			Renderer::SetRenderTarget(target);

		renderer.BeginScene(camera);
		renderer.Clear(m_ClearColor);
		DrawMode(renderer, mode, shadows, &m_FoxPalette);
		renderer.EndScene();

		if (target)
			Renderer::SetRenderTarget(previous);
	}

	void ShadowTest::RunChecks()
	{
		const float aspect = static_cast<float>(k_CheckWidth) / static_cast<float>(k_CheckHeight);
		const PerspectiveCamera sunCamera = CameraFor(Mode::Sun, aspect);
		const PerspectiveCamera acneCamera = CameraFor(Mode::Acne, aspect);
		const PerspectiveCamera foxCamera = CameraFor(Mode::Skinned, aspect);

		DrawInto(m_SunPair.On, Mode::Sun, true, sunCamera);
		const Renderer3D::Statistics sunStats = Application::Get().GetRenderer3D().GetStatistics();
		DrawInto(m_SunPair.Off, Mode::Sun, false, sunCamera);
		DrawInto(m_AcnePair.On, Mode::Acne, true, acneCamera);
		DrawInto(m_AcnePair.Off, Mode::Acne, false, acneCamera);
		if (m_Fox)
		{
			DrawInto(m_FoxPair.On, Mode::Skinned, true, foxCamera);
			DrawInto(m_FoxPair.Off, Mode::Skinned, false, foxCamera);
		}
		Application::Get().GetSceneRenderer().Render(*m_EntityScene, m_EntityTarget);

		Check(sunStats.ShadowViews == m_DefaultSettings.CascadeCount && sunStats.ShadowDrawCalls > 0,
			std::format("a casting sun renders {} cascades into the atlas ({} views, {} instanced draws)", m_DefaultSettings.CascadeCount, sunStats.ShadowViews, sunStats.ShadowDrawCalls));

		const std::weak_ptr<int> alive = m_Alive;
		auto readBack = [alive](Framebuffer* target, std::function<void(const TexturePixels&)> check)
		{
			target->GetAttachment(0)->ReadPixels([alive, check = std::move(check)](const TexturePixels& pixels)
			{
				if (!alive.expired())
					check(pixels);
			});
		};

		const glm::mat4 sunViewProjection = sunCamera.GetViewProjectionMatrix();
		const glm::ivec2 behindBox = ToPixel(sunViewProjection, ShadowOnFloor({ 0.0f, 0.0f, 0.0f }, 0.8f * k_BoxSize.y, k_SunDirection), k_CheckWidth, k_CheckHeight);
		const glm::ivec2 behindFarPillar = ToPixel(sunViewProjection, ShadowOnFloor({ k_FarPillar.x, 0.0f, k_FarPillar.z }, 0.8f * k_PillarSize.y, k_SunDirection), k_CheckWidth, k_CheckHeight);
		const glm::ivec2 inSun = ToPixel(sunViewProjection, { 3.0f, 0.0f, -2.0f }, k_CheckWidth, k_CheckHeight);

		readBack(m_SunPair.Off, [this](const TexturePixels& pixels) { m_SunPair.OffPixels = pixels.Data; });
		readBack(m_SunPair.On, [this, behindBox, behindFarPillar, inSun](const TexturePixels& pixels)
		{
			const glm::vec4 shadowOn = PixelAt(pixels, behindBox);
			const glm::vec4 shadowOff = PixelAt(m_SunPair.OffPixels, k_CheckWidth, behindBox);
			Check(shadowOn.g < 0.75f * shadowOff.g, std::format("the floor behind a box goes dark when the sun casts ({:.3f} against {:.3f})", shadowOn.g, shadowOff.g));

			const glm::vec4 farOn = PixelAt(pixels, behindFarPillar);
			const glm::vec4 farOff = PixelAt(m_SunPair.OffPixels, k_CheckWidth, behindFarPillar);
			Check(farOn.g < 0.75f * farOff.g, std::format("so does the floor behind a pillar 35 m off, in a far cascade ({:.3f} against {:.3f})", farOn.g, farOff.g));

			const glm::vec4 sunOn = PixelAt(pixels, inSun);
			const glm::vec4 sunOff = PixelAt(m_SunPair.OffPixels, k_CheckWidth, inSun);
			Check(sunOn == sunOff, std::format("the floor in the sun is unchanged by shadows ({:.3f} against {:.3f})", sunOn.g, sunOff.g));
		});

		readBack(m_EntityTarget, [this, behindBox, inSun](const TexturePixels& pixels)
		{
			// The entity scene's camera matches the sun scene's, at the target's aspect.
			const glm::vec4 hidden = PixelAt(pixels, ToPixel(CameraFor(Mode::Sun, static_cast<float>(k_CheckWidth) / static_cast<float>(k_CheckHeight)).GetViewProjectionMatrix(), { 0.0f, 1.0f, 0.0f }, k_CheckWidth, k_CheckHeight));
			const glm::vec4 shadow = PixelAt(pixels, behindBox);
			const glm::vec4 sun = PixelAt(pixels, inSun);
			Check(std::abs(hidden.r - hidden.g) < 0.05f && shadow.g < 0.75f * sun.g,
				std::format("a ShadowsOnly box through the ECS isn't drawn but shadows the floor (where it stands {:.2f}/{:.2f}; shadow {:.3f}, sun {:.3f})", hidden.r, hidden.g, shadow.g, sun.g));
		});

		const glm::mat4 acneViewProjection = acneCamera.GetViewProjectionMatrix();
		readBack(m_AcnePair.Off, [this](const TexturePixels& pixels) { m_AcnePair.OffPixels = pixels.Data; });
		readBack(m_AcnePair.On, [this, acneViewProjection](const TexturePixels& pixels)
		{
			int samples = 0, darker = 0;
			for (int z = -3; z <= 3; ++z)
			{
				for (int x = -3; x <= 3; ++x)
				{
					const glm::ivec2 pixel = ToPixel(acneViewProjection, { static_cast<float>(x), 0.0f, static_cast<float>(z) }, k_CheckWidth, k_CheckHeight);
					const glm::vec4 on = PixelAt(pixels, pixel);
					const glm::vec4 off = PixelAt(m_AcnePair.OffPixels, k_CheckWidth, pixel);
					++samples;
					darker += on.g < off.g - 2.0f / 255.0f ? 1 : 0;
				}
			}
			Check(darker == 0, std::format("a plane the sun grazes at 80 degrees doesn't shadow itself ({} of {} points darker)", darker, samples));
		});

		if (m_Fox)
		{
			const glm::vec3 foxCenter = 0.5f * (m_FoxMin + m_FoxMax);
			const float torso = 0.6f * (m_FoxMax.y - m_FoxMin.y);
			const glm::vec3 shadowPoint = ShadowOnFloor({ foxCenter.x, 0.0f, foxCenter.z }, torso, k_FoxSunDirection);
			const glm::ivec2 pixel = ToPixel(foxCamera.GetViewProjectionMatrix(), shadowPoint, k_CheckWidth, k_CheckHeight);
			readBack(m_FoxPair.Off, [this](const TexturePixels& pixels) { m_FoxPair.OffPixels = pixels.Data; });
			readBack(m_FoxPair.On, [this, pixel](const TexturePixels& pixels)
			{
				const glm::vec4 on = PixelAt(pixels, pixel);
				const glm::vec4 off = PixelAt(m_FoxPair.OffPixels, k_CheckWidth, pixel);
				Check(on.g < 0.85f * off.g, std::format("the skinned Fox casts onto the floor beside it ({:.3f} against {:.3f})", on.g, off.g));
			});
		}
		else
		{
			Check(false, "Fox.gltf loads for the skinned shadow check");
		}
	}

	void ShadowTest::Update(float deltaTime)
	{
		m_Time += deltaTime;

		if (!m_ChecksDone && !Renderer::IsFrameSkipped())
		{
			m_ChecksDone = true;
			RunChecks();
		}

		if (m_Fox && m_Mode == Mode::Skinned)
		{
			m_FoxAnimator.Update(deltaTime);
			const std::span<const glm::mat4> palette = m_FoxAnimator.GetSkinningPalette();
			if (!palette.empty())
				m_FoxPalette.assign(palette.begin(), palette.end());
		}

		PerspectiveCamera camera = CameraFor(m_Mode, m_AspectRatio);
		if (m_Pan)
		{
			const glm::vec3 drift(0.25f * std::sin(0.2f * m_Time), 0.0f, 0.25f * std::cos(0.2f * m_Time));
			camera.SetPosition(camera.GetPosition() + drift);
		}
		m_Camera = camera;
		DrawInto(nullptr, m_Mode, m_Shadows, m_Camera);
	}

	void ShadowTest::Cleanup()
	{
		m_Alive.reset();
		Application::Get().GetRenderer3D().SetShadowSettings(m_DefaultSettings);

		for (CheckPair* pair : { &m_SunPair, &m_AcnePair, &m_FoxPair })
		{
			DestroyAndDelete(pair->On);
			DestroyAndDelete(pair->Off);
			pair->OffPixels.clear();
		}
		DestroyAndDelete(m_EntityTarget);

		delete m_EntityScene;
		m_EntityScene = nullptr;
		delete m_FoxMaterial;
		m_FoxMaterial = nullptr;
		m_FoxAnimator = Animator();
		m_FoxPalette.clear();
		delete m_Fox;
		m_Fox = nullptr;
		m_Checks.clear();
	}

	void ShadowTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void ShadowTest::ImGuiRender()
	{
		int mode = static_cast<int>(m_Mode);
		ImGui::RadioButton("Sun", &mode, static_cast<int>(Mode::Sun));
		ImGui::SameLine();
		ImGui::RadioButton("Grazing plane", &mode, static_cast<int>(Mode::Acne));
		ImGui::SameLine();
		ImGui::RadioButton("Fox", &mode, static_cast<int>(Mode::Skinned));
		m_Mode = static_cast<Mode>(mode);

		ImGui::Checkbox("Sun casts shadows", &m_Shadows);
		ImGui::Checkbox("Pan slowly (shimmer)", &m_Pan);

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		Renderer3DShadowSettings settings = renderer.GetShadowSettings();
		bool changed = false;
		int cascades = static_cast<int>(settings.CascadeCount);
		changed |= ImGui::SliderInt("Cascades", &cascades, 1, static_cast<int>(Renderer3D::k_MaxShadowCascades));
		settings.CascadeCount = static_cast<uint32_t>(cascades);
		changed |= ImGui::SliderFloat("Max distance", &settings.MaxDistance, 5.0f, 150.0f);
		changed |= ImGui::SliderFloat("Split lambda", &settings.SplitLambda, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Cascade blend", &settings.CascadeBlend, 0.0f, 0.5f);
		changed |= ImGui::SliderInt("Depth bias", &settings.DepthBias, 0, 64);
		changed |= ImGui::SliderFloat("Slope bias", &settings.SlopeBias, 0.0f, 8.0f);
		changed |= ImGui::SliderFloat("Normal bias (texels)", &settings.NormalBias, 0.0f, 6.0f);
		changed |= ImGui::Checkbox("Tint by cascade", &settings.DebugCascades);
		if (changed)
			renderer.SetShadowSettings(settings);

		const Renderer3D::Statistics& stats = renderer.GetStatistics();
		ImGui::Text("Views %u, casters %u, atlas draws %u", stats.ShadowViews, stats.ShadowCasters, stats.ShadowDrawCalls);

		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
