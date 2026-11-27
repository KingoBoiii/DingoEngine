#include "SkyboxTest.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <limits>

namespace GlslEnvironment
{
	using uint = uint32_t;
	using glm::vec2;
	using glm::vec3;
	using glm::asin;
	using glm::atan;
	using glm::clamp;
	using glm::cos;
	using glm::normalize;
	using glm::sin;

#include "DingoEngine/Graphics/Shaders/CubeMap.glsl"
#include "DingoEngine/Graphics/Shaders/Environment.glsl"
}

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_Width = 128;
		constexpr uint32_t k_Height = 64;
		constexpr uint32_t k_FaceSize = 32;
		constexpr uint32_t k_Mips = 6;

		glm::vec3 Panorama(const glm::vec3& dir)
		{
			const glm::vec3 base(0.5f + 0.4f * dir.y, 0.4f + 0.3f * dir.x, 0.5f + 0.3f * dir.z);
			const glm::vec3 sun = glm::normalize(glm::vec3(0.3f, 0.6f, -0.5f));
			const float angle = std::acos(std::clamp(glm::dot(dir, sun), -1.0f, 1.0f));
			return base + 20.0f * std::exp(-(angle * angle) / (0.12f * 0.12f));
		}

		std::array<uint8_t, 4> EncodeRgbe(const glm::vec3& color)
		{
			const float largest = (std::max)({ color.r, color.g, color.b });
			if (largest < 1e-32f)
				return { 0, 0, 0, 0 };
			int exponent = 0;
			const float scale = std::frexp(largest, &exponent) * 256.0f / largest;
			return {
				static_cast<uint8_t>(color.r * scale),
				static_cast<uint8_t>(color.g * scale),
				static_cast<uint8_t>(color.b * scale),
				static_cast<uint8_t>(exponent + 128) };
		}

		glm::vec3 DecodeRgbe(const std::array<uint8_t, 4>& rgbe)
		{
			if (rgbe[3] == 0)
				return glm::vec3(0.0f);
			const float scale = std::ldexp(1.0f, static_cast<int>(rgbe[3]) - 136);
			return glm::vec3(rgbe[0], rgbe[1], rgbe[2]) * scale;
		}

		glm::vec3 SampleSource(const std::vector<float>& source, const glm::vec2& uv)
		{
			const float x = uv.x * static_cast<float>(k_Width) - 0.5f;
			const float y = uv.y * static_cast<float>(k_Height) - 0.5f;
			const float fx = std::floor(x);
			const float fy = std::floor(y);
			const float tx = x - fx;
			const float ty = y - fy;
			auto texel = [&](int ix, int iy)
			{
				ix = ((ix % static_cast<int>(k_Width)) + static_cast<int>(k_Width)) % static_cast<int>(k_Width);
				iy = std::clamp(iy, 0, static_cast<int>(k_Height) - 1);
				const size_t at = (static_cast<size_t>(iy) * k_Width + static_cast<size_t>(ix)) * 3;
				return glm::vec3(source[at], source[at + 1], source[at + 2]);
			};
			const int ix = static_cast<int>(fx);
			const int iy = static_cast<int>(fy);
			return glm::mix(glm::mix(texel(ix, iy), texel(ix + 1, iy), tx), glm::mix(texel(ix, iy + 1), texel(ix + 1, iy + 1), tx), ty);
		}

		glm::vec3 BakedReference(const std::vector<float>& source, uint32_t face, uint32_t x, uint32_t y)
		{
			glm::vec3 sum(0.0f);
			for (int j = 0; j < 2; ++j)
			{
				for (int i = 0; i < 2; ++i)
				{
					const glm::vec2 uv = (glm::vec2(static_cast<float>(x), static_cast<float>(y)) + glm::vec2(0.25f + 0.5f * static_cast<float>(i), 0.25f + 0.5f * static_cast<float>(j))) / static_cast<float>(k_FaceSize);
					sum += SampleSource(source, GlslEnvironment::DE_EquirectUv(GlslEnvironment::DE_CubeFaceDirection(face, uv)));
				}
			}
			return sum * 0.25f;
		}

		bool Close(const glm::vec3& a, const glm::vec3& b, float tolerance)
		{
			for (int i = 0; i < 3; ++i)
			{
				if (!(std::abs(a[i] - b[i]) <= tolerance * (std::max)(1.0f, std::abs(b[i]))))
					return false;
			}
			return true;
		}

		constexpr uint32_t k_ShotSize = 32;
		const glm::vec4 k_Clear(1.0f, 0.0f, 1.0f, 1.0f);

		glm::vec3 SrgbEncode(const glm::vec3& linear)
		{
			glm::vec3 out;
			for (int i = 0; i < 3; ++i)
			{
				const float c = (std::max)(linear[i], 0.0f);
				out[i] = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
			}
			return out;
		}

		glm::vec3 SkyReference(const std::vector<float>& source, const glm::vec3& direction, float intensity = 1.0f)
		{
			return glm::min(SrgbEncode(SampleSource(source, GlslEnvironment::DE_EquirectUv(glm::normalize(direction))) * intensity), glm::vec3(1.0f));
		}

		float MaxDifference(const glm::vec4& pixel, const glm::vec3& expected)
		{
			return (std::max)({ std::abs(pixel.r - expected.r), std::abs(pixel.g - expected.g), std::abs(pixel.b - expected.b) });
		}

		bool IsClear(const glm::vec4& pixel)
		{
			return std::abs(pixel.r - 1.0f) < 0.01f && pixel.g < 0.01f && std::abs(pixel.b - 1.0f) < 0.01f;
		}

		bool SamePixel(const glm::vec4& a, const glm::vec4& b)
		{
			return std::abs(a.r - b.r) < 0.005f && std::abs(a.g - b.g) < 0.005f && std::abs(a.b - b.b) < 0.005f;
		}

		void WritePanorama(const std::filesystem::path& file, uint32_t width, uint32_t height, std::vector<float>* decoded)
		{
			if (decoded)
				decoded->assign(static_cast<size_t>(width) * height * 3, 0.0f);
			std::vector<std::array<uint8_t, 4>> fileRows(static_cast<size_t>(width) * height);
			for (uint32_t row = 0; row < height; ++row)
			{
				for (uint32_t x = 0; x < width; ++x)
				{
					const glm::vec2 uv((static_cast<float>(x) + 0.5f) / static_cast<float>(width), (static_cast<float>(row) + 0.5f) / static_cast<float>(height));
					const std::array<uint8_t, 4> rgbe = EncodeRgbe(Panorama(GlslEnvironment::DE_EquirectDirection(uv)));
					if (decoded)
					{
						const glm::vec3 value = DecodeRgbe(rgbe);
						const size_t at = (static_cast<size_t>(row) * width + x) * 3;
						(*decoded)[at] = value.r;
						(*decoded)[at + 1] = value.g;
						(*decoded)[at + 2] = value.b;
					}
					fileRows[static_cast<size_t>(height - 1 - row) * width + x] = rgbe;
				}
			}
			std::ofstream out(file, std::ios::binary);
			const std::string header = std::format("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {} +X {}\n", height, width);
			out.write(header.data(), static_cast<std::streamsize>(header.size()));
			out.write(reinterpret_cast<const char*>(fileRows.data()), static_cast<std::streamsize>(fileRows.size() * 4));
		}

		struct CubeReadback
		{
			uint32_t Remaining = 0;
			uint32_t Empty = 0;
			std::array<std::vector<glm::vec3>, k_Mips * 6> Pixels;
		};
	}

	void SkyboxTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_FramesSinceChecks = 0;
		m_Alive = std::make_shared<int>(0);

		m_Folder = TestTempDirectory("DingoSkyboxTest");
		std::error_code error;
		std::filesystem::create_directories(m_Folder, error);
		m_HdrFile = m_Folder / "panorama.hdr";

		WritePanorama(m_HdrFile, k_Width, k_Height, &m_Source);

		m_Environment = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetDebugName("SkyboxTest panorama"));

		m_Renderer = Renderer3D::Create();
		if (m_Renderer)
		{
			m_PbrMaterial = m_Renderer->CreatePbrMaterial(PbrMaterialParams()
				.SetMaterial(MaterialParams().SetDebugName("SkyboxTest_Pbr"))
				.SetBaseColor({ 0.9f, 0.5f, 0.2f, 1.0f })
				.SetMetallic(0.0f)
				.SetRoughness(0.4f));
		}
	}

	Framebuffer* SkyboxTest::NewTarget(const char* name, bool depth)
	{
		Framebuffer* target = Framebuffer::Create(FramebufferParams()
			.SetDebugName(name)
			.SetWidth(static_cast<int32_t>(k_ShotSize))
			.SetHeight(static_cast<int32_t>(k_ShotSize))
			.SetEnableDepth(depth)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		m_Targets.push_back(target);
		return target;
	}

	glm::mat4 SkyboxTest::ShotMatrix(const glm::vec3& direction, float aspect) const
	{
		PerspectiveCamera camera(60.0f, aspect, 0.1f, 100.0f);
		camera.SetPosition(glm::vec3(0.0f));
		camera.SetUp(std::abs(direction.y) > 0.9f ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
		camera.SetTarget(direction);
		return camera.GetViewProjectionMatrix();
	}

	Renderer3D::Statistics SkyboxTest::RenderShot(Framebuffer* target, const glm::mat4& viewProjection, const SceneEnvironment* sky, const std::function<void(Renderer3D&)>& submit)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		m_Renderer->BeginScene(viewProjection);
		m_Renderer->Clear(k_Clear);
		if (submit)
			submit(*m_Renderer);
		if (sky)
			m_Renderer->SetEnvironment(*sky);
		m_Renderer->EndScene();
		Renderer::SetRenderTarget(previous);
		return m_Renderer->GetStatistics();
	}

	void SkyboxTest::ReadTargets(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done)
	{
		auto images = std::make_shared<std::vector<TexturePixels>>(targets.size());
		auto remaining = std::make_shared<size_t>(targets.size());
		const std::weak_ptr<int> alive = m_Alive;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			targets[i]->GetAttachment(0)->ReadPixels([alive, images, remaining, i, done](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				(*images)[i] = pixels;
				if (--*remaining == 0)
					done(*images);
			});
		}
	}

	void SkyboxTest::SubmitSpheres(Renderer3D& renderer)
	{
		DirectionalLight sun;
		sun.Direction = { -0.3f, -1.0f, -0.6f };
		sun.Intensity = 1.0f;
		renderer.SubmitLight(sun);
		renderer.SetAmbientLight(glm::vec3(1.0f), 0.3f);
		auto sphere = [](const glm::vec3& center) { return glm::scale(glm::translate(glm::mat4(1.0f), center), glm::vec3(1.6f)); };
		renderer.SubmitMesh(renderer.GetSphereMesh(), sphere({ -1.1f, -0.2f, -4.0f }), { 0.3f, 0.6f, 0.9f, 1.0f });
		renderer.SubmitMesh(renderer.GetSphereMesh(), sphere({ 1.1f, -0.2f, -4.0f }), { 1.0f, 1.0f, 1.0f, 1.0f }, m_PbrMaterial);
	}

	void SkyboxTest::RunChecks()
	{
		CheckRejections();
		CheckBake();
		CheckLifetime();
		CheckSky();
		CheckScene();
		CheckAssets();
	}

	void SkyboxTest::CheckScene()
	{
		if (!m_Environment || !m_Renderer || !m_Environment->IsReady())
		{
			Check(false, "Skybox 9: the renderer and the Environment are ready");
			return;
		}

		struct Case
		{
			std::string Name;
			float Expect; // linear intensity the centre pixel shows, or negative for no sky
			std::function<void(Renderer3D&)> Submit;
		};

		Environment* environment = m_Environment;
		const std::vector<float> source = m_Source;
		auto sky = [environment](float intensity) { return SkyboxComponent(environment, intensity); };
		auto withCamera = [](Scene& scene)
		{
			Entity camera = scene.CreateEntity("Camera");
			camera.AddComponent<CameraComponent>();
			return camera;
		};

		std::vector<Case> cases;
		cases.push_back({ "the camera entity's own sky beats the scene-wide one", 0.5f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(sky(1.0f));
			Entity camera = withCamera(scene);
			camera.AddComponent<SkyboxComponent>(sky(0.5f));
			scene.SubmitLights(renderer, camera);
		} });
		cases.push_back({ "a disabled camera sky falls back to the scene-wide one", 1.0f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(sky(1.0f));
			Entity camera = withCamera(scene);
			camera.AddComponent<SkyboxComponent>(sky(0.5f)).Enabled = false;
			scene.SubmitLights(renderer, camera);
		} });
		cases.push_back({ "Visible = false keeps the sky out of the frame", -1.0f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			SkyboxComponent hidden = sky(1.0f);
			hidden.Visible = false;
			scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(hidden);
			Entity camera = withCamera(scene);
			scene.SubmitLights(renderer, camera);
		} });
		cases.push_back({ "an inactive entity's sky is skipped for the next one", 1.0f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			Entity inactive = scene.CreateEntity("Inactive Sky");
			inactive.AddComponent<SkyboxComponent>(sky(0.5f));
			inactive.SetActive(false);
			scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(sky(1.0f));
			Entity camera = withCamera(scene);
			scene.SubmitLights(renderer, camera);
		} });
		cases.push_back({ "a sky on another camera's entity does not reach this view", -1.0f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			Entity other = scene.CreateEntity("Other Camera");
			other.AddComponent<CameraComponent>();
			other.AddComponent<SkyboxComponent>(sky(1.0f));
			Entity camera = withCamera(scene);
			scene.SubmitLights(renderer, camera);
		} });
		cases.push_back({ "a scene whose only sky sits on a camera draws it for a pass naming no camera", 0.25f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			Entity camera = withCamera(scene);
			camera.AddComponent<SkyboxComponent>(sky(0.25f));
			scene.SubmitLights(renderer);
		} });
		cases.push_back({ "two scene-wide skies draw the first in entity order (one warning logged)", 0.5f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			scene.CreateEntity("First").AddComponent<SkyboxComponent>(sky(0.5f));
			scene.CreateEntity("Second").AddComponent<SkyboxComponent>(sky(1.0f));
			Entity camera = withCamera(scene);
			scene.SubmitLights(renderer, camera);
		} });
		cases.push_back({ "SetEnvironment after SubmitLights overrides the component", 1.0f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			scene.CreateEntity("Sky").AddComponent<SkyboxComponent>(sky(0.5f));
			Entity camera = withCamera(scene);
			scene.SubmitLights(renderer, camera);
			SceneEnvironment direct;
			direct.Environment = environment;
			renderer.SetEnvironment(direct);
		} });
		cases.push_back({ "a SkyboxComponent without an Environment draws nothing", -1.0f, [=](Renderer3D& renderer)
		{
			Scene scene("Skybox Check");
			scene.CreateEntity("Sky").AddComponent<SkyboxComponent>();
			Entity camera = withCamera(scene);
			scene.SubmitLights(renderer, camera);
		} });

		std::vector<Framebuffer*> targets;
		auto drawn = std::make_shared<std::vector<bool>>();
		for (const Case& entry : cases)
		{
			targets.push_back(NewTarget("SkyboxTest scene sky"));
			drawn->push_back(RenderShot(targets.back(), ShotMatrix({ 0, 0, -1 }), nullptr, entry.Submit).Skybox);
		}

		auto names = std::make_shared<std::vector<Case>>(std::move(cases));
		const int centre = static_cast<int>(k_ShotSize / 2);
		ReadTargets(targets, [this, names, drawn, source, centre](const std::vector<TexturePixels>& images)
		{
			for (size_t i = 0; i < names->size(); ++i)
			{
				const Case& entry = (*names)[i];
				bool ok = !images[i].Data.empty();
				float error = 0.0f;
				if (ok && entry.Expect < 0.0f)
					ok = !(*drawn)[i] && IsClear(images[i].GetPixel(centre, centre));
				else if (ok)
				{
					error = MaxDifference(images[i].GetPixel(centre, centre), SkyReference(source, { 0, 0, -1 }, entry.Expect));
					ok = (*drawn)[i] && error < 0.05f;
				}
				Check(ok, std::format("Skybox 9: {} (sky drawn {}, error {:.3f})", entry.Name, static_cast<bool>((*drawn)[i]), error));
			}
		});
	}

	void SkyboxTest::CheckAssets()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		const std::filesystem::path reloadFile = m_Folder / "reload.hdr";
		WritePanorama(reloadFile, k_Width, k_Height, nullptr);
		m_AssetHandle = assets.Load(reloadFile);
		Environment* loaded = assets.GetEnvironment(m_AssetHandle);
		const AssetMetadata* metadata = assets.GetMetadata(m_AssetHandle);
		const bool typed = loaded && metadata && metadata->Type == AssetType::Environment && assets.Get<Environment>(m_AssetHandle) == loaded
			&& assets.IsReady(m_AssetHandle) && AssetManager::SupportsInPlaceReload(AssetType::Environment) && std::string(ToString(AssetType::Environment)) == "Environment";
		Check(typed && loaded->GetFaceSize() == 32 && !loaded->IsReady(),
			std::format("Skybox 13: AssetManager::Load of a .hdr gives an Environment (typed {}, face {})", typed, loaded ? loaded->GetFaceSize() : 0));

		bool reloadOk = false;
		if (loaded)
		{
			const bool baked = loaded->Prepare();
			const uint64_t cubeId = loaded->GetCubeMap()->GetId();
			WritePanorama(reloadFile, 256, 128, nullptr);
			const bool reloaded = assets.Reload(m_AssetHandle);
			reloadOk = baked && reloaded && assets.GetEnvironment(m_AssetHandle) == loaded && loaded->GetFaceSize() == 64
				&& loaded->GetMipLevelCount() == 7 && loaded->GetCubeMap()->GetId() == cubeId && !loaded->IsReady() && loaded->Prepare() && loaded->IsReady();
		}
		Check(reloadOk, std::format("Skybox 13: a reload keeps the Environment and its cube's id, takes the new 256 x 128 file (face 64) and bakes again ({})", reloadOk));

		const std::filesystem::path packSource = m_Folder / "pack-src";
		const std::filesystem::path packFile = m_Folder / "pack-out" / "skybox.dpak";
		std::error_code error;
		std::filesystem::create_directories(packSource, error);
		std::filesystem::create_directories(packFile.parent_path(), error);
		WritePanorama(packSource / "skybox_pack.hdr", k_Width, k_Height, nullptr);
		const PackageResult packed = AssetManager::Package(PackageParams().SetSourceDirectory(packSource).SetOutputPath(packFile));
		bool packOk = false;
		bool closedOk = false;
		if (packed.Success)
		{
			const PackageHandle package = assets.Unpackage(packFile, PackageKey());
			const AssetHandle handle = package != k_InvalidPackage ? assets.Load("skybox_pack.hdr") : k_InvalidAsset;
			Environment* fromPack = assets.GetEnvironment(handle);
			packOk = fromPack && fromPack->GetFaceSize() == 32 && assets.GetAssetSource(handle) == "skybox.dpak";
			if (package != k_InvalidPackage)
			{
				assets.ClosePackage(package);
				closedOk = !assets.IsPackageMounted(package) && !assets.GetEnvironment(handle);
			}
		}
		Check(packOk && closedOk, std::format("Skybox 13: a .hdr packed into a temporary .dpak loads as an Environment from the mounted package and is gone after ClosePackage (packed {}, loaded {}, closed {})", packed.Success, packOk, closedOk));

		WritePanorama(m_Folder / "async.hdr", k_Width, k_Height, nullptr);
		m_AsyncHandle = assets.LoadAsync(m_Folder / "async.hdr");
		m_AsyncFrames = 0;
		m_AsyncPending = IsValidAssetHandle(m_AsyncHandle);
		if (!m_AsyncPending)
			Check(false, "Skybox 13: LoadAsync of a .hdr gives a handle");
	}

	void SkyboxTest::CheckSky()
	{
		if (!m_Environment || !m_Renderer || !m_Environment->IsReady() || !m_PbrMaterial)
		{
			Check(false, "Skybox 3 to 12: the renderer, the Environment and the PBR material are ready");
			return;
		}

		const std::vector<float> source = m_Source;
		const int centre = static_cast<int>(k_ShotSize / 2);
		const glm::vec3 axes[5] = { { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 } };
		const char* names[5] = { "-Z", "+X", "-X", "+Y", "-Y" };
		SceneEnvironment sky;
		sky.Environment = m_Environment;

		std::vector<Framebuffer*> orientation;
		for (int i = 0; i < 5; ++i)
		{
			orientation.push_back(NewTarget("SkyboxTest orientation"));
			RenderShot(orientation.back(), ShotMatrix(axes[i]), &sky);
		}
		ReadTargets(orientation, [this, source, axes, names, centre](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			std::string detail;
			for (int i = 0; i < 5; ++i)
			{
				const float diff = images[i].Data.empty() ? 1.0f : MaxDifference(images[i].GetPixel(centre, centre), SkyReference(source, axes[i]));
				worst = (std::max)(worst, diff);
				detail += std::format(" {} {:.3f}", names[i], diff);
			}
			Check(worst < 0.05f, std::format("Skybox 3: the centre pixel of a view down each axis matches the panorama there (error per axis:{})", detail));
		});

		const SceneEnvironment turned{ m_Environment, 1.0f, 90.0f, true };
		const SceneEnvironment turnedBack{ m_Environment, 1.0f, -90.0f, true };
		std::vector<Framebuffer*> rotation;
		for (int i = 0; i < 3; ++i)
			rotation.push_back(NewTarget("SkyboxTest rotation"));
		RenderShot(rotation[0], ShotMatrix(axes[2]), &turned);
		RenderShot(rotation[1], ShotMatrix(axes[0]), &turned);
		RenderShot(rotation[2], ShotMatrix(axes[1]), &turnedBack);
		ReadTargets(rotation, [this, source, axes, centre](const std::vector<TexturePixels>& images)
		{
			const glm::vec3 expected[3] = { SkyReference(source, axes[0]), SkyReference(source, axes[1]), SkyReference(source, axes[0]) };
			float worst = 0.0f;
			for (int i = 0; i < 3; ++i)
				worst = (std::max)(worst, images[i].Data.empty() ? 1.0f : MaxDifference(images[i].GetPixel(centre, centre), expected[i]));
			Check(worst < 0.05f, std::format("Skybox 4: Rotation 90 shows -Z where -X looks and +X where -Z looks, and -90 shows -Z where +X looks (largest error {:.3f})", worst));
		});

		const SceneEnvironment doubled{ m_Environment, 2.0f, 0.0f, true };
		const SceneEnvironment dark{ m_Environment, 0.0f, 0.0f, true };
		std::vector<Framebuffer*> intensity = { NewTarget("SkyboxTest intensity 2"), NewTarget("SkyboxTest intensity 0") };
		RenderShot(intensity[0], ShotMatrix(axes[0]), &doubled);
		RenderShot(intensity[1], ShotMatrix(axes[0]), &dark);
		ReadTargets(intensity, [this, source, axes, centre](const std::vector<TexturePixels>& images)
		{
			const float twice = images[0].Data.empty() ? 1.0f : MaxDifference(images[0].GetPixel(centre, centre), SkyReference(source, axes[0], 2.0f));
			const float none = images[1].Data.empty() ? 1.0f : MaxDifference(images[1].GetPixel(centre, centre), glm::vec3(0.0f));
			Check(twice < 0.05f && none < 0.01f, std::format("Skybox 5: Intensity 2 doubles the linear radiance (error {:.3f}) and Intensity 0 draws black (error {:.3f})", twice, none));
		});

		auto boxScene = [](Renderer3D& renderer)
		{
			renderer.SetAmbientLight(glm::vec3(1.0f), 1.0f);
			auto box = [](const glm::vec3& center, const glm::vec3& size) { return glm::scale(glm::translate(glm::mat4(1.0f), center), size); };
			renderer.SubmitMesh(renderer.GetBoxMesh(), box({ 0.0f, 0.0f, -3.0f }, glm::vec3(1.5f)), { 1.0f, 0.0f, 0.0f, 1.0f });
			renderer.SubmitMesh(renderer.GetBoxMesh(), box({ 30.0f, 0.0f, -95.0f }, { 20.0f, 20.0f, 0.1f }), { 0.0f, 1.0f, 0.0f, 1.0f });
		};
		std::vector<Framebuffer*> opaque = { NewTarget("SkyboxTest boxes sky"), NewTarget("SkyboxTest boxes none") };
		const Renderer3D::Statistics boxStats = RenderShot(opaque[0], ShotMatrix(axes[0]), &sky, boxScene);
		RenderShot(opaque[1], ShotMatrix(axes[0]), nullptr, boxScene);
		ReadTargets(opaque, [this, boxStats](const std::vector<TexturePixels>& images)
		{
			uint32_t covered = 0;
			uint32_t changed = 0;
			uint32_t unfilled = 0;
			if (!images[0].Data.empty() && !images[1].Data.empty())
			{
				for (uint32_t y = 0; y < k_ShotSize; ++y)
				{
					for (uint32_t x = 0; x < k_ShotSize; ++x)
					{
						const glm::vec4 with = images[0].GetPixel(x, y);
						const glm::vec4 without = images[1].GetPixel(x, y);
						if (IsClear(without))
							unfilled += IsClear(with) ? 1 : 0;
						else
						{
							++covered;
							changed += SamePixel(with, without) ? 0 : 1;
						}
					}
				}
			}
			const glm::vec4 quad = images[0].Data.empty() ? glm::vec4(0.0f) : images[0].GetPixel(25, 16);
			Check(boxStats.Skybox && covered > 100 && changed == 0 && unfilled == 0 && quad.g > 0.5f && quad.r < 0.2f,
				std::format("Skybox 7a: a box and a far quad draw the same with the sky ({} mesh pixels, {} changed), the sky fills the rest ({} left unfilled), the far quad stays green ({:.2f})", covered, changed, unfilled, quad.g));
		});

		std::vector<Framebuffer*> meshes = { NewTarget("SkyboxTest spheres sky"), NewTarget("SkyboxTest spheres none") };
		auto sphereScene = [this](Renderer3D& renderer) { SubmitSpheres(renderer); };
		SceneEnvironment unlitSky = sky;
		unlitSky.Lighting = false;
		const Renderer3D::Statistics sphereStats = RenderShot(meshes[0], ShotMatrix(axes[0]), &unlitSky, sphereScene);
		RenderShot(meshes[1], ShotMatrix(axes[0]), nullptr, sphereScene);
		ReadTargets(meshes, [this, sphereStats](const std::vector<TexturePixels>& images)
		{
			uint32_t covered = 0;
			uint32_t changed = 0;
			if (!images[0].Data.empty() && !images[1].Data.empty())
			{
				for (uint32_t y = 0; y < k_ShotSize; ++y)
				{
					for (uint32_t x = 0; x < k_ShotSize; ++x)
					{
						const glm::vec4 without = images[1].GetPixel(x, y);
						if (IsClear(without))
							continue;
						++covered;
						changed += SamePixel(images[0].GetPixel(x, y), without) ? 0 : 1;
					}
				}
			}
			Check(sphereStats.Skybox && covered > 200 && changed == 0, std::format("Skybox 7: a lit and a PBR sphere draw the same with the sky when its Lighting is off ({} mesh pixels, {} changed, sky drawn {})", covered, changed, sphereStats.Skybox));
		});

		{
			Framebuffer* target = NewTarget("SkyboxTest post");
			const glm::vec3 sunDirection = glm::normalize(glm::vec3(0.3f, 0.6f, -0.5f));
			PerspectiveCamera camera(60.0f, 1.0f, 0.1f, 100.0f);
			camera.SetPosition(glm::vec3(0.0f));
			camera.SetTarget(sunDirection);
			PostProcessSettings settings;
			settings.Enabled = true;
			settings.Tone.Operator = ToneMapOperator::None;
			settings.Tone.Exposure = -3.0f;
			PostProcessStack& post = Renderer::GetPostProcessStack();
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(target);
			post.Begin(settings, camera.GetProjectionMatrix());
			m_Renderer->BeginScene(camera);
			m_Renderer->Clear(k_Clear);
			m_Renderer->SetEnvironment(sky);
			m_Renderer->EndScene();
			post.End();
			Renderer::SetRenderTarget(previous);
			const bool drawn = m_Renderer->GetStatistics().Skybox;
			ReadTargets({ target }, [this, source, sunDirection, centre, drawn](const std::vector<TexturePixels>& images)
			{
				const glm::vec3 expected = SrgbEncode(SampleSource(source, GlslEnvironment::DE_EquirectUv(sunDirection))) * 0.125f;
				const glm::vec4 pixel = images[0].Data.empty() ? glm::vec4(0.0f) : images[0].GetPixel(centre, centre);
				const float diff = MaxDifference(pixel, expected);
				Check(drawn && diff < 0.1f && expected.r > 0.3f && expected.r < 0.8f && pixel.r < 0.97f,
					std::format("Skybox 8: through the post chain at -3 EV the sun reads an eighth of its encoded value: {:.2f} against {:.2f} (error {:.3f})", pixel.r, expected.r, diff));
			});
		}

		{
			m_Second = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetFaceSize(16).SetDebugName("SkyboxTest second"));
			std::vector<Framebuffer*> frame = { NewTarget("SkyboxTest scene 1"), NewTarget("SkyboxTest scene 2"), NewTarget("SkyboxTest scene 3") };
			const SceneEnvironment first{ m_Environment, 1.0f, 0.0f, true };
			const SceneEnvironment other{ m_Second, 2.0f, 0.0f, true };
			const SceneEnvironment rotated{ m_Environment, 1.0f, 90.0f, true };
			const Renderer3D::Statistics one = RenderShot(frame[0], ShotMatrix(axes[0]), &first);
			const Renderer3D::Statistics two = RenderShot(frame[1], ShotMatrix(axes[0]), &other);
			const Renderer3D::Statistics three = RenderShot(frame[2], ShotMatrix(axes[0]), &rotated);
			ReadTargets(frame, [this, source, axes, centre, one, two, three](const std::vector<TexturePixels>& images)
			{
				const float a = images[0].Data.empty() ? 1.0f : MaxDifference(images[0].GetPixel(centre, centre), SkyReference(source, axes[0]));
				const float b = images[1].Data.empty() ? 1.0f : MaxDifference(images[1].GetPixel(centre, centre), SkyReference(source, axes[0], 2.0f));
				const float c = images[2].Data.empty() ? 1.0f : MaxDifference(images[2].GetPixel(centre, centre), SkyReference(source, axes[1]));
				Check(one.Skybox && two.Skybox && three.Skybox && two.EnvironmentBakes == 1 && a < 0.05f && b < 0.07f && c < 0.05f,
					std::format("Skybox 10: three scenes of one frame draw their own sky: the first environment (error {:.3f}), a second one baked in its scene at intensity 2 ({:.3f}, {} bakes) and the first turned 90 ({:.3f})", a, b, two.EnvironmentBakes, c));
			});
		}

		{
			Fog fog;
			fog.Mode = FogMode::Linear;
			fog.Color = { 1.0f, 0.0f, 0.0f };
			fog.Start = 1.0f;
			fog.End = 5.0f;
			fog.MaxOpacity = 1.0f;
			auto lone = [](Renderer3D& renderer)
			{
				renderer.SetAmbientLight(glm::vec3(1.0f), 1.0f);
				renderer.SubmitMesh(renderer.GetBoxMesh(), glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -3.0f }), glm::vec3(1.5f)), { 0.2f, 0.9f, 0.2f, 1.0f });
			};
			std::vector<Framebuffer*> fogged = { NewTarget("SkyboxTest unfogged"), NewTarget("SkyboxTest fogged") };
			RenderShot(fogged[0], ShotMatrix(axes[0]), &sky, lone);
			const Renderer3D::Statistics stats = RenderShot(fogged[1], ShotMatrix(axes[0]), &sky, [&](Renderer3D& renderer) { lone(renderer); renderer.SetFog(fog); });
			ReadTargets(fogged, [this, stats, centre](const std::vector<TexturePixels>& images)
			{
				bool sameSky = !images[0].Data.empty() && !images[1].Data.empty();
				const uint32_t corners[4][2] = { { 1, 1 }, { 30, 1 }, { 1, 30 }, { 30, 30 } };
				for (const auto& corner : corners)
					sameSky = sameSky && SamePixel(images[0].GetPixel(corner[0], corner[1]), images[1].GetPixel(corner[0], corner[1]));
				const bool boxFogged = !images[0].Data.empty() && !images[1].Data.empty() && !SamePixel(images[0].GetPixel(centre, centre), images[1].GetPixel(centre, centre));
				Check(stats.Fogged && stats.Skybox && sameSky && boxFogged,
					std::format("Skybox 11: a fogged scene leaves its sky pixels unchanged (fogged {}, sky drawn {}, corners equal {}) while the box is fogged ({})", stats.Fogged, stats.Skybox, sameSky, boxFogged));
			});
		}

		{
			Framebuffer* ortho = NewTarget("SkyboxTest orthographic");
			Framebuffer* hidden = NewTarget("SkyboxTest hidden");
			Framebuffer* bad = NewTarget("SkyboxTest invalid");
			const Renderer3D::Statistics orthoStats = RenderShot(ortho, glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f), &sky);
			SceneEnvironment noSky = sky;
			noSky.DrawSkybox = false;
			const Renderer3D::Statistics hiddenStats = RenderShot(hidden, ShotMatrix(axes[0]), &noSky);
			bool rejected = false;
			const Renderer3D::Statistics badStats = RenderShot(bad, ShotMatrix(axes[0]), nullptr, [&](Renderer3D& renderer)
			{
				SceneEnvironment negative = sky;
				negative.Intensity = -1.0f;
				SceneEnvironment infinite = sky;
				infinite.Rotation = std::numeric_limits<float>::infinity();
				rejected = !renderer.SetEnvironment(negative) && !renderer.SetEnvironment(infinite);
			});
			Framebuffer* flat = NewTarget("SkyboxTest no depth", false);
			const Renderer3D::Statistics flatStats = RenderShot(flat, ShotMatrix(axes[0]), &sky);
			ReadTargets({ flat }, [this, source, axes, flatStats, centre](const std::vector<TexturePixels>& images)
			{
				const float error = images[0].Data.empty() ? 1.0f : MaxDifference(images[0].GetPixel(centre, centre), SkyReference(source, axes[0]));
				Check(flatStats.Skybox && error < 0.05f, std::format("Skybox 12b: a target without depth still gets the sky, drawn first (sky drawn {}, error {:.3f})", flatStats.Skybox, error));
			});
			ReadTargets({ ortho, hidden, bad }, [this, orthoStats, hiddenStats, badStats, rejected, centre](const std::vector<TexturePixels>& images)
			{
				bool allClear = true;
				for (const TexturePixels& image : images)
					allClear = allClear && !image.Data.empty() && IsClear(image.GetPixel(centre, centre));
				Check(!orthoStats.Skybox && !hiddenStats.Skybox && !badStats.Skybox && rejected && allClear,
					std::format("Skybox 12: no sky for an orthographic camera ({}), DrawSkybox false ({}) or an invalid environment ({}, rejected {}); all three frames stay the clear colour ({})", orthoStats.Skybox, hiddenStats.Skybox, badStats.Skybox, rejected, allClear));
			});
		}
	}

	void SkyboxTest::DrawView()
	{
		if (!m_Renderer || !m_Environment || !m_PbrMaterial || !m_Environment->IsReady())
			return;

		Framebuffer* output = Renderer::GetRenderTarget();
		const float halfWidth = static_cast<float>(output ? output->GetWidth() : m_Width) * 0.5f;
		const float halfHeight = static_cast<float>(output ? output->GetHeight() : m_Height) * 0.5f;
		SceneEnvironment sky;
		sky.Environment = m_Environment;

		auto draw = [&](const Viewport& viewport, const glm::vec3& direction, const SceneEnvironment& environment, bool spheres)
		{
			Renderer::SetViewport(viewport);
			m_Renderer->BeginScene(ShotMatrix(direction, viewport.Width / viewport.Height));
			if (spheres)
				SubmitSpheres(*m_Renderer);
			m_Renderer->SetEnvironment(environment);
			m_Renderer->EndScene();
		};

		draw({ 0.0f, 0.0f, halfWidth, halfHeight }, glm::vec3(0.0f, 0.1f, -1.0f), sky, true);

		const glm::vec3 axes[6] = { { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 } };
		const float cellWidth = halfWidth / 3.0f;
		for (int i = 0; i < 6; ++i)
		{
			const Viewport cell{ halfWidth + static_cast<float>(i % 3) * cellWidth, static_cast<float>(i / 3) * halfHeight * 0.5f, cellWidth, halfHeight * 0.5f };
			draw(cell, axes[i], sky, false);
		}

		SceneEnvironment turned = sky;
		turned.Rotation = 90.0f;
		draw({ 0.0f, halfHeight, halfWidth, halfHeight }, glm::vec3(0.0f, 0.1f, -1.0f), turned, true);

		{
			const Viewport viewport{ halfWidth, halfHeight, halfWidth, halfHeight };
			const glm::vec3 direction = glm::normalize(glm::vec3(0.3f, 0.45f, -0.5f));
			PostProcessSettings settings;
			settings.Enabled = true;
			settings.Tone.Operator = ToneMapOperator::Neutral;
			settings.Tone.Exposure = -1.0f;
			settings.Bloom.Enabled = true;
			Renderer::SetViewport(viewport);
			PostProcessStack& post = Renderer::GetPostProcessStack();
			post.Begin(settings);
			m_Renderer->BeginScene(ShotMatrix(direction, viewport.Width / viewport.Height));
			m_Renderer->Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			SubmitSpheres(*m_Renderer);
			m_Renderer->SetEnvironment(sky);
			m_Renderer->EndScene();
			post.End();
		}
		Renderer::ResetViewport();
	}

	void SkyboxTest::CheckBake()
	{
		if (!m_Environment || m_Environment->GetFaceSize() != k_FaceSize || m_Environment->GetMipLevelCount() != k_Mips)
		{
			Check(false, "Skybox 1: the 128 x 64 panorama loads as an Environment with a 32 texel face and 6 mips");
			Check(false, "Skybox 2: every mip is the 2 x 2 average of the mip above (no Environment)");
			return;
		}
		if (m_Environment->IsReady() || !m_Environment->Prepare() || !m_Environment->IsReady() || m_Environment->GetCubeMap() == nullptr)
		{
			Check(false, "Skybox 1: Prepare bakes the Environment inside a frame");
			Check(false, "Skybox 2: every mip is the 2 x 2 average of the mip above (no bake)");
			return;
		}

		const std::weak_ptr<int> alive = m_Alive;
		auto readback = std::make_shared<CubeReadback>();
		readback->Remaining = k_Mips * 6;
		for (uint32_t mip = 0; mip < k_Mips; ++mip)
		{
			for (uint32_t face = 0; face < 6; ++face)
			{
				m_Environment->GetCubeMap()->ReadSubresource(mip, face, [this, alive, readback, mip, face](const TexturePixels& pixels)
				{
					if (alive.expired())
						return;
					const uint32_t size = (std::max)(k_FaceSize >> mip, 1u);
					std::vector<glm::vec3>& target = readback->Pixels[mip * 6 + face];
					if (pixels.Data.empty() || pixels.Width != size || pixels.Height != size)
						++readback->Empty;
					else
					{
						target.reserve(static_cast<size_t>(size) * size);
						for (uint32_t y = 0; y < size; ++y)
						{
							for (uint32_t x = 0; x < size; ++x)
								target.push_back(glm::vec3(pixels.GetPixel(x, y)));
						}
					}
					if (--readback->Remaining != 0)
						return;

					uint32_t wrongBake = 0;
					float peak = 0.0f;
					for (uint32_t f = 0; f < 6; ++f)
					{
						const std::vector<glm::vec3>& baked = readback->Pixels[f];
						if (baked.size() != static_cast<size_t>(k_FaceSize) * k_FaceSize)
						{
							wrongBake += k_FaceSize * k_FaceSize;
							continue;
						}
						for (uint32_t y = 0; y < k_FaceSize; ++y)
						{
							for (uint32_t x = 0; x < k_FaceSize; ++x)
							{
								const glm::vec3 got = baked[static_cast<size_t>(y) * k_FaceSize + x];
								peak = (std::max)(peak, got.r);
								wrongBake += Close(got, BakedReference(m_Source, f, x, y), 0.02f) ? 0 : 1;
							}
						}
					}
					Check(readback->Empty == 0 && wrongBake == 0 && peak > 5.0f,
						std::format("Skybox 1: mip 0 of all six faces matches the CPU resample of the panorama ({} of {} texels wrong, brightest {:.1f}, {} empty readbacks)", wrongBake, 6 * k_FaceSize * k_FaceSize, peak, readback->Empty));

					uint32_t wrongMip = 0;
					uint32_t total = 0;
					for (uint32_t m = 1; m < k_Mips; ++m)
					{
						const uint32_t dst = (std::max)(k_FaceSize >> m, 1u);
						const uint32_t src = (std::max)(k_FaceSize >> (m - 1), 1u);
						for (uint32_t f = 0; f < 6; ++f)
						{
							const std::vector<glm::vec3>& above = readback->Pixels[(m - 1) * 6 + f];
							const std::vector<glm::vec3>& mine = readback->Pixels[m * 6 + f];
							for (uint32_t y = 0; y < dst; ++y)
							{
								for (uint32_t x = 0; x < dst; ++x)
								{
									++total;
									if (above.size() != static_cast<size_t>(src) * src || mine.size() != static_cast<size_t>(dst) * dst)
									{
										++wrongMip;
										continue;
									}
									auto at = [&](uint32_t sx, uint32_t sy) { return above[static_cast<size_t>(sy) * src + sx]; };
									const glm::vec3 expected = (at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1)) * 0.25f;
									wrongMip += Close(mine[static_cast<size_t>(y) * dst + x], expected, 0.01f) ? 0 : 1;
								}
							}
						}
					}
					Check(wrongMip == 0 && readback->Empty == 0, std::format("Skybox 2: every mip of every face is the 2 x 2 average of the mip above, down to 1 x 1 ({} of {} texels wrong)", wrongMip, total));
				});
			}
		}
	}

	void SkyboxTest::CheckLifetime()
	{
		Environment* unbaked = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetFaceSize(16).SetDebugName("SkyboxTest unbaked"));
		const bool unbakedOk = unbaked && !unbaked->IsReady() && unbaked->GetFaceSize() == 16 && unbaked->GetMipLevelCount() == 5;
		delete unbaked;

		Environment* sameFrame = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetFaceSize(64).SetDebugName("SkyboxTest same frame"));
		const bool bakedOk = sameFrame && sameFrame->Prepare() && sameFrame->IsReady();
		delete sameFrame;

		m_LateDelete = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetDebugName("SkyboxTest late"));
		const bool lateOk = m_LateDelete && m_LateDelete->Prepare();
		const bool idsOk = m_LateDelete && m_Environment && m_LateDelete->GetId() != m_Environment->GetId() && m_Environment->GetId() != 0;

		Check(unbakedOk && bakedOk && lateOk && idsOk,
			std::format("Skybox 14: an Environment deleted before its bake, one deleted in the frame that baked it and one deleted a frame later (read the log for validation errors; unbaked {}, same frame {}, late {}, ids {})", unbakedOk, bakedOk, lateOk, idsOk));
	}

	void SkyboxTest::CheckRejections()
	{
		const std::filesystem::path notHdr = m_Folder / "panorama.png";
		const std::filesystem::path junk = m_Folder / "junk.hdr";
		{
			std::ofstream out(notHdr, std::ios::binary);
			out << "not an image";
			std::ofstream junkOut(junk, std::ios::binary);
			junkOut << "not a radiance file";
		}

		Environment* wrongExtension = Environment::Create(EnvironmentParams().SetPath(notHdr));
		Environment* junkFile = Environment::Create(EnvironmentParams().SetPath(junk));
		Environment* missing = Environment::Create(EnvironmentParams().SetPath(m_Folder / "missing.hdr"));
		const uint32_t badSizes[] = { 8, 100, 4096, 24 };
		uint32_t accepted = 0;
		for (uint32_t size : badSizes)
		{
			Environment* environment = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetFaceSize(size));
			accepted += environment ? 1 : 0;
			delete environment;
		}
		Environment* edgeLow = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetFaceSize(16));
		Environment* edgeHigh = Environment::Create(EnvironmentParams().SetPath(m_HdrFile).SetFaceSize(1024));
		const bool edgesOk = edgeLow && edgeHigh && edgeLow->GetFaceSize() == 16 && edgeHigh->GetFaceSize() == 1024;

		Check(!wrongExtension && !junkFile && !missing && accepted == 0 && edgesOk,
			std::format("Skybox 15: Create returns nullptr, with an error, for a .png, a .hdr that is not Radiance, a missing file and sizes 8, 100, 4096 and 24 (accepted {}), and takes 16 and 1024 (expect 7 errors logged)", accepted));

		delete wrongExtension;
		delete junkFile;
		delete missing;
		delete edgeLow;
		delete edgeHigh;
	}

	void SkyboxTest::Update(float deltaTime)
	{
		if (!m_ChecksDone && !Renderer::IsFrameSkipped())
		{
			m_ChecksDone = true;
			RunChecks();
		}
		if (m_AsyncPending)
		{
			AssetManager& assets = Application::Get().GetAssetManager();
			Environment* loaded = assets.GetEnvironment(m_AsyncHandle);
			if (loaded || ++m_AsyncFrames > 600)
			{
				m_AsyncPending = false;
				Check(loaded && loaded->GetFaceSize() == 32 && assets.IsReady(m_AsyncHandle),
					std::format("Skybox 13: LoadAsync decodes a .hdr on the loader thread and publishes an Environment ({} frames)", m_AsyncFrames));
			}
		}

		if (m_ChecksDone && m_LateDelete && ++m_FramesSinceChecks >= 3)
		{
			delete m_LateDelete;
			m_LateDelete = nullptr;
		}

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.EndScene();
		DrawView();
	}

	void SkyboxTest::Cleanup()
	{
		m_Alive.reset();
		AssetManager& assets = Application::Get().GetAssetManager();
		if (IsValidAssetHandle(m_AssetHandle))
			assets.Remove(m_AssetHandle);
		if (IsValidAssetHandle(m_AsyncHandle))
			assets.Remove(m_AsyncHandle);
		m_AssetHandle = k_InvalidAsset;
		m_AsyncHandle = k_InvalidAsset;
		m_AsyncPending = false;
		delete m_LateDelete;
		m_LateDelete = nullptr;
		for (Framebuffer*& target : m_Targets)
			DestroyAndDelete(target);
		m_Targets.clear();
		DestroyAndDelete(m_PbrMaterial);
		delete m_Second;
		m_Second = nullptr;
		if (m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
		delete m_Environment;
		m_Environment = nullptr;
		std::error_code error;
		std::filesystem::remove_all(m_Folder, error);
		m_Checks.clear();
	}

	void SkyboxTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Width = width;
		m_Height = height;
	}

	void SkyboxTest::ImGuiRender()
	{
		ImGui::TextWrapped("Top left: the sky with a lit and a PBR sphere. Top right: the six axes. Bottom left: turned 90 degrees. Bottom right: through the post chain at -1 EV with bloom.");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
