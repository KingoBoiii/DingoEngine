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
		// The grazed plane's positive control: a 0.4 m block in its far corner, whose shadow runs 2.3 m
		// towards -x, clear of the self-shadowing grid (z = -3..3).
		const glm::vec3 k_AcneBlock{ 3.5f, 0.2f, -3.5f };
		const glm::vec3 k_AcneBlockShadow{ 2.2f, 0.0f, -3.5f };

		const glm::vec4 k_FloorColor{ 0.6f, 0.6f, 0.6f, 1.0f };

		// The sun scene: a 1 x 2 x 1 box at the origin, a pillar nearby and one 35 m away.
		const glm::vec3 k_BoxSize{ 1.0f, 2.0f, 1.0f };
		const glm::vec3 k_NearPillar{ 10.0f, 2.0f, 4.0f };
		const glm::vec3 k_FarPillar{ 20.0f, 2.0f, 24.0f };
		const glm::vec3 k_PillarSize{ 1.0f, 4.0f, 1.0f };

		const glm::vec3 k_SpotPosition{ -3.0f, 4.0f, 0.0f };
		const glm::vec3 k_SpotTarget{ 2.0f, 0.0f, 0.0f };
		const glm::vec3 k_PointPosition{ 0.0f, 2.0f, 0.0f };
		constexpr float k_PillarRing = 3.0f;
		constexpr int k_BudgetLights = 12;

		// Shadow probes: a row across the edge of the box's sun shadow, 5 cm above the floor, where it
		// crosses z = -0.3 at about x = -1.475; points behind each occluder and in the open.
		constexpr int k_SweepProbes = 64;
		constexpr uint64_t k_SweepKey = 1000;
		constexpr uint64_t k_SunBehindKey = 1;
		constexpr uint64_t k_SunOpenKey = 2;
		constexpr uint64_t k_SpotBehindKey = 10;
		constexpr uint64_t k_SpotOpenKey = 11;
		constexpr uint64_t k_UncastKey = 12;
		constexpr uint64_t k_PointBehindKey = 20;
		const glm::vec3 k_SunBehind{ -0.6f, 0.3f, -0.3f };
		const glm::vec3 k_SunOpen{ 3.0f, 0.3f, -2.0f };
		const glm::vec3 k_SpotBehind{ 0.9f, 0.2f, 0.0f };
		const glm::vec3 k_SpotOpen{ 2.5f, 0.3f, 1.5f };
		const glm::vec3 k_Uncast{ -6.0f, 0.5f, 3.0f };
		const glm::vec3 k_PointBehind{ 4.5f, 0.3f, 0.0f };

		glm::vec3 SweepPoint(int i)
		{
			return { -1.75f + 0.5f * static_cast<float>(i) / static_cast<float>(k_SweepProbes - 1), 0.05f, -0.3f };
		}

		// Where on the floor the ray from a light at `light` through `occluder` lands.
		glm::vec3 ShadowFromLight(const glm::vec3& light, const glm::vec3& occluder)
		{
			return light + (occluder - light) * (light.y / (light.y - occluder.y));
		}

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
			else if (*mode == "spot")
				m_Mode = Mode::Spot;
			else if (*mode == "point")
				m_Mode = Mode::Point;
			else if (*mode == "budget")
				m_Mode = Mode::Budget;
			else if (*mode == "probe")
				m_Mode = Mode::Probe;
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
		m_SpotPair = { MakeTarget("ShadowTest spot on"), MakeTarget("ShadowTest spot off") };
		m_PointPair = { MakeTarget("ShadowTest point on"), MakeTarget("ShadowTest point off") };
		m_BudgetPair = { MakeTarget("ShadowTest budget first"), MakeTarget("ShadowTest budget second") };
		m_ProbeTargets = { MakeTarget("ShadowTest probes sun"), MakeTarget("ShadowTest probes spot"), MakeTarget("ShadowTest probes point") };
		m_ProbeFramesLeft = 0;
		m_BudgetSecondFramePending = false;
		m_FadePixelsPending = false;
		m_FadeHardPixels.clear();
		m_FadeBandPixels.clear();
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

		m_EntitySun = m_EntityScene->CreateEntity("Sun");
		DirectionalLightComponent& sun = m_EntitySun.AddComponent<DirectionalLightComponent>();
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
			case Mode::Spot:
				camera.SetPosition({ 2.0f, 7.0f, 9.0f });
				camera.SetTarget({ 0.5f, 0.0f, 0.0f });
				break;
			case Mode::Point:
				camera.SetPosition({ 0.0f, 17.0f, 4.0f });
				camera.SetTarget({ 0.0f, 0.0f, 0.0f });
				break;
			case Mode::Budget:
				camera.SetPosition({ 0.0f, 12.0f, 14.0f });
				camera.SetTarget({ 0.0f, 0.0f, 0.0f });
				break;
			case Mode::Sun:
			case Mode::Probe:
			default:
				camera.SetPosition({ -5.0f, 4.0f, -6.0f });
				camera.SetTarget({ 2.0f, 0.0f, 3.0f });
				break;
		}
		return camera;
	}

	void ShadowTest::DrawMode(Renderer3D& renderer, Mode mode, bool shadows, const std::vector<glm::mat4>* palette, bool probes) const
	{
		Mesh* box = renderer.GetBoxMesh();
		if (mode == Mode::Spot || mode == Mode::Point || mode == Mode::Budget)
		{
			renderer.SetAmbientLight(glm::vec3(1.0f), 0.15f);
			renderer.SubmitMesh(box, Box({ 0.0f, -0.05f, 0.0f }, { 30.0f, 0.1f, 30.0f }), k_FloorColor);
			if (mode == Mode::Spot)
			{
				SpotLight spot;
				spot.Position = k_SpotPosition;
				spot.Direction = k_SpotTarget - k_SpotPosition;
				spot.Intensity = 1.5f;
				spot.Range = 15.0f;
				spot.InnerConeAngle = 25.0f;
				spot.OuterConeAngle = 40.0f;
				spot.CastShadows = shadows;
				renderer.SubmitLight(spot);
				const ShadowProbeLight spotLight = renderer.GetLastSubmittedLight();
				renderer.SubmitMesh(box, Box({ 0.0f, 0.5f, 0.0f }, glm::vec3(1.0f)), { 0.8f, 0.4f, 0.3f, 1.0f });
				if (probes)
				{
					renderer.AddShadowProbe(spotLight, k_SpotBehind, k_SpotBehindKey);
					renderer.AddShadowProbe(spotLight, k_SpotOpen, k_SpotOpenKey);
					PointLight uncast;
					uncast.Position = k_Uncast + glm::vec3(0.0f, 0.5f, 0.0f);
					uncast.Range = 2.0f;
					renderer.SubmitLight(uncast);
					renderer.AddShadowProbe(renderer.GetLastSubmittedLight(), k_Uncast, k_UncastKey);
				}
			}
			else if (mode == Mode::Point)
			{
				PointLight point;
				point.Position = k_PointPosition;
				point.Intensity = 1.5f;
				point.Range = 12.0f;
				point.CastShadows = shadows;
				renderer.SubmitLight(point);
				if (probes)
					renderer.AddShadowProbe(renderer.GetLastSubmittedLight(), k_PointBehind, k_PointBehindKey);
				for (const glm::vec2 offset : { glm::vec2(1, 0), glm::vec2(-1, 0), glm::vec2(0, 1), glm::vec2(0, -1) })
					renderer.SubmitMesh(box, Box({ offset.x * k_PillarRing, 0.6f, offset.y * k_PillarRing }, { 0.6f, 1.2f, 0.6f }), { 0.4f, 0.5f, 0.8f, 1.0f });
				renderer.SubmitMesh(renderer.GetSphereMesh(), Box(k_PointPosition, glm::vec3(0.2f)), glm::vec4(1.0f), nullptr, ShadowCasting::Off);
			}
			else
			{
				for (int i = 0; i < k_BudgetLights; ++i)
				{
					const float x = -11.0f + 2.0f * static_cast<float>(i);
					SpotLight spot;
					spot.Position = { x, 3.0f, -1.5f };
					spot.Direction = { 0.0f, -1.0f, 0.6f };
					spot.Intensity = 0.8f;
					spot.Range = 8.0f;
					spot.OuterConeAngle = 35.0f;
					spot.CastShadows = shadows;
					renderer.SubmitLight(spot);
					renderer.SubmitMesh(box, Box({ x, 0.3f, 0.0f }, glm::vec3(0.6f)), { 0.8f, 0.4f, 0.3f, 1.0f });
				}
			}
			return;
		}

		DirectionalLight sun;
		sun.Intensity = 1.0f;
		sun.CastShadows = shadows;
		sun.Direction = mode == Mode::Acne ? k_GrazingDirection : mode == Mode::Skinned ? k_FoxSunDirection : k_SunDirection;
		renderer.SubmitLight(sun);
		renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);

		if (mode == Mode::Probe || probes)
		{
			const ShadowProbeLight sunLight = renderer.GetLastSubmittedLight();
			renderer.AddShadowProbe(sunLight, k_SunBehind, k_SunBehindKey);
			renderer.AddShadowProbe(sunLight, k_SunOpen, k_SunOpenKey);
			for (int i = 0; i < k_SweepProbes; ++i)
			{
				renderer.AddShadowProbe(sunLight, SweepPoint(i), k_SweepKey + i);
				if (mode == Mode::Probe)
				{
					// Red in shadow, green lit; the spheres themselves cast nothing.
					const float lit = renderer.GetShadowProbeResult(k_SweepKey + i).value_or(1.0f);
					renderer.SubmitMesh(renderer.GetSphereMesh(), Box(SweepPoint(i) + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(0.02f)),
						{ 1.0f - lit, lit, 0.1f, 1.0f }, nullptr, ShadowCasting::Off);
				}
			}
		}

		switch (mode)
		{
			case Mode::Acne:
				renderer.SubmitMesh(box, Box({ 0.0f, -0.025f, 0.0f }, { 8.0f, 0.05f, 8.0f }), k_FloorColor);
				renderer.SubmitMesh(box, Box(k_AcneBlock, glm::vec3(0.4f)), { 0.8f, 0.4f, 0.3f, 1.0f });
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

	void ShadowTest::DrawInto(Framebuffer* target, Mode mode, bool shadows, const PerspectiveCamera& camera, bool probes)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		Framebuffer* previous = Renderer::GetRenderTarget();
		if (target)
			Renderer::SetRenderTarget(target);

		renderer.BeginScene(camera);
		renderer.Clear(m_ClearColor);
		DrawMode(renderer, mode, shadows, &m_FoxPalette, probes);
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
		const glm::ivec2 acneBlockShadow = ToPixel(acneViewProjection, k_AcneBlockShadow, k_CheckWidth, k_CheckHeight);
		readBack(m_AcnePair.On, [this, acneViewProjection, acneBlockShadow](const TexturePixels& pixels)
		{
			const glm::vec4 blockOn = PixelAt(pixels, acneBlockShadow);
			const glm::vec4 blockOff = PixelAt(m_AcnePair.OffPixels, k_CheckWidth, acneBlockShadow);
			Check(blockOn.g < 0.75f * blockOff.g, std::format("the grazing sun's shadows do draw: a block on the plane casts its long shadow ({:.3f} against {:.3f})", blockOn.g, blockOff.g));

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

		RunLocalChecks();
	}

	void ShadowTest::RunLocalChecks()
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		const float aspect = static_cast<float>(k_CheckWidth) / static_cast<float>(k_CheckHeight);
		const PerspectiveCamera spotCamera = CameraFor(Mode::Spot, aspect);
		const PerspectiveCamera pointCamera = CameraFor(Mode::Point, aspect);
		const PerspectiveCamera budgetCamera = CameraFor(Mode::Budget, aspect);

		DrawInto(m_SpotPair.On, Mode::Spot, true, spotCamera);
		const Renderer3D::Statistics spotStats = renderer.GetStatistics();
		DrawInto(m_SpotPair.Off, Mode::Spot, false, spotCamera);
		DrawInto(m_PointPair.On, Mode::Point, true, pointCamera);
		const Renderer3D::Statistics pointStats = renderer.GetStatistics();
		DrawInto(m_PointPair.Off, Mode::Point, false, pointCamera);
		DrawInto(m_BudgetPair.On, Mode::Budget, true, budgetCamera);
		const Renderer3D::Statistics budgetStats = renderer.GetStatistics();
		m_BudgetSecondFramePending = true;

		Check(spotStats.ShadowViews == 1 && spotStats.ShadowedLights == 1 && spotStats.ShadowCascades == 0,
			std::format("a casting spot light renders one tile ({} tiles, {} shadowed lights)", spotStats.ShadowViews, spotStats.ShadowedLights));
		Check(pointStats.ShadowViews == 6 && pointStats.ShadowedLights == 1,
			std::format("a casting point light renders six cube faces ({} tiles)", pointStats.ShadowViews));
		const uint32_t cap = (std::min)(renderer.GetCapabilities().MaxShadowedLocalLights, Renderer3D::k_MaxShadowedLocalLights);
		Check(budgetStats.ShadowedLights == cap && budgetStats.UnshadowedLights == k_BudgetLights - cap && budgetStats.LocalLights == static_cast<uint32_t>(k_BudgetLights),
			std::format("{} casting spot lights, {} shadow slots: the rest light unshadowed ({} shadowed, {} unshadowed)", k_BudgetLights, cap, budgetStats.ShadowedLights, budgetStats.UnshadowedLights));

		{
			const SpotLightComponent component;
			SpotLightComponent casting = component;
			casting.CastShadows = true;
			casting.ShadowStrength = 0.5f;
			const SpotLight light = casting.ToLight(Transform3DComponent());
			Check(light.CastShadows && light.ShadowStrength == 0.5f && !component.ToLight(Transform3DComponent()).CastShadows,
				"SpotLightComponent::ToLight carries CastShadows and ShadowStrength");
		}

		// Shadow slots go by rank even when every light fits the budget: ten casting spot lights in a
		// row away from the camera, submitted farthest first. The two farthest light unshadowed, and
		// a probe of an unshadowed light answers at once while a shadowed one waits for the GPU.
		{
			Renderer3D* rankRenderer = Renderer3D::Create();
			PerspectiveCamera camera(60.0f, aspect, 0.1f, 100.0f);
			camera.SetPosition({ 0.0f, 4.0f, 6.0f });
			camera.SetTarget({ 0.0f, 0.0f, -6.0f });
			constexpr int k_RankLights = 10;
			constexpr uint64_t k_RankKey = 7000;
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(m_EntityTarget);
			const Viewport inset{ 0.0f, 0.0f, static_cast<float>(k_CheckWidth / 2), static_cast<float>(k_CheckHeight / 2) };
			Renderer::SetViewport(inset);
			rankRenderer->BeginScene(camera);
			rankRenderer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
			for (int i = 0; i < k_RankLights; ++i)
			{
				const float z = -3.0f * static_cast<float>(k_RankLights - 1 - i);
				SpotLight spot;
				spot.Position = { 0.0f, 3.0f, z };
				spot.Direction = { 0.0f, -1.0f, 0.0f };
				spot.Range = 4.0f;
				spot.OuterConeAngle = 35.0f;
				spot.CastShadows = true;
				rankRenderer->SubmitLight(spot);
				rankRenderer->AddShadowProbe(rankRenderer->GetLastSubmittedLight(), { 0.0f, 1.0f, z }, k_RankKey + i);
			}
			rankRenderer->SubmitMesh(rankRenderer->GetBoxMesh(), Box({ 0.0f, -0.05f, -12.0f }, { 4.0f, 0.1f, 32.0f }), glm::vec4(1.0f));
			rankRenderer->EndScene();
			const std::optional<Viewport> kept = Renderer::GetViewport();
			Renderer::SetRenderTarget(previous);
			Check(kept && kept->Width == inset.Width && kept->Height == inset.Height,
				"a scene with shadows and probes keeps the viewport it was drawn in");

			const uint32_t slots = (std::min)(rankRenderer->GetCapabilities().MaxShadowedLocalLights, Renderer3D::k_MaxShadowedLocalLights);
			std::string unshadowed;
			for (int i = 0; i < k_RankLights; ++i)
				if (rankRenderer->GetShadowProbeResult(k_RankKey + i).has_value())
					unshadowed += std::format("{}{}", unshadowed.empty() ? "" : ", ", i);
			std::string expected;
			for (int i = 0; i < k_RankLights - static_cast<int>(slots); ++i)
				expected += std::format("{}{}", expected.empty() ? "" : ", ", i);
			const Renderer3D::Statistics stats = rankRenderer->GetStatistics();
			rankRenderer->Shutdown();
			delete rankRenderer;
			Check(stats.LocalLights == static_cast<uint32_t>(k_RankLights) && stats.ShadowedLights == slots && unshadowed == expected,
				std::format("with every light inside the budget, the shadow slots go to the {} nearest (unshadowed: submissions [{}], expected [{}])", slots, unshadowed, expected));
		}

		// The budget fade, on its own renderer: four light slots and two shadow slots, six casting point
		// lights in a row away from the camera, each with a block beside it. Drawn with a hard cut and
		// with a band, read back after each: the band dims the last drawn light, and the second
		// shadowed light's shadow, at the shadow slots' edge, lightens.
		{
			Renderer3DParams params;
			params.Capabilities.MaxLocalLights = 4;
			params.Capabilities.MaxShadowedLocalLights = 2;
			Renderer3D* fadeRenderer = Renderer3D::Create(params);
			PerspectiveCamera fadeCamera(45.0f, aspect, 0.1f, 100.0f);
			fadeCamera.SetPosition({ 0.0f, 8.0f, 4.0f });
			fadeCamera.SetTarget({ 0.0f, 0.0f, -6.0f });
			auto lightZ = [](int i) { return -2.0f - 2.5f * static_cast<float>(i); };
			const std::weak_ptr<int> fadeAlive = m_Alive;
			auto drawLights = [&](float band, std::vector<uint8_t>* pixelsOut)
			{
				fadeRenderer->SetLightBudgetFade(band);
				Framebuffer* previous = Renderer::GetRenderTarget();
				Renderer::SetRenderTarget(m_EntityTarget);
				fadeRenderer->BeginScene(fadeCamera);
				fadeRenderer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
				fadeRenderer->SetAmbientLight(glm::vec3(1.0f), 0.05f);
				for (int i = 0; i < 6; ++i)
				{
					PointLight light;
					light.Position = { 0.0f, 1.0f, lightZ(i) };
					light.Range = 2.0f;
					light.CastShadows = true;
					fadeRenderer->SubmitLight(light);
					fadeRenderer->SubmitMesh(fadeRenderer->GetBoxMesh(), Box({ 0.35f, 0.2f, lightZ(i) }, glm::vec3(0.4f)), glm::vec4(1.0f));
				}
				fadeRenderer->SubmitMesh(fadeRenderer->GetBoxMesh(), Box({ 0.0f, -0.05f, -8.0f }, { 30.0f, 0.1f, 40.0f }), glm::vec4(1.0f));
				fadeRenderer->EndScene();
				m_EntityTarget->GetAttachment(0)->ReadPixels([fadeAlive, pixelsOut](const TexturePixels& pixels)
				{
					if (!fadeAlive.expired())
						*pixelsOut = pixels.Data;
				});
				Renderer::SetRenderTarget(previous);
				return fadeRenderer->GetStatistics();
			};
			const Renderer3D::Statistics hard = drawLights(0.0f, &m_FadeHardPixels);
			const Renderer3D::Statistics faded = drawLights(0.5f, &m_FadeBandPixels);
			fadeRenderer->Shutdown();
			delete fadeRenderer;
			Check(hard.LocalLights == 4 && hard.FadedLights == 0 && faded.LocalLights == 4 && faded.FadedLights > 0 && hard.ShadowedLights == 2 && faded.ShadowedLights == 2,
				std::format("past the light budget, a fade band dims the lights at its edge (hard cut {} faded, band 0.5 {} faded; {} and {} shadowed)", hard.FadedLights, faded.FadedLights, hard.ShadowedLights, faded.ShadowedLights));

			const glm::mat4 fadeViewProjection = fadeCamera.GetViewProjectionMatrix();
			m_FadeLastLitPixel = ToPixel(fadeViewProjection, { -0.5f, 0.0f, lightZ(3) }, k_CheckWidth, k_CheckHeight);
			m_FadeShadowPixel = ToPixel(fadeViewProjection, ShadowFromLight({ 0.0f, 1.0f, lightZ(1) }, { 0.45f, 0.4f, lightZ(1) }), k_CheckWidth, k_CheckHeight);
			m_FadePixelsPending = true;
		}

		const std::weak_ptr<int> alive = m_Alive;
		auto readBack = [alive](Framebuffer* target, std::function<void(const TexturePixels&)> check)
		{
			target->GetAttachment(0)->ReadPixels([alive, check = std::move(check)](const TexturePixels& pixels)
			{
				if (!alive.expired())
					check(pixels);
			});
		};

		const glm::mat4 spotViewProjection = spotCamera.GetViewProjectionMatrix();
		const glm::ivec2 behindSpotBox = ToPixel(spotViewProjection, ShadowFromLight(k_SpotPosition, { 0.2f, 0.6f, 0.0f }), k_CheckWidth, k_CheckHeight);
		const glm::ivec2 spotLit = ToPixel(spotViewProjection, { 2.5f, 0.0f, 1.5f }, k_CheckWidth, k_CheckHeight);
		readBack(m_SpotPair.Off, [this](const TexturePixels& pixels) { m_SpotPair.OffPixels = pixels.Data; });
		readBack(m_SpotPair.On, [this, behindSpotBox, spotLit](const TexturePixels& pixels)
		{
			const glm::vec4 on = PixelAt(pixels, behindSpotBox);
			const glm::vec4 off = PixelAt(m_SpotPair.OffPixels, k_CheckWidth, behindSpotBox);
			Check(on.g < 0.75f * off.g, std::format("the floor behind a box goes dark when a spot light casts ({:.3f} against {:.3f})", on.g, off.g));
			const glm::vec4 litOn = PixelAt(pixels, spotLit);
			const glm::vec4 litOff = PixelAt(m_SpotPair.OffPixels, k_CheckWidth, spotLit);
			Check(litOn == litOff && litOff.g > 0.3f, std::format("the floor in the spot light's cone is unchanged where nothing stands ({:.3f} against {:.3f})", litOn.g, litOff.g));
		});

		const glm::mat4 pointViewProjection = pointCamera.GetViewProjectionMatrix();
		std::vector<glm::ivec2> behindPillars, openFloor;
		for (const glm::vec2 offset : { glm::vec2(1, 0), glm::vec2(-1, 0), glm::vec2(0, 1), glm::vec2(0, -1) })
		{
			const glm::vec3 occluder(offset.x * k_PillarRing, 0.6f, offset.y * k_PillarRing);
			behindPillars.push_back(ToPixel(pointViewProjection, ShadowFromLight(k_PointPosition, occluder), k_CheckWidth, k_CheckHeight));
		}
		// The diagonals lie on the seams between cube faces, and close to the light the floor is on the
		// -y face.
		for (const glm::vec3 point : { glm::vec3(2, 0, 2), glm::vec3(-2, 0, 2), glm::vec3(2, 0, -2), glm::vec3(-2, 0, -2),
			glm::vec3(4, 0, 4), glm::vec3(-4, 0, -4), glm::vec3(1, 0, 0.5f), glm::vec3(-0.5f, 0, -1) })
			openFloor.push_back(ToPixel(pointViewProjection, point, k_CheckWidth, k_CheckHeight));

		readBack(m_PointPair.Off, [this](const TexturePixels& pixels) { m_PointPair.OffPixels = pixels.Data; });
		readBack(m_PointPair.On, [this, behindPillars, openFloor](const TexturePixels& pixels)
		{
			int dark = 0;
			for (const glm::ivec2& pixel : behindPillars)
				dark += PixelAt(pixels, pixel).g < 0.75f * PixelAt(m_PointPair.OffPixels, k_CheckWidth, pixel).g ? 1 : 0;
			Check(dark == 4, std::format("a point light's four pillars cast on the +x, -x, +z and -z faces ({} of 4 dark)", dark));

			int changed = 0;
			for (const glm::ivec2& pixel : openFloor)
				changed += PixelAt(pixels, pixel) != PixelAt(m_PointPair.OffPixels, k_CheckWidth, pixel) ? 1 : 0;
			Check(changed == 0, std::format("the open floor around a point light is unchanged, across the cube's seams and on its -y face ({} of {} changed)", changed, openFloor.size()));
		});

		readBack(m_BudgetPair.On, [this](const TexturePixels& pixels) { m_BudgetPair.OffPixels = pixels.Data; });

		// Answers come back a frame or two later, so the probe checks issue probes for a few frames.
		m_ProbeFramesLeft = 8;
		m_ProbeAnswerFrame = -1;
		UpdateProbeChecks();
	}

	// The budget scene again, a frame after the first: tiers, cascades and the versioned buffers must
	// give the same picture frame to frame, not only twice in one frame.
	void ShadowTest::DrawBudgetSecondFrame()
	{
		m_BudgetSecondFramePending = false;
		const float aspect = static_cast<float>(k_CheckWidth) / static_cast<float>(k_CheckHeight);
		DrawInto(m_BudgetPair.Off, Mode::Budget, true, CameraFor(Mode::Budget, aspect));
		const std::weak_ptr<int> alive = m_Alive;
		m_BudgetPair.Off->GetAttachment(0)->ReadPixels([this, alive](const TexturePixels& pixels)
		{
			if (!alive.expired())
				Check(!pixels.Data.empty() && pixels.Data == m_BudgetPair.OffPixels, "a still scene of twelve casting lights draws the same picture in two consecutive frames");
		});
	}

	void ShadowTest::CheckFadePixels()
	{
		m_FadePixelsPending = false;
		const glm::vec4 litHard = PixelAt(m_FadeHardPixels, k_CheckWidth, m_FadeLastLitPixel);
		const glm::vec4 litBand = PixelAt(m_FadeBandPixels, k_CheckWidth, m_FadeLastLitPixel);
		Check(litHard.g > 0.1f && litBand.g < 0.85f * litHard.g,
			std::format("the band visibly dims the last light inside the budget ({:.3f} against {:.3f} with a hard cut)", litBand.g, litHard.g));
		const glm::vec4 shadowHard = PixelAt(m_FadeHardPixels, k_CheckWidth, m_FadeShadowPixel);
		const glm::vec4 shadowBand = PixelAt(m_FadeBandPixels, k_CheckWidth, m_FadeShadowPixel);
		Check(shadowBand.g > shadowHard.g + 0.02f,
			std::format("and hands the last shadow slot over: that light's shadow lightens ({:.3f} against {:.3f} with a hard cut)", shadowBand.g, shadowHard.g));
	}

	void ShadowTest::UpdateProbeChecks()
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		const float aspect = static_cast<float>(k_CheckWidth) / static_cast<float>(k_CheckHeight);

		// A light without a shadow answers at once, before the GPU is asked anything.
		const bool firstFrame = m_ProbeFramesLeft == 8;
		DrawInto(m_ProbeTargets[0], Mode::Sun, true, CameraFor(Mode::Sun, aspect), true);
		DrawInto(m_ProbeTargets[1], Mode::Spot, true, CameraFor(Mode::Spot, aspect), true);
		if (firstFrame)
		{
			const std::optional<float> uncast = renderer.GetShadowProbeResult(k_UncastKey);
			Check(uncast && *uncast == 1.0f, std::format("a probe of a light without a shadow answers 1 at once ({})", uncast ? std::format("{:.3f}", *uncast) : "no answer"));
		}
		DrawInto(m_ProbeTargets[2], Mode::Point, true, CameraFor(Mode::Point, aspect), true);
		const float entityVisibility = m_EntityScene->GetLightVisibility(m_EntitySun, k_SunBehind);
		Application::Get().GetSceneRenderer().Render(*m_EntityScene, m_EntityTarget);
		if (m_ProbeAnswerFrame < 0 && renderer.GetShadowProbeResult(k_SunBehindKey))
			m_ProbeAnswerFrame = 8 - m_ProbeFramesLeft;

		if (--m_ProbeFramesLeft > 0)
			return;

		if (m_FadePixelsPending)
			Check(false, "the budget fade's two frames read back");

		Check(m_ProbeAnswerFrame >= 1 && m_ProbeAnswerFrame <= 4,
			std::format("the GPU's probe answers arrive within four frames ({} frames)", m_ProbeAnswerFrame));

		auto answer = [&renderer](uint64_t key) { return renderer.GetShadowProbeResult(key).value_or(-1.0f); };
		const float sunBehind = answer(k_SunBehindKey);
		const float sunOpen = answer(k_SunOpenKey);
		Check(sunBehind >= 0.0f && sunBehind < 0.05f && sunOpen == 1.0f,
			std::format("a sun probe behind the box reads 0 and one in the open 1 ({:.3f}, {:.3f})", sunBehind, sunOpen));

		const float spotBehind = answer(k_SpotBehindKey);
		const float spotOpen = answer(k_SpotOpenKey);
		Check(spotBehind >= 0.0f && spotBehind < 0.05f && spotOpen == 1.0f,
			std::format("a spot light's probe behind its box reads 0 and one in its cone 1 ({:.3f}, {:.3f})", spotBehind, spotOpen));

		const float pointBehind = answer(k_PointBehindKey);
		Check(pointBehind >= 0.0f && pointBehind < 0.05f, std::format("a point light's probe behind a pillar reads 0 ({:.3f})", pointBehind));

		int inBetween = 0;
		const float first = answer(k_SweepKey);
		const float last = answer(k_SweepKey + k_SweepProbes - 1);
		for (int i = 0; i < k_SweepProbes; ++i)
		{
			const float value = answer(k_SweepKey + i);
			inBetween += value > 0.05f && value < 0.95f ? 1 : 0;
		}
		Check(first == 1.0f && last >= 0.0f && last < 0.05f && inBetween > 0,
			std::format("probes across the sun shadow's edge go from 1 to 0 through the PCF edge ({:.3f} .. {:.3f}, {} in between)", first, last, inBetween));

		Check(entityVisibility >= 0.0f && entityVisibility < 0.05f,
			std::format("Scene::GetLightVisibility of the sun behind a ShadowsOnly box reads 0 ({:.3f})", entityVisibility));
	}


	void ShadowTest::Update(float deltaTime)
	{
		m_Time += deltaTime;

		if (!m_ChecksDone && !Renderer::IsFrameSkipped())
		{
			m_ChecksDone = true;
			RunChecks();
		}
		else if (m_ProbeFramesLeft > 0 && !Renderer::IsFrameSkipped())
		{
			if (m_BudgetSecondFramePending)
				DrawBudgetSecondFrame();
			if (m_FadePixelsPending && !m_FadeHardPixels.empty() && !m_FadeBandPixels.empty())
				CheckFadePixels();
			UpdateProbeChecks();
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

		for (CheckPair* pair : { &m_SunPair, &m_AcnePair, &m_FoxPair, &m_SpotPair, &m_PointPair, &m_BudgetPair })
		{
			DestroyAndDelete(pair->On);
			DestroyAndDelete(pair->Off);
			pair->OffPixels.clear();
		}
		DestroyAndDelete(m_EntityTarget);
		for (Framebuffer*& target : m_ProbeTargets)
			DestroyAndDelete(target);
		m_ProbeFramesLeft = 0;
		m_EntitySun = {};

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
		ImGui::RadioButton("Spot", &mode, static_cast<int>(Mode::Spot));
		ImGui::SameLine();
		ImGui::RadioButton("Point", &mode, static_cast<int>(Mode::Point));
		ImGui::SameLine();
		ImGui::RadioButton("Budget", &mode, static_cast<int>(Mode::Budget));
		ImGui::SameLine();
		ImGui::RadioButton("Probes", &mode, static_cast<int>(Mode::Probe));
		m_Mode = static_cast<Mode>(mode);

		ImGui::Checkbox("Lights cast shadows", &m_Shadows);
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
		int localResolution = static_cast<int>(settings.LocalShadowResolution);
		changed |= ImGui::SliderInt("Local light tile", &localResolution, 128, 2048);
		settings.LocalShadowResolution = static_cast<uint32_t>(localResolution);
		changed |= ImGui::Checkbox("Tint by cascade", &settings.DebugCascades);
		if (changed)
			renderer.SetShadowSettings(settings);

		const Renderer3D::Statistics& stats = renderer.GetStatistics();
		ImGui::Text("Tiles %u (%u cascades), casters %u, atlas draws %u", stats.ShadowViews, stats.ShadowCascades, stats.ShadowCasters, stats.ShadowDrawCalls);
		ImGui::Text("Local lights %u shadowed, %u unshadowed", stats.ShadowedLights, stats.UnshadowedLights);

		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
