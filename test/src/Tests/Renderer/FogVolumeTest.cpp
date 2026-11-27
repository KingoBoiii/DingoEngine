#include "FogVolumeTest.h"

#include "DingoEngine/Graphics/LightMath.h"
#include "DingoEngine/Graphics/Renderer3DAccess.h"
#include "DingoEngine/Graphics/VolumeNoise.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_FramesBeforeChecks = 10;
		constexpr float k_Fov = 60.0f;
		constexpr float k_Aspect = 32.0f / 18.0f;
		const glm::vec3 k_Eye(0.0f, 1.0f, 0.0f);
		const glm::vec3 k_Forward(0.0f, 0.0f, -1.0f);
		constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();
		constexpr float k_Infinity = std::numeric_limits<float>::infinity();

		using Access = Internal::Renderer3DAccess;
		using Record = std::remove_cv_t<std::remove_reference_t<decltype(Access::FogVolumeRecords(std::declval<const Renderer3D&>()).Volumes[0])>>;

		FogVolume Volume(float id)
		{
			FogVolume volume;
			volume.HalfExtents = glm::vec3(0.5f);
			volume.Density = id;
			return volume;
		}

		glm::mat4 At(float x, float y, float z)
		{
			return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
		}

		std::vector<int> KeptIds(const Renderer3D& renderer)
		{
			const auto& data = Access::FogVolumeRecords(renderer);
			const uint32_t count = Access::FogVolumeFroxelCount(renderer) + Access::FogVolumeRaymarchCount(renderer);
			std::vector<int> ids;
			for (uint32_t slot = 0; slot < count; ++slot)
				ids.push_back(static_cast<int>(data.Volumes[slot].Medium.x + 0.5f));
			return ids;
		}

		std::string Describe(const std::vector<int>& ids)
		{
			std::string text;
			for (int id : ids)
				text += (text.empty() ? "" : ", ") + std::to_string(id);
			return "[" + text + "]";
		}

		bool SameRecords(const Renderer3D& a, const Renderer3D& b, uint32_t count)
		{
			const auto& left = Access::FogVolumeRecords(a);
			const auto& right = Access::FogVolumeRecords(b);
			return std::memcmp(left.Volumes, right.Volumes, sizeof(Record) * count) == 0;
		}

		FogVolume SceneVolume()
		{
			FogVolume volume;
			volume.Shape = FogVolumeShape::Sphere;
			volume.HalfExtents = glm::vec3(2.0f, 1.5f, 3.0f);
			volume.Density = 0.3f;
			volume.Albedo = glm::vec3(0.9f, 0.8f, 0.7f);
			volume.Anisotropy = 0.4f;
			volume.ScatterTint = glm::vec3(1.0f, 0.9f, 1.2f);
			volume.OutsideShadowLight = 0.5f;
			volume.AmbientIntensity = 0.7f;
			volume.LightIntensity = 1.3f;
			volume.LocalLightIntensity = 0.8f;
			volume.EdgeFade = 0.75f;
			volume.HeightFadeStart = 0.5f;
			volume.HeightFadeEnd = 2.5f;
			volume.HeightBandBottom = 0.25f;
			volume.HeightBandTop = 1.5f;
			volume.HeightBandMultiplier = 2.0f;
			volume.NearFadeStart = 1.0f;
			volume.NearFadeEnd = 3.0f;
			volume.NearFadeKeep = 0.25f;
			volume.FarFadeStart = 20.0f;
			volume.FarFadeEnd = 40.0f;
			volume.FarFadeKeep = 0.5f;
			volume.NoiseScale = 3.0f;
			volume.NoiseVelocity = glm::vec3(0.5f, 0.0f, -0.25f);
			volume.NoiseMin = 0.25f;
			volume.NoiseMax = 1.0f;
			return volume;
		}

		constexpr glm::uvec3 k_Grid{ 32, 18, 16 };
		constexpr uint32_t k_DriftFrames = 40;
		constexpr double k_DriftTick = 1.0 / 30.0;
		constexpr float k_DriftTolerance = 2.0f / 255.0f;

		using Scene = FogVolumeTest::Scene;
		using Placed = FogVolumeTest::Placed;
		using Sun = FogVolumeTest::Sun;

		float SliceDepth(const VolumetricFog& fog, float boundary)
		{
			return fog.NearDistance * std::pow(fog.MaxDistance / fog.NearDistance, boundary / static_cast<float>(k_Grid.z));
		}

		glm::vec3 FroxelRay(uint32_t x, uint32_t y)
		{
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(k_Grid.x);
			const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(k_Grid.y);
			const float tanHalf = std::tan(glm::radians(k_Fov * 0.5f));
			return glm::vec3((2.0f * u - 1.0f) * tanHalf * k_Aspect, (1.0f - 2.0f * v) * tanHalf, -1.0f);
		}

		struct Sample
		{
			glm::vec3 World{ 0.0f };
			glm::vec3 Direction{ 0.0f };
			float Depth = 0.0f;
			float Distance = 0.0f;
			uint32_t X = 0;
			uint32_t Y = 0;
		};

		Sample SampleAt(const VolumetricFog& fog, uint32_t x, uint32_t y, uint32_t slice)
		{
			Sample sample;
			const glm::vec3 ray = FroxelRay(x, y);
			sample.Depth = SliceDepth(fog, static_cast<float>(slice) + 0.5f);
			sample.World = k_Eye + ray * sample.Depth;
			sample.Direction = glm::normalize(ray);
			sample.Distance = glm::length(ray) * sample.Depth;
			sample.X = x;
			sample.Y = y;
			return sample;
		}

		float HenyeyGreenstein(float cosTheta, float g)
		{
			const float g2 = g * g;
			const float denominator = 1.0f + g2 - 2.0f * g * cosTheta;
			return (1.0f - g2) / (4.0f * glm::pi<float>() * denominator * std::sqrt(denominator));
		}

		float EdgeFade(float distanceToSurface, float fade)
		{
			if (fade <= 0.0f)
				return distanceToSurface > 0.0f ? 1.0f : 0.0f;
			const float s = glm::clamp(distanceToSurface / fade, 0.0f, 1.0f);
			return s * s * (3.0f - 2.0f * s);
		}

		float HeightProfile(float h, const FogVolume& v)
		{
			float profile = 1.0f;
			if (v.HeightFadeEnd > v.HeightFadeStart)
				profile = 1.0f - glm::clamp((h - v.HeightFadeStart) / (v.HeightFadeEnd - v.HeightFadeStart), 0.0f, 1.0f);
			if (v.HeightBandMultiplier != 1.0f && v.HeightBandTop > v.HeightBandBottom)
			{
				const float soft = (std::max)(v.HeightBandSoftness, 1e-4f);
				const float rise = glm::clamp((h - v.HeightBandBottom) / soft, 0.0f, 1.0f);
				const float fall = glm::clamp((v.HeightBandTop - h) / soft, 0.0f, 1.0f);
				const float edge = (std::min)(rise * rise * (3.0f - 2.0f * rise), fall * fall * (3.0f - 2.0f * fall));
				profile *= 1.0f + (v.HeightBandMultiplier - 1.0f) * edge;
			}
			return profile;
		}

		float DistanceFade(float t, float start, float end, float keepBefore, float keepAfter)
		{
			if (end <= start)
				return 1.0f;
			const float s = glm::clamp((t - start) / (end - start), 0.0f, 1.0f);
			return keepBefore + (keepAfter - keepBefore) * s;
		}

		const std::vector<uint8_t>& NoiseTexels()
		{
			static const std::vector<uint8_t> texels = Internal::GenerateVolumeNoise();
			return texels;
		}

		float SampleNoise(const glm::vec3& uvw)
		{
			constexpr int size = static_cast<int>(Internal::k_VolumeNoiseSize);
			const std::vector<uint8_t>& texels = NoiseTexels();
			const glm::vec3 t = uvw * static_cast<float>(size) - glm::vec3(0.5f);
			const glm::vec3 base = glm::floor(t);
			const glm::vec3 f = t - base;
			auto texel = [&](int x, int y, int z)
			{
				auto wrap = [](int v) { return ((v % size) + size) % size; };
				return static_cast<float>(texels[(static_cast<size_t>(wrap(z)) * size + wrap(y)) * size + wrap(x)]) / 255.0f;
			};
			float value = 0.0f;
			for (int dz = 0; dz < 2; ++dz)
			{
				for (int dy = 0; dy < 2; ++dy)
				{
					for (int dx = 0; dx < 2; ++dx)
					{
						const float weight = (dx ? f.x : 1.0f - f.x) * (dy ? f.y : 1.0f - f.y) * (dz ? f.z : 1.0f - f.z);
						value += weight * texel(static_cast<int>(base.x) + dx, static_cast<int>(base.y) + dy, static_cast<int>(base.z) + dz);
					}
				}
			}
			return value;
		}

		glm::vec3 NoisePhase(const FogVolume& v, double time)
		{
			const glm::dvec3 phase = glm::dvec3(v.NoiseVelocity) * time / static_cast<double>(v.NoiseScale);
			return glm::vec3(static_cast<float>(phase.x - std::floor(phase.x)), static_cast<float>(phase.y - std::floor(phase.y)), static_cast<float>(phase.z - std::floor(phase.z)));
		}

		struct Medium
		{
			float SigmaT = 0.0f;
			float Weight = 1.0f;
		};

		Medium VolumeMedium(const Placed& placed, const Sample& sample, double time)
		{
			const FogVolume& v = placed.Settings;
			const glm::vec3 worldHalf(v.HalfExtents.x * glm::length(glm::vec3(placed.World[0])), v.HalfExtents.y * glm::length(glm::vec3(placed.World[1])), v.HalfExtents.z * glm::length(glm::vec3(placed.World[2])));
			const glm::vec3 local = glm::vec3(glm::inverse(placed.World * glm::scale(glm::mat4(1.0f), v.HalfExtents)) * glm::vec4(sample.World, 1.0f));

			float distance;
			if (v.Shape == FogVolumeShape::Sphere)
				distance = (1.0f - glm::length(local)) * (std::min)({ worldHalf.x, worldHalf.y, worldHalf.z });
			else
				distance = (std::min)({ (1.0f - std::abs(local.x)) * worldHalf.x, (1.0f - std::abs(local.y)) * worldHalf.y, (1.0f - std::abs(local.z)) * worldHalf.z });
			const float edge = EdgeFade(distance, v.EdgeFade);

			Medium medium;
			if (edge <= 0.0f)
				return medium;
			float density = v.Density * edge * HeightProfile((local.y + 1.0f) * worldHalf.y, v);
			if (v.NoiseMin != v.NoiseMax)
			{
				const float n = SampleNoise(sample.World / v.NoiseScale - NoisePhase(v, time));
				density *= v.NoiseMin + (v.NoiseMax - v.NoiseMin) * n;
			}
			medium.SigmaT = density;
			medium.Weight = DistanceFade(sample.Distance, v.NearFadeStart, v.NearFadeEnd, v.NearFadeKeep, 1.0f) * DistanceFade(sample.Distance, v.FarFadeStart, v.FarFadeEnd, 1.0f, v.FarFadeKeep);
			return medium;
		}

		glm::vec4 ExpectedScatter(const Scene& scene, const Sample& sample, float sunScale = 1.0f)
		{
			const VolumetricFog& fog = scene.Fog;
			const float sigmaGlobal = fog.Density * std::exp(-fog.HeightFalloff * (std::max)(sample.World.y - fog.BaseHeight, 0.0f));
			float sigmaT = sigmaGlobal;
			float weightedG = fog.Anisotropy * sigmaGlobal;
			const glm::vec3 base = fog.Albedo * sigmaGlobal * fog.ScatterTint;
			glm::vec3 ambient = base * fog.AmbientIntensity;
			glm::vec3 sun = base * fog.LightIntensity;
			glm::vec3 localCoefficient = base * fog.LocalLightIntensity;

			for (const Placed& placed : scene.Volumes)
			{
				const Medium medium = VolumeMedium(placed, sample, scene.Time);
				if (medium.SigmaT <= 0.0f)
					continue;
				const FogVolume& v = placed.Settings;
				const glm::vec3 coefficient = v.Albedo * medium.SigmaT * v.ScatterTint * medium.Weight;
				sigmaT += medium.SigmaT;
				weightedG += v.Anisotropy * medium.SigmaT;
				ambient += coefficient * v.AmbientIntensity;
				sun += coefficient * v.LightIntensity;
				localCoefficient += coefficient * (v.LocalLightIntensity * fog.LocalLightIntensity);
			}

			const float g = sigmaT > 1e-6f ? weightedG / sigmaT : fog.Anisotropy;
			glm::vec3 light = glm::vec3(scene.Ambient) * ambient;
			for (const Sun& s : scene.Suns)
			{
				const float phase = glm::pi<float>() * HenyeyGreenstein(glm::dot(glm::normalize(s.Direction), -sample.Direction), g);
				light += phase * s.Color * sun * (sunScale * s.VolumetricIntensity);
			}

			glm::vec3 lamps(0.0f);
			auto addLamp = [&](const glm::vec3& position, const glm::vec3& color, float range, const Internal::LightCone& cone, float scatter, float visibility)
			{
				const glm::vec3 offset = sample.World - position;
				const float distanceSquared = glm::dot(offset, offset);
				if (!(distanceSquared < range * range))
					return;
				const glm::vec3 fromLight = offset / std::sqrt(distanceSquared);
				const float falloff = 1.0f - distanceSquared / (range * range);
				const float coneFactor = glm::clamp(glm::dot(fromLight, cone.Axis) * cone.Scale + cone.Offset, 0.0f, 1.0f);
				const float phase = glm::pi<float>() * HenyeyGreenstein(glm::dot(fromLight, -sample.Direction), g);
				lamps += phase * color * (falloff * falloff) * (coneFactor * coneFactor) * scatter * visibility;
			};
			for (const PointLight& point : scene.Points)
				addLamp(point.Position, point.Color * point.Intensity, point.Range, Internal::GetLightCone(point), point.VolumetricIntensity, 1.0f);
			for (const SpotLight& spot : scene.Spots)
				addLamp(spot.Position, spot.Color * spot.Intensity, spot.Range, Internal::GetLightCone(spot), spot.VolumetricIntensity, 1.0f);
			light += lamps * localCoefficient;
			return glm::vec4(light, sigmaT);
		}

		bool IsSlice(const TexturePixels& image)
		{
			return !image.Data.empty() && image.Width == k_Grid.x && image.Height == k_Grid.y;
		}

		float Error(float got, float want, float floor)
		{
			const float value = std::abs(got - want) / (std::max)(std::abs(want), floor);
			return std::isfinite(value) ? value : 1e9f;
		}

		struct Comparison
		{
			float Alpha = 0.0f;
			float Rgb = 0.0f;
			float MinAlpha = 1e9f;
			float MaxAlpha = -1e9f;
			float MinRed = 1e9f;
			float MaxRed = -1e9f;
			uint32_t Zero = 0;
			uint32_t Partial = 0;
			uint32_t Full = 0;
		};

		Comparison Compare(const std::vector<TexturePixels>& images, const std::vector<uint32_t>& slices, const VolumetricFog& fog, const std::function<glm::vec4(const Sample&)>& expected, float floor, uint32_t rows = k_Grid.y)
		{
			Comparison result;
			std::vector<float> alphas;
			for (size_t i = 0; i < slices.size(); ++i)
			{
				if (i >= images.size() || !IsSlice(images[i]))
				{
					result.Alpha = result.Rgb = 1e9f;
					return result;
				}
				for (uint32_t y = 0; y < rows; ++y)
				{
					for (uint32_t x = 0; x < k_Grid.x; ++x)
					{
						const glm::vec4 want = expected(SampleAt(fog, x, y, slices[i]));
						const glm::vec4 got = images[i].GetPixel(x, y);
						result.Alpha = (std::max)(result.Alpha, Error(got.a, want.a, floor));
						for (int c = 0; c < 3; ++c)
							result.Rgb = (std::max)(result.Rgb, Error(got[c], want[c], floor));
						result.MinAlpha = (std::min)(result.MinAlpha, want.a);
						result.MaxAlpha = (std::max)(result.MaxAlpha, want.a);
						if (want.a > 1e-6f)
						{
							result.MinRed = (std::min)(result.MinRed, want.r);
							result.MaxRed = (std::max)(result.MaxRed, want.r);
						}
						alphas.push_back(want.a);
					}
				}
			}
			for (const float a : alphas)
			{
				if (a < 1e-6f)
					++result.Zero;
				else if (a >= 0.99f * result.MaxAlpha)
					++result.Full;
				else
					++result.Partial;
			}
			return result;
		}

		float WorstDifference(const std::vector<TexturePixels>& a, const std::vector<TexturePixels>& b)
		{
			if (a.empty() || a.size() != b.size())
				return 1e9f;
			float worst = 0.0f;
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (!IsSlice(a[i]) || !IsSlice(b[i]))
					return 1e9f;
				for (uint32_t y = 0; y < k_Grid.y; ++y)
				{
					for (uint32_t x = 0; x < k_Grid.x; ++x)
					{
						const glm::vec4 d = glm::abs(a[i].GetPixel(x, y) - b[i].GetPixel(x, y));
						const float m = (std::max)((std::max)(d.r, d.g), (std::max)(d.b, d.a));
						worst = (std::max)(worst, std::isfinite(m) ? m : 1e9f);
					}
				}
			}
			return worst;
		}

		struct ReadSet
		{
			std::vector<TexturePixels> Images;
			size_t Remaining = 0;
			std::function<void(const std::vector<TexturePixels>&)> Done;
		};

		std::shared_ptr<ReadSet> MakeReadSet(size_t count, std::function<void(const std::vector<TexturePixels>&)> done)
		{
			auto set = std::make_shared<ReadSet>();
			set->Images.resize(count);
			set->Remaining = count;
			set->Done = std::move(done);
			return set;
		}

		void ReadInto(const std::shared_ptr<ReadSet>& set, size_t index, Texture* texture, uint32_t slice, const std::weak_ptr<int>& alive)
		{
			if (!texture)
			{
				if (--set->Remaining == 0)
					set->Done(set->Images);
				return;
			}
			texture->ReadSubresource(0, slice, [alive, set, index](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				set->Images[index] = pixels;
				if (--set->Remaining == 0)
					set->Done(set->Images);
			});
		}

		void ReadSlices(Texture* texture, const std::vector<uint32_t>& slices, const std::weak_ptr<int>& alive, std::function<void(const std::vector<TexturePixels>&)> done)
		{
			const std::shared_ptr<ReadSet> set = MakeReadSet(slices.size(), std::move(done));
			for (size_t i = 0; i < slices.size(); ++i)
				ReadInto(set, i, texture, slices[i], alive);
		}

		Placed Everywhere(FogVolume settings)
		{
			settings.HalfExtents = glm::vec3(300.0f);
			settings.EdgeFade = 0.0f;
			Placed placed;
			placed.Settings = settings;
			placed.World = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, -60.0f));
			return placed;
		}

		std::string Counts(const Comparison& c)
		{
			return std::format("{} empty, {} partial, {} full texels, extinction {:.4f}..{:.4f}", c.Zero, c.Partial, c.Full, c.MinAlpha, c.MaxAlpha);
		}

		using Reporter = std::function<void(bool, const std::string&)>;
		using Expected = std::function<glm::vec4(const Sample&)>;
		using Extra = std::function<std::string(const Comparison&)>;

		void ReadCompare(Texture* texture, const std::vector<uint32_t>& slices, const std::weak_ptr<int>& alive, const Reporter& report, const std::string& name, const VolumetricFog& fog, const Expected& expected, float tolerance, float floor, const Extra& extra = {}, uint32_t rows = k_Grid.y)
		{
			ReadSlices(texture, slices, alive, [=](const std::vector<TexturePixels>& images)
			{
				const Comparison c = Compare(images, slices, fog, expected, floor, rows);
				const std::string problem = extra ? extra(c) : std::string();
				const bool ok = c.Alpha < tolerance && c.Rgb < tolerance && problem.empty();
				report(ok, std::format("{} (worst extinction {:.4f}, in-scatter {:.4f}; {}{})", name, c.Alpha, c.Rgb, Counts(c), problem.empty() ? std::string() : "; " + problem));
			});
		}

		Scene Haze(float ambient = 0.5f)
		{
			Scene scene;
			scene.Ambient = ambient;
			return scene;
		}

		Scene DriftScene(double time, bool temporal)
		{
			Scene scene = Haze();
			scene.Fog.Temporal = temporal;
			scene.Time = time;
			Sun sun;
			sun.Direction = glm::vec3(0.3f, -1.0f, -0.4f);
			scene.Suns.push_back(sun);
			FogVolume volume;
			volume.Density = 0.03f;
			volume.Anisotropy = 0.0f;
			volume.NoiseScale = 96.0f;
			volume.NoiseVelocity = glm::vec3(30.0f, 0.0f, 0.0f);
			volume.NoiseMin = 0.4f;
			volume.NoiseMax = 1.6f;
			scene.Volumes.push_back(Everywhere(volume));
			return scene;
		}

		Placed Place(const FogVolume& settings, const glm::mat4& world)
		{
			Placed placed;
			placed.Settings = settings;
			placed.World = world;
			return placed;
		}
	}

	void FogVolumeTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_FramesBeforeChecks = 0;
		m_Alive = std::make_shared<int>(0);
		m_DriftActive = false;
		m_DriftFrame = 0;
		m_DriftTime = 5.0;

		Renderer3DParams on;
		on.Capabilities.Volumetrics = true;
		on.Capabilities.VolumetricGrid = glm::uvec3(32, 18, 16);
		m_Renderer = Renderer3D::Create(on);
		m_Probe = Renderer3D::Create(on);
		m_Box = Mesh::CreateBox(1.0f, 1.0f, 1.0f);

		Renderer3DParams wide = on;
		wide.Capabilities.MaxFogVolumes = 20;
		m_Wide = Renderer3D::Create(wide);

		Renderer3DParams off;
		off.Capabilities.Volumetrics = false;
		m_Off = Renderer3D::Create(off);

		m_Target = Framebuffer::Create(FramebufferParams()
			.SetDebugName("FogVolumeTest target")
			.SetWidth(32)
			.SetHeight(18)
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA16F }));
	}

	Renderer3D::Statistics FogVolumeTest::Render(Renderer3D& renderer, const std::function<void(Renderer3D&)>& body, bool orthographic)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(m_Target);

		glm::mat4 viewProjection(1.0f);
		if (orthographic)
		{
			const glm::mat4 view = glm::lookAt(k_Eye, k_Eye + k_Forward, glm::vec3(0.0f, 1.0f, 0.0f));
			viewProjection = glm::ortho(-12.0f, 12.0f, -6.75f, 6.75f, 0.1f, 100.0f) * view;
		}
		else
		{
			PerspectiveCamera camera(k_Fov, k_Aspect, 0.1f, 100.0f);
			camera.SetPosition(k_Eye);
			camera.SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
			camera.SetTarget(k_Eye + k_Forward);
			viewProjection = camera.GetViewProjectionMatrix();
		}

		renderer.BeginScene(viewProjection);
		renderer.Clear(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		body(renderer);
		renderer.EndScene();

		Renderer::SetRenderTarget(previous);
		return renderer.GetStatistics();
	}

	void FogVolumeTest::Update(float)
	{
		if (!m_ChecksDone && !Renderer::IsFrameSkipped() && ++m_FramesBeforeChecks > k_FramesBeforeChecks)
		{
			m_ChecksDone = true;
			RunChecks();
		}
		else if (m_DriftActive && !Renderer::IsFrameSkipped())
			StepDrift();

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.EndScene();
	}

	void FogVolumeTest::RunChecks()
	{
		if (!m_Renderer || !m_Wide || !m_Off || !m_Probe || !m_Box || !m_Target)
		{
			Check(false, "Fog volume: the renderers and the target are created");
			return;
		}

		CheckOff();
		CheckMedia();
		CheckShapes();
		CheckHeight();
		CheckFades();
		CheckNoise();
		CheckLight();
		CheckMixture();
		CheckLocalLights();
		CheckBudget();
		CheckScenePath();
		CheckSceneRules();
		CheckInvalid();
		CheckDebugView();
		m_DriftActive = true;
	}

	Texture* FogVolumeTest::Scatter() const
	{
		return Internal::Renderer3DAccess::VolumetricScatter(*m_Probe);
	}

	Texture* FogVolumeTest::Integrated() const
	{
		return Internal::Renderer3DAccess::VolumetricIntegrated(*m_Probe);
	}

	Renderer3D::Statistics FogVolumeTest::Draw(const Scene& scene)
	{
		return Render(*m_Probe, [&](Renderer3D& r)
		{
			r.SetAmbientLight(glm::vec3(1.0f), scene.Ambient);
			for (const Sun& sun : scene.Suns)
			{
				DirectionalLight light;
				light.Direction = glm::normalize(sun.Direction);
				light.Color = sun.Color;
				light.Intensity = 1.0f;
				light.CastShadows = sun.Casts;
				light.VolumetricIntensity = sun.VolumetricIntensity;
				r.SubmitLight(light);
			}
			for (const PointLight& point : scene.Points)
				r.SubmitLight(point);
			for (const SpotLight& spot : scene.Spots)
				r.SubmitLight(spot);
			if (scene.Wall)
				r.SubmitMesh(m_Box, glm::translate(glm::mat4(1.0f), k_Eye + glm::vec3(0.0f, 0.0f, -8.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(30.0f, 30.0f, 0.5f)), glm::vec4(1.0f));
			if (scene.DistanceFog)
			{
				Fog fog;
				fog.Mode = FogMode::Exponential;
				fog.Density = 0.02f;
				r.SetFog(fog);
			}
			if (scene.Global)
				r.SetVolumetricFog(scene.Fog);
			r.SetFogVolumeTime(scene.Time);
			for (const Placed& placed : scene.Volumes)
				r.SubmitFogVolume(placed.Settings, placed.World);
			r.SubmitMesh(m_Box, glm::translate(glm::mat4(1.0f), k_Eye + glm::vec3(0.0f, 0.0f, -10.0f)), glm::vec4(1.0f));
		});
	}

	void FogVolumeTest::CheckMedia()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };

		FogVolume volume;
		volume.Density = 0.04f;
		Scene boxOnly = Haze();
		boxOnly.Global = false;
		boxOnly.DistanceFog = true;
		boxOnly.Volumes.push_back(Everywhere(volume));

		const Renderer3D::Statistics only = Draw(boxOnly);
		Check(only.Fogged && only.Volumetrics && only.VolumetricDispatches == 2 && only.FogVolumes == 1,
			std::format("Fog volume 2a: a box with no VolumetricFog keeps the distance fog and runs the froxel passes (fogged {}, volumetrics {}, dispatches {}, kept {})", only.Fogged, only.Volumetrics, only.VolumetricDispatches, only.FogVolumes));
		ReadCompare(Scatter(), { 4, 8, 12 }, m_Alive, report, "Fog volume 2b: with no VolumetricFog the extinction is the box's own, nothing doubled", boxOnly.Fog,
			[boxOnly](const Sample& s) { return ExpectedScatter(boxOnly, s); }, 0.01f, 0.01f,
			[](const Comparison& c) { return std::abs(c.MinAlpha - 0.04f) < 1e-5f && std::abs(c.MaxAlpha - 0.04f) < 1e-5f ? std::string() : std::string("the box's extinction is not 0.04 everywhere"); });

		Scene both = Haze();
		both.DistanceFog = true;
		both.Fog.Density = 0.02f;
		both.Volumes.push_back(Everywhere(volume));
		const Renderer3D::Statistics replaced = Draw(both);
		Check(!replaced.Fogged && replaced.Volumetrics && replaced.FogVolumes == 1,
			std::format("Fog volume 2c: a VolumetricFog replaces the distance fog (fogged {}, volumetrics {}, kept {})", replaced.Fogged, replaced.Volumetrics, replaced.FogVolumes));
		ReadCompare(Scatter(), { 4, 8, 12 }, m_Alive, report, "Fog volume 2d: the global fog and the box add their extinctions (0.06)", both.Fog,
			[both](const Sample& s) { return ExpectedScatter(both, s); }, 0.01f, 0.01f,
			[](const Comparison& c) { return std::abs(c.MaxAlpha - 0.06f) < 1e-5f ? std::string() : std::string("the summed extinction is not 0.06"); });
	}

	void FogVolumeTest::CheckShapes()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };
		const Extra crosses = [](const Comparison& c) { return c.Zero > 0 && c.Partial > 0 && c.Full > 0 ? std::string() : std::string("the slices do not cross the shape's edge fade"); };

		auto verify = [&](const char* name, const FogVolume& settings, const glm::mat4& world)
		{
			Scene scene = Haze();
			scene.Suns.push_back(Sun{});
			scene.Volumes.push_back(Place(settings, world));
			Draw(scene);
			ReadCompare(Scatter(), { 6, 8, 9, 10, 11, 12 }, m_Alive, report, name, scene.Fog,
				[scene](const Sample& s) { return ExpectedScatter(scene, s); }, 0.01f, 0.01f, crosses);
		};

		FogVolume box;
		box.Shape = FogVolumeShape::Box;
		box.HalfExtents = glm::vec3(3.0f, 3.0f, 4.0f);
		box.Density = 0.1f;
		box.EdgeFade = 1.0f;
		box.Anisotropy = 0.3f;
		verify("Fog volume 3: a rotated box's extinction inside, outside and across its edge fade", box,
			glm::rotate(glm::rotate(At(1.0f, 1.5f, -12.0f), glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

		FogVolume sphere;
		sphere.Shape = FogVolumeShape::Sphere;
		sphere.HalfExtents = glm::vec3(4.0f, 3.0f, 3.0f);
		sphere.Density = 0.1f;
		sphere.EdgeFade = 1.5f;
		sphere.Anisotropy = 0.3f;
		verify("Fog volume 4: a scaled, rotated ellipsoid's extinction inside, outside and across its edge fade", sphere,
			glm::scale(glm::rotate(At(-1.0f, 1.5f, -12.0f), glm::radians(25.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.5f, 1.0f, 1.2f)));
	}

	void FogVolumeTest::CheckHeight()
	{
		FogVolume volume;
		volume.HalfExtents = glm::vec3(300.0f, 15.0f, 300.0f);
		volume.Density = 0.08f;
		volume.EdgeFade = 0.5f;
		volume.HeightFadeStart = 4.0f;
		volume.HeightFadeEnd = 18.0f;
		volume.HeightBandBottom = 7.0f;
		volume.HeightBandTop = 11.0f;
		volume.HeightBandMultiplier = 2.5f;
		volume.HeightBandSoftness = 1.5f;
		Scene scene = Haze();
		scene.Volumes.push_back(Place(volume, At(0.0f, 5.0f, -60.0f)));
		Draw(scene);

		const std::vector<uint32_t> slices = { 8, 10, 12, 14 };
		ReadSlices(Scatter(), slices, m_Alive, [this, scene, slices, volume](const std::vector<TexturePixels>& images)
		{
			const Comparison all = Compare(images, slices, scene.Fog, [&](const Sample& s) { return ExpectedScatter(scene, s); }, 0.01f);
			Check(all.Alpha < 0.01f && all.Rgb < 0.01f && all.Zero > 0 && all.Partial > 20 && all.MaxAlpha > 0.12f,
				std::format("Fog volume 5a: a tall volume's height fade and band match at every texel from 0 to 20 m above its bottom (worst extinction {:.4f}, in-scatter {:.4f}; {})", all.Alpha, all.Rgb, Counts(all)));

			std::string detail;
			float worst = 0.0f;
			float miss = 0.0f;
			for (const float target : { 5.0f, 9.0f, 13.0f, 16.0f })
			{
				float best = 1e9f;
				float got = 0.0f;
				float want = 0.0f;
				for (size_t i = 0; i < slices.size(); ++i)
				{
					for (uint32_t y = 0; y < k_Grid.y; ++y)
					{
						const Sample s = SampleAt(scene.Fog, k_Grid.x / 2, y, slices[i]);
						const float height = s.World.y + 10.0f;
						if (IsSlice(images[i]) && std::abs(height - target) < best)
						{
							best = std::abs(height - target);
							got = images[i].GetPixel(s.X, s.Y).a;
							want = ExpectedScatter(scene, s).a;
						}
					}
				}
				miss = (std::max)(miss, best);
				worst = (std::max)(worst, Error(got, want, 0.01f));
				detail += std::format(" [{} m: {:.4f} want {:.4f} (profile {:.3f})]", target, got, want, HeightProfile(target, volume));
			}
			Check(miss < 0.5f && worst < 0.01f, std::format("Fog volume 5b: the extinction at 5, 9, 13 and 16 m above the bottom follows the profile (worst {:.4f}, nearest texel within {:.2f} m;{})", worst, miss, detail));
		});
	}

	void FogVolumeTest::CheckFades()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };
		FogVolume volume;
		volume.Density = 0.05f;
		volume.NearFadeStart = 2.0f;
		volume.NearFadeEnd = 8.0f;
		volume.NearFadeKeep = 0.2f;
		volume.FarFadeStart = 20.0f;
		volume.FarFadeEnd = 50.0f;
		volume.FarFadeKeep = 0.4f;
		Scene scene = Haze();
		scene.Volumes.push_back(Everywhere(volume));
		Draw(scene);
		ReadCompare(Scatter(), { 2, 4, 6, 8, 10, 12, 14 }, m_Alive, report, "Fog volume 6: near and far fades scale the in-scatter with the camera distance and leave the extinction at 0.05", scene.Fog,
			[scene](const Sample& s) { return ExpectedScatter(scene, s); }, 0.01f, 0.01f,
			[](const Comparison& c)
			{
				if (c.MaxAlpha - c.MinAlpha > 1e-6f)
					return std::string("the expected extinction is not flat");
				return c.MinRed < 0.5f * c.MaxRed ? std::string() : std::string("the fades do not dim the in-scatter");
			});
	}

	void FogVolumeTest::CheckNoise()
	{
		FogVolume plain;
		plain.Density = 0.05f;
		plain.NoiseScale = 3.0f;
		FogVolume equal = plain;
		equal.NoiseMin = equal.NoiseMax = 1.7f;
		Scene off = Haze();
		off.Volumes.push_back(Everywhere(plain));
		Scene same = Haze();
		same.Volumes.push_back(Everywhere(equal));

		const std::vector<uint32_t> slices = { 6, 10 };
		const std::shared_ptr<ReadSet> first = MakeReadSet(4, [this, same, slices](const std::vector<TexturePixels>& images)
		{
			const float difference = WorstDifference({ images[0], images[1] }, { images[2], images[3] });
			const Comparison c = Compare({ images[2], images[3] }, slices, same.Fog, [&](const Sample& s) { return ExpectedScatter(same, s); }, 0.01f);
			Check(difference == 0.0f && c.Alpha < 0.01f && c.Rgb < 0.01f,
				std::format("Fog volume 7a: NoiseMin equal to NoiseMax is no noise at all, bit for bit (difference {:.6f}, extinction {:.4f}, in-scatter {:.4f})", difference, c.Alpha, c.Rgb));
		});
		Draw(off);
		ReadInto(first, 0, Scatter(), slices[0], m_Alive);
		ReadInto(first, 1, Scatter(), slices[1], m_Alive);
		Draw(same);
		ReadInto(first, 2, Scatter(), slices[0], m_Alive);
		ReadInto(first, 3, Scatter(), slices[1], m_Alive);

		FogVolume noisy;
		noisy.Density = 0.05f;
		noisy.NoiseScale = 3.0f;
		noisy.NoiseVelocity = glm::vec3(0.4f, 0.0f, -0.2f);
		noisy.NoiseMin = 0.0f;
		noisy.NoiseMax = 2.0f;
		Scene scene = Haze();
		scene.Time = 7.3;
		scene.Volumes.push_back(Everywhere(noisy));
		Draw(scene);
		const std::vector<uint32_t> noiseSlices = { 4, 7, 10 };
		ReadSlices(Scatter(), noiseSlices, m_Alive, [this, scene, noiseSlices](const std::vector<TexturePixels>& images)
		{
			Scene later = scene;
			later.Time += 3.0;
			const Comparison c = Compare(images, noiseSlices, scene.Fog, [&](const Sample& s) { return ExpectedScatter(scene, s); }, 0.05f);
			const Comparison wrong = Compare(images, noiseSlices, scene.Fog, [&](const Sample& s) { return ExpectedScatter(later, s); }, 0.05f);
			Check(c.Alpha < 0.05f && c.Rgb < 0.05f && c.MaxAlpha - c.MinAlpha > 0.04f,
				std::format("Fog volume 7b: NoiseMin 0 and NoiseMax 2 follow the CPU noise texture, drift phase included (worst extinction {:.4f}, in-scatter {:.4f}; {})", c.Alpha, c.Rgb, Counts(c)));
			Check(wrong.Alpha > 0.2f, std::format("Fog volume 7c: the same noise 3 s later is clearly different, so 7b is sensitive to the phase (worst {:.4f})", wrong.Alpha));
		});
	}

	void FogVolumeTest::StepDrift()
	{
		const uint32_t frame = m_DriftFrame++;
		const bool fast = frame >= k_DriftFrames;
		const double tick = k_DriftTick * (fast ? 4.0 : 1.0);
		m_DriftTime += tick;
		const bool last = frame + 1 == k_DriftFrames || frame + 1 == 2 * k_DriftFrames;
		if (frame + 1 == 2 * k_DriftFrames)
			m_DriftActive = false;

		const Renderer3D::Statistics stats = Draw(DriftScene(m_DriftTime, true));
		if (!last)
			return;

		const std::vector<uint32_t> slices = { 4, 6, 8 };
		const std::shared_ptr<ReadSet> set = MakeReadSet(3 * slices.size(), [this, fast, stats, slices](const std::vector<TexturePixels>& images)
		{
			const size_t n = slices.size();
			const std::vector<TexturePixels> temporal(images.begin(), images.begin() + n);
			const std::vector<TexturePixels> fresh(images.begin() + n, images.begin() + 2 * n);
			const std::vector<TexturePixels> earlier(images.begin() + 2 * n, images.end());
			const float againstFresh = WorstDifference(temporal, fresh);
			const float againstEarlier = WorstDifference(temporal, earlier);
			const char* clock = fast ? "4x" : "1x";
			Check(stats.VolumetricTemporal && againstFresh <= k_DriftTolerance,
				std::format("Fog volume 8{}: after {} Temporal frames at a clock of {} a drifting noise is within 2/255 of the Temporal off volume (worst {:.5f}, temporal {})", fast ? 'b' : 'a', k_DriftFrames, clock, againstFresh, stats.VolumetricTemporal));
			Check(againstEarlier > 3.0f * k_DriftTolerance,
				std::format("Fog volume 8{}: at {} the same volume 10 frames earlier is clearly different, so the match is not a still picture (worst {:.5f})", fast ? 'd' : 'c', clock, againstEarlier));
		});
		for (size_t i = 0; i < slices.size(); ++i)
			ReadInto(set, i, Integrated(), slices[i], m_Alive);
		Draw(DriftScene(m_DriftTime, false));
		for (size_t i = 0; i < slices.size(); ++i)
			ReadInto(set, slices.size() + i, Integrated(), slices[i], m_Alive);
		Draw(DriftScene(m_DriftTime - 10.0 * tick, false));
		for (size_t i = 0; i < slices.size(); ++i)
			ReadInto(set, 2 * slices.size() + i, Integrated(), slices[i], m_Alive);
	}

	void FogVolumeTest::CheckLight()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };

		auto tinted = [](float anisotropy, const glm::vec3& sunDirection)
		{
			FogVolume volume;
			volume.Density = 0.04f;
			volume.Albedo = glm::vec3(0.8f, 0.6f, 1.0f);
			volume.ScatterTint = glm::vec3(0.5f, 2.0f, 0.25f);
			volume.AmbientIntensity = 2.0f;
			volume.LightIntensity = 3.0f;
			volume.Anisotropy = anisotropy;
			Scene scene = Haze();
			Sun sun;
			sun.Direction = sunDirection;
			sun.Color = glm::vec3(1.0f, 0.9f, 0.7f);
			scene.Suns.push_back(sun);
			scene.Volumes.push_back(Everywhere(volume));
			return scene;
		};
		const Scene forward = tinted(0.6f, glm::vec3(0.3f, -1.0f, -0.5f));
		Draw(forward);
		ReadCompare(Scatter(), { 4, 8, 12 }, m_Alive, report, "Fog volume 9a: a volume's Albedo, ScatterTint, ambient and light intensities and Anisotropy 0.6 shape its in-scatter", forward.Fog,
			[forward](const Sample& s) { return ExpectedScatter(forward, s); }, 0.01f, 0.01f);
		const Scene backward = tinted(-0.5f, glm::vec3(0.2f, -0.3f, 1.0f));
		Draw(backward);
		ReadCompare(Scatter(), { 4, 8, 12 }, m_Alive, report, "Fog volume 9b: Anisotropy -0.5 under a sun the camera looks toward", backward.Fog,
			[backward](const Sample& s) { return ExpectedScatter(backward, s); }, 0.01f, 0.01f);

		FogVolume dim;
		dim.Density = 0.02f;
		dim.OutsideShadowLight = 0.25f;
		dim.Anisotropy = 0.3f;
		Scene shadowed = Haze(0.0f);
		shadowed.Fog.NearDistance = 0.5f;
		shadowed.Fog.MaxDistance = 300.0f;
		Sun sun;
		sun.Direction = glm::vec3(0.3f, -1.0f, -0.5f);
		sun.Casts = true;
		shadowed.Suns.push_back(sun);
		shadowed.Volumes.push_back(Everywhere(dim));
		const Renderer3D::Statistics stats = Draw(shadowed);
		if (stats.ShadowCascades == 0)
		{
			Check(false, "Fog volume 9c: the sun draws shadow cascades");
			return;
		}

		const float end = stats.ShadowCascadeEnds[stats.ShadowCascades - 1];
		const float start = stats.ShadowCascades > 1 ? stats.ShadowCascadeEnds[stats.ShadowCascades - 2] : 0.0f;
		std::vector<uint32_t> inside;
		std::vector<uint32_t> outside;
		for (uint32_t slice = 0; slice < k_Grid.z; ++slice)
		{
			const float depth = SliceDepth(shadowed.Fog, static_cast<float>(slice) + 0.5f);
			if (depth < end - 0.5f * (end - start) && depth > 2.0f)
				inside.push_back(slice);
			else if (depth > end + 1.0f)
				outside.push_back(slice);
		}
		Check(!inside.empty() && !outside.empty(), std::format("Fog volume 9c: the slices reach both inside and past the last cascade (end {:.1f} m; {} inside, {} past)", end, inside.size(), outside.size()));
		if (inside.empty() || outside.empty())
			return;
		ReadCompare(Scatter(), inside, m_Alive, report, "Fog volume 9d: air inside the cascades the shadow map proves lit takes the whole sun", shadowed.Fog,
			[shadowed](const Sample& s) { return ExpectedScatter(shadowed, s, 1.0f); }, 0.01f, 0.01f, {}, 5);
		ReadCompare(Scatter(), outside, m_Alive, report, "Fog volume 9e: air past the last cascade takes the volume's OutsideShadowLight (0.25) of the sun", shadowed.Fog,
			[shadowed](const Sample& s) { return ExpectedScatter(shadowed, s, 0.25f); }, 0.01f, 0.01f, {}, 5);
	}

	void FogVolumeTest::CheckMixture()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };

		Scene scene = Haze(0.6f);
		scene.Fog.Density = 0.02f;
		scene.Fog.HeightFalloff = 0.05f;
		scene.Fog.Albedo = glm::vec3(0.9f, 0.9f, 1.0f);
		scene.Fog.Anisotropy = 0.1f;
		scene.Fog.ScatterTint = glm::vec3(1.0f, 0.8f, 0.8f);
		Sun key;
		key.Direction = glm::vec3(-0.4f, -1.0f, -0.3f);
		key.Color = glm::vec3(1.0f, 0.8f, 0.6f);
		Sun fill;
		fill.Direction = glm::vec3(0.2f, -0.5f, 1.0f);
		fill.Color = glm::vec3(0.3f, 0.4f, 0.9f);
		scene.Suns = { key, fill };

		FogVolume box;
		box.HalfExtents = glm::vec3(4.0f, 3.0f, 4.0f);
		box.Density = 0.06f;
		box.Anisotropy = 0.7f;
		box.ScatterTint = glm::vec3(0.5f, 1.0f, 1.5f);
		box.Albedo = glm::vec3(1.0f, 0.7f, 0.5f);
		box.AmbientIntensity = 0.8f;
		box.LightIntensity = 1.5f;
		box.EdgeFade = 1.0f;
		FogVolume sphere;
		sphere.Shape = FogVolumeShape::Sphere;
		sphere.HalfExtents = glm::vec3(4.0f, 3.0f, 4.0f);
		sphere.Density = 0.04f;
		sphere.Anisotropy = -0.3f;
		sphere.ScatterTint = glm::vec3(1.5f, 1.0f, 0.5f);
		sphere.Albedo = glm::vec3(0.6f, 0.9f, 1.0f);
		sphere.AmbientIntensity = 1.2f;
		sphere.LightIntensity = 0.7f;
		sphere.EdgeFade = 1.0f;
		scene.Volumes.push_back(Place(box, At(-1.5f, 1.0f, -12.0f)));
		scene.Volumes.push_back(Place(sphere, At(1.5f, 1.5f, -12.0f)));

		const Renderer3D::Statistics stats = Draw(scene);
		Check(stats.FogVolumes == 2, std::format("Fog volume 10a: both overlapping volumes are kept (kept {})", stats.FogVolumes));
		ReadCompare(Scatter(), { 6, 8, 9, 10, 11, 12 }, m_Alive, report, "Fog volume 10b: two overlapping volumes and the global fog against the CPU mixture of the three", scene.Fog,
			[scene](const Sample& s) { return ExpectedScatter(scene, s); }, 0.01f, 0.01f,
			[](const Comparison& c) { return c.MaxAlpha > 0.1f ? std::string() : std::string("the volumes do not overlap in the slices"); });

		Scene capped = scene;
		capped.Fog.MaxInScatter = 0.12f;
		const std::shared_ptr<ReadSet> set = MakeReadSet(4, [this, capped](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			float alpha = 0.0f;
			uint32_t scaled = 0;
			uint32_t free = 0;
			for (size_t i = 0; i < 2; ++i)
			{
				if (!IsSlice(images[i]) || !IsSlice(images[i + 2]))
				{
					worst = alpha = 1e9f;
					break;
				}
				for (uint32_t y = 0; y < k_Grid.y; ++y)
				{
					for (uint32_t x = 0; x < k_Grid.x; ++x)
					{
						const glm::vec4 open = images[i].GetPixel(x, y);
						const glm::vec4 got = images[i + 2].GetPixel(x, y);
						const float peak = (std::max)((std::max)(open.r, open.g), open.b);
						const glm::vec3 want = peak > capped.Fog.MaxInScatter ? glm::vec3(open) * (capped.Fog.MaxInScatter / peak) : glm::vec3(open);
						(peak > capped.Fog.MaxInScatter ? scaled : free) += 1;
						worst = (std::max)(worst, (std::max)((std::max)(std::abs(got.r - want.r), std::abs(got.g - want.g)), std::abs(got.b - want.b)));
						alpha = (std::max)(alpha, std::abs(got.a - open.a));
					}
				}
			}
			Check(worst < 1e-3f && alpha < 1e-5f && scaled > 0 && free > 0,
				std::format("Fog volume 10c: the global MaxInScatter scales the strongest channel down to 0.12 and keeps the hue, the transmittance alone (worst {:.5f}, alpha {:.6f}, {} texels capped, {} free)", worst, alpha, scaled, free));
		});
		Draw(scene);
		ReadInto(set, 0, Integrated(), 6, m_Alive);
		ReadInto(set, 1, Integrated(), 15, m_Alive);
		Draw(capped);
		ReadInto(set, 2, Integrated(), 6, m_Alive);
		ReadInto(set, 3, Integrated(), 15, m_Alive);
	}

	void FogVolumeTest::CheckLocalLights()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };

		auto lamps = [](Scene scene)
		{
			PointLight point;
			point.Position = glm::vec3(0.5f, 1.5f, -8.0f);
			point.Color = glm::vec3(1.0f, 0.8f, 0.6f);
			point.Intensity = 6.0f;
			point.Range = 14.0f;
			scene.Points.push_back(point);
			return scene;
		};
		auto spotted = [](Scene scene, const glm::vec3& position, const glm::vec3& direction)
		{
			SpotLight spot;
			spot.Position = position;
			spot.Direction = direction;
			spot.Color = glm::vec3(0.7f, 0.9f, 1.0f);
			spot.Intensity = 8.0f;
			spot.Range = 40.0f;
			spot.InnerConeAngle = 10.0f;
			spot.OuterConeAngle = 40.0f;
			scene.Spots.push_back(spot);
			return scene;
		};
		auto sunlit = [](Scene scene)
		{
			Sun sun;
			sun.Direction = glm::vec3(0.3f, -1.0f, -0.5f);
			sun.Color = glm::vec3(1.0f, 0.9f, 0.7f);
			scene.Suns.push_back(sun);
			return scene;
		};
		auto haze = [](float density, float anisotropy)
		{
			Scene scene = Haze(0.0f);
			scene.Fog.Density = density;
			scene.Fog.Anisotropy = anisotropy;
			return scene;
		};
		auto withVolume = [](Scene scene, float localLightIntensity)
		{
			FogVolume volume;
			volume.Density = 0.03f;
			volume.Anisotropy = -0.2f;
			volume.Albedo = glm::vec3(1.0f, 0.7f, 0.5f);
			volume.ScatterTint = glm::vec3(0.5f, 1.0f, 1.5f);
			volume.LocalLightIntensity = localLightIntensity;
			scene.Volumes.push_back(Everywhere(volume));
			return scene;
		};
		auto gather = [this](const std::vector<Scene>& scenes, const std::vector<uint32_t>& slices, std::function<void(const std::vector<std::vector<TexturePixels>>&)> done)
		{
			const size_t count = scenes.size();
			const size_t perScene = slices.size();
			const std::shared_ptr<ReadSet> set = MakeReadSet(count * perScene, [count, perScene, done](const std::vector<TexturePixels>& images)
			{
				std::vector<std::vector<TexturePixels>> grouped(count);
				for (size_t s = 0; s < count; ++s)
					grouped[s].assign(images.begin() + s * perScene, images.begin() + (s + 1) * perScene);
				done(grouped);
			});
			std::vector<Renderer3D::Statistics> stats;
			for (size_t s = 0; s < count; ++s)
			{
				stats.push_back(Draw(scenes[s]));
				for (size_t i = 0; i < perScene; ++i)
					ReadInto(set, s * perScene + i, Scatter(), slices[i], m_Alive);
			}
			return stats;
		};

		const Scene pointOnly = lamps(haze(0.04f, 0.3f));
		Draw(pointOnly);
		ReadCompare(Scatter(), { 6, 8, 10, 12 }, m_Alive, report, "Fog volume 15a: a point light scatters in the global fog as pi x Henyey-Greenstein x falloff^2 x intensity", pointOnly.Fog,
			[pointOnly](const Sample& s) { return ExpectedScatter(pointOnly, s); }, 0.01f, 0.01f,
			[](const Comparison& c) { return c.MaxRed > 0.01f && c.MaxRed - c.MinRed > 0.01f ? std::string() : std::string("the light adds too little in-scatter to prove anything"); });

		Scene mixed = sunlit(haze(0.04f, 0.3f));
		mixed.Fog.LocalLightIntensity = 2.0f;
		mixed = withVolume(mixed, 0.5f);
		mixed = lamps(mixed);
		mixed = spotted(mixed, glm::vec3(1.0f, 1.0f, -6.0f), glm::vec3(-0.2f, 0.0f, -1.0f));
		mixed.Spots[0].VolumetricIntensity = 0.5f;
		Draw(mixed);
		ReadCompare(Scatter(), { 6, 8, 10, 12, 13 }, m_Alive, report, "Fog volume 15b: a point and a spot light (cone, VolumetricIntensity 0.5) in the global fog at LocalLightIntensity 2 and a volume at 0.5, with a sun", mixed.Fog,
			[mixed](const Sample& s) { return ExpectedScatter(mixed, s); }, 0.01f, 0.01f,
			[](const Comparison& c) { return c.MaxRed > 0.02f ? std::string() : std::string("the lights add too little in-scatter to prove anything"); });

		const Scene base = withVolume(sunlit(haze(0.04f, 0.3f)), 1.0f);
		Scene noLocal = spotted(lamps(base), glm::vec3(1.0f, 1.0f, -6.0f), glm::vec3(-0.2f, 0.0f, -1.0f));
		noLocal.Fog.LocalLightIntensity = 0.0f;
		noLocal.Volumes[0].Settings.LocalLightIntensity = 0.0f;
		Scene globalZero = spotted(lamps(base), glm::vec3(1.0f, 1.0f, -6.0f), glm::vec3(-0.2f, 0.0f, -1.0f));
		globalZero.Fog.LocalLightIntensity = 0.0f;
		Scene noLight = spotted(lamps(base), glm::vec3(1.0f, 1.0f, -6.0f), glm::vec3(-0.2f, 0.0f, -1.0f));
		noLight.Points[0].VolumetricIntensity = 0.0f;
		noLight.Spots[0].VolumetricIntensity = 0.0f;
		const Scene lit = spotted(lamps(base), glm::vec3(1.0f, 1.0f, -6.0f), glm::vec3(-0.2f, 0.0f, -1.0f));
		gather({ base, noLocal, noLight, lit, globalZero }, { 8, 10, 12 }, [this](const std::vector<std::vector<TexturePixels>>& images)
		{
			const float local = WorstDifference(images[0], images[1]);
			const float light = WorstDifference(images[0], images[2]);
			const float control = WorstDifference(images[0], images[3]);
			const float globalOnly = WorstDifference(images[0], images[4]);
			Check(local == 0.0f && light == 0.0f && control > 0.005f,
				std::format("Fog volume 15c: LocalLightIntensity 0 (global and volume) and every light's VolumetricIntensity 0 each leave the directional-only volume untouched, while the lights do change it (differences {:.6f}, {:.6f}; with the lights {:.4f})", local, light, control));
			Check(globalOnly == 0.0f && control > 0.005f,
				std::format("Fog volume 15g: the global LocalLightIntensity 0 alone silences the lights in a volume that keeps its own at 1 (difference {:.6f}; with the lights {:.4f})", globalOnly, control));
		});

		Scene ambientHaze = haze(0.04f, 0.3f);
		ambientHaze.Ambient = 0.5f;
		const Scene fullSun = sunlit(ambientHaze);
		Scene halfSun = fullSun;
		halfSun.Suns[0].VolumetricIntensity = 0.5f;
		Scene noSunLight = fullSun;
		noSunLight.Suns[0].VolumetricIntensity = 0.0f;
		const Scene& noSun = ambientHaze;
		Draw(halfSun);
		ReadCompare(Scatter(), { 6, 8, 10, 12 }, m_Alive, report, "Fog volume 15h: a sun's VolumetricIntensity 0.5 halves its in-scatter in a scene with only the global fog", halfSun.Fog,
			[halfSun](const Sample& s) { return ExpectedScatter(halfSun, s); }, 0.01f, 0.01f,
			[](const Comparison& c) { return c.MaxRed > 0.005f ? std::string() : std::string("the sun adds too little in-scatter to prove anything"); });
		gather({ fullSun, noSunLight, noSun }, { 8, 10, 12 }, [this](const std::vector<std::vector<TexturePixels>>& images)
		{
			const float silenced = WorstDifference(images[1], images[2]);
			const float control = WorstDifference(images[0], images[2]);
			Check(silenced == 0.0f && control > 0.005f,
				std::format("Fog volume 15i: a sun with VolumetricIntensity 0 leaves only the ambient in-scatter, as if it were absent (difference {:.6f}; the sun at 1 differs by {:.4f})", silenced, control));
		});

		const Scene sunOnly = sunlit(haze(0.04f, 0.3f));
		const Scene away = spotted(sunOnly, glm::vec3(0.0f, 1.0f, -8.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		const Scene towards = spotted(sunOnly, glm::vec3(0.0f, 1.0f, -8.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		gather({ sunOnly, away, towards }, { 11, 12, 13, 14 }, [this](const std::vector<std::vector<TexturePixels>>& images)
		{
			const float outside = WorstDifference(images[0], images[1]);
			const float inside = WorstDifference(images[0], images[2]);
			Check(outside < 3e-4f && inside > 0.005f,
				std::format("Fog volume 15d: a spot light adds nothing past the cone's edge (it points at the camera, the slices lie behind it: {:.6f}) and the same light pointing at them does ({:.4f})", outside, inside));
		});

		Scene walled = sunOnly;
		walled.Wall = true;
		Scene shadowed = spotted(walled, glm::vec3(0.0f, 1.0f, -4.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		shadowed.Spots[0].CastShadows = true;
		const Scene unshadowed = spotted(walled, glm::vec3(0.0f, 1.0f, -4.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		const std::vector<Renderer3D::Statistics> stats = gather({ walled, shadowed, unshadowed }, { 11, 12, 13, 14 }, [this](const std::vector<std::vector<TexturePixels>>& images)
		{
			const float behind = WorstDifference(images[0], images[1]);
			const float open = WorstDifference(images[0], images[2]);
			Check(behind < 3e-4f && open > 0.005f,
				std::format("Fog volume 15e: the air behind a wall the spot light shines on gets none of it ({:.6f}), and without the shadow it does ({:.4f})", behind, open));
		});
		Check(stats[1].ShadowedLights == 1, std::format("Fog volume 15f: the spot light casting past the wall draws a shadow (shadowed lights {})", stats[1].ShadowedLights));
	}

	void FogVolumeTest::CheckDebugView()
	{
		const Reporter report = [this](bool ok, const std::string& name) { Check(ok, name); };
		m_Probe->SetVolumetricDebugView(VolumetricDebugView::ShadowSamples);

		FogVolume dim;
		dim.Density = 0.02f;
		Scene shadowed = Haze(0.0f);
		shadowed.Fog.NearDistance = 0.5f;
		shadowed.Fog.MaxDistance = 300.0f;
		Sun sun;
		sun.Direction = glm::vec3(0.3f, -1.0f, -0.5f);
		sun.Casts = true;
		shadowed.Suns.push_back(sun);
		shadowed.Volumes.push_back(Everywhere(dim));
		const Renderer3D::Statistics stats = Draw(shadowed);
		if (stats.ShadowCascades == 0)
		{
			Check(false, "Fog volume 16a: the sun draws shadow cascades");
			m_Probe->SetVolumetricDebugView(VolumetricDebugView::None);
			return;
		}

		const float end = stats.ShadowCascadeEnds[stats.ShadowCascades - 1];
		const float start = stats.ShadowCascades > 1 ? stats.ShadowCascadeEnds[stats.ShadowCascades - 2] : 0.0f;
		std::vector<uint32_t> inside;
		std::vector<uint32_t> outside;
		for (uint32_t slice = 0; slice < k_Grid.z; ++slice)
		{
			const float depth = SliceDepth(shadowed.Fog, static_cast<float>(slice) + 0.5f);
			if (depth < end - 0.5f * (end - start) && depth > 2.0f)
				inside.push_back(slice);
			else if (depth > end + 1.0f)
				outside.push_back(slice);
		}
		Check(!inside.empty() && !outside.empty(), std::format("Fog volume 16a: the slices reach both inside and past the last cascade ({} inside, {} past)", inside.size(), outside.size()));
		if (!inside.empty() && !outside.empty())
		{
			ReadCompare(Scatter(), inside, m_Alive, report, "Fog volume 16b: the debug view paints air the shadow map proves lit green (0, 0.02, 0)", shadowed.Fog,
				[](const Sample&) { return glm::vec4(0.0f, 0.02f, 0.0f, 0.02f); }, 0.01f, 0.01f, {}, 5);
			ReadCompare(Scatter(), outside, m_Alive, report, "Fog volume 16c: the debug view paints air past the last cascade blue (0, 0, 0.02)", shadowed.Fog,
				[](const Sample&) { return glm::vec4(0.0f, 0.0f, 0.02f, 0.02f); }, 0.01f, 0.01f, {}, 5);
		}

		Scene unshadowed = Haze();
		unshadowed.Suns.push_back(Sun{});
		unshadowed.Volumes.push_back(Everywhere(dim));
		Draw(unshadowed);
		ReadCompare(Scatter(), { 4, 8, 12 }, m_Alive, report, "Fog volume 16d: with no shadow-casting sun every sample is yellow (0.02, 0.02, 0)", unshadowed.Fog,
			[](const Sample&) { return glm::vec4(0.02f, 0.02f, 0.0f, 0.02f); }, 0.01f, 0.01f);

		m_Probe->SetVolumetricDebugView(VolumetricDebugView::None);
		Draw(unshadowed);
		ReadCompare(Scatter(), { 4, 8, 12 }, m_Alive, report, "Fog volume 16e: with the debug view off the in-scatter is the normal sun and ambient light", unshadowed.Fog,
			[unshadowed](const Sample& s) { return ExpectedScatter(unshadowed, s); }, 0.01f, 0.01f);

		Internal::Renderer3DAccess::RequestVolumetricSlice(*m_Probe, true, false, 8, 2.0f);
		Draw(unshadowed);
		Texture* picture = Internal::Renderer3DAccess::VolumetricSlice(*m_Probe);
		Check(picture && picture->GetParams().Width == k_Grid.x && picture->GetParams().Height == k_Grid.y,
			std::format("Fog volume 16f: the slice texture has the grid's size (got {} x {})", picture ? picture->GetParams().Width : 0u, picture ? picture->GetParams().Height : 0u));
		const std::shared_ptr<ReadSet> set = MakeReadSet(2, [report](const std::vector<TexturePixels>& images)
		{
			float worst = 0.0f;
			bool shaped = IsSlice(images[0]) && IsSlice(images[1]);
			for (uint32_t y = 0; shaped && y < k_Grid.y; ++y)
			{
				for (uint32_t x = 0; x < k_Grid.x; ++x)
				{
					const glm::vec4 source = images[0].GetPixel(x, y);
					const glm::vec4 shown = images[1].GetPixel(x, y);
					worst = (std::max)(worst, glm::length(glm::vec3(shown) - 2.0f * glm::vec3(source)));
					worst = (std::max)(worst, std::abs(shown.a - 1.0f));
				}
			}
			report(shaped && worst < 1e-3f, std::format("Fog volume 16g: slice 8 of the scatter volume, at exposure 2, is the in-scatter doubled with alpha 1 (shaped {}, worst {:.5f})", shaped, worst));
		});
		ReadInto(set, 0, Scatter(), 8, m_Alive);
		ReadInto(set, 1, picture, 0, m_Alive);
	}

	void FogVolumeTest::CheckOff()
	{
		const Renderer3D::Statistics off = Render(*m_Off, [](Renderer3D& r) { r.SubmitFogVolume(Volume(1.0f), At(0.0f, 1.0f, -5.0f)); });
		Check(off.FogVolumes == 0 && off.RaymarchVolumes == 0 && off.VolumetricDispatches == 0 && Access::FogVolumeFroxelCount(*m_Off) == 0,
			std::format("Fog volume 1a: with Capabilities.Volumetrics off no volume is kept and nothing is dispatched (volumes {}, dispatches {})", off.FogVolumes, off.VolumetricDispatches));

		const Renderer3D::Statistics orthographic = Render(*m_Renderer, [](Renderer3D& r) { r.SubmitFogVolume(Volume(1.0f), At(0.0f, 1.0f, -5.0f)); }, true);
		Check(orthographic.FogVolumes == 0 && orthographic.VolumetricDispatches == 0 && Access::FogVolumeFroxelCount(*m_Renderer) == 0,
			std::format("Fog volume 1b: an orthographic camera keeps no volume (volumes {}, dispatches {})", orthographic.FogVolumes, orthographic.VolumetricDispatches));

		const Renderer3D::Statistics behind = Render(*m_Renderer, [](Renderer3D& r) { r.SubmitFogVolume(Volume(1.0f), At(0.0f, 1.0f, 20.0f)); });
		Check(behind.FogVolumes == 0 && behind.FogVolumesCulled == 1 && behind.FogVolumesDropped == 0,
			std::format("Fog volume 1c: a volume behind the camera is culled (kept {}, culled {}, dropped {})", behind.FogVolumes, behind.FogVolumesCulled, behind.FogVolumesDropped));

		const Renderer3D::Statistics past = Render(*m_Renderer, [](Renderer3D& r) { r.SubmitFogVolume(Volume(1.0f), At(0.0f, 1.0f, -90.0f)); });
		Check(past.FogVolumes == 0 && past.FogVolumesCulled == 1,
			std::format("Fog volume 1d: a Froxel volume past the volumetric MaxDistance is culled (kept {}, culled {})", past.FogVolumes, past.FogVolumesCulled));

		const Renderer3D::Statistics front = Render(*m_Renderer, [](Renderer3D& r) { r.SubmitFogVolume(Volume(1.0f), At(0.0f, 1.0f, -5.0f)); });
		Check(front.FogVolumes == 1 && front.RaymarchVolumes == 0 && front.FogVolumesCulled == 0 && Access::FogVolumeFroxelCount(*m_Renderer) == 1 && front.VolumetricDispatches == 2,
			std::format("Fog volume 1e: a volume in front is kept as a Froxel one and, with no global fog, dispatches the froxel passes (kept {}, dispatches {})", front.FogVolumes, front.VolumetricDispatches));

		const Renderer3D::Statistics empty = Render(*m_Renderer, [](Renderer3D&) {});
		Check(empty.FogVolumes == 0 && Access::FogVolumeFroxelCount(*m_Renderer) == 0, "Fog volume 1f: a scene that submits none keeps none");
	}

	void FogVolumeTest::CheckBudget()
	{
		static constexpr int k_Distance[10] = { 7, 2, 9, 0, 5, 8, 1, 4, 6, 3 };
		const Renderer3D::Statistics tight = Render(*m_Renderer, [](Renderer3D& r)
		{
			for (int i = 0; i < 10; ++i)
				r.SubmitFogVolume(Volume(static_cast<float>(i + 1)), At(0.0f, 1.0f, -3.0f - 2.0f * static_cast<float>(k_Distance[i])));
		});
		const std::vector<int> expected = { 1, 2, 4, 5, 7, 8, 9, 10 };
		const std::vector<int> kept = KeptIds(*m_Renderer);
		Check(tight.FogVolumes == 8 && tight.FogVolumesDropped == 2 && tight.FogVolumesCulled == 0,
			std::format("Fog volume 11a: 10 volumes against a budget of 8 keep 8 and drop 2 (kept {}, dropped {}, culled {})", tight.FogVolumes, tight.FogVolumesDropped, tight.FogVolumesCulled));
		Check(kept == expected, std::format("Fog volume 11b: the 2 farthest are dropped and the rest stay in submission order (kept {}, expected {})", Describe(kept), Describe(expected)));

		const Renderer3D::Statistics wide = Render(*m_Wide, [](Renderer3D& r)
		{
			for (int i = 0; i < 20; ++i)
				r.SubmitFogVolume(Volume(static_cast<float>(i + 1)), At(0.0f, 1.0f, -3.0f - 2.0f * static_cast<float>(i)));
		});
		std::vector<int> firstSixteen;
		for (int i = 1; i <= 16; ++i)
			firstSixteen.push_back(i);
		const std::vector<int> keptWide = KeptIds(*m_Wide);
		Check(wide.FogVolumes == 16 && wide.FogVolumesDropped == 4 && keptWide == firstSixteen,
			std::format("Fog volume 11c: a budget of 20 is capped at 16, so 20 volumes keep the nearest 16 (kept {}, dropped {}, ids {})", wide.FogVolumes, wide.FogVolumesDropped, Describe(keptWide)));

		const Renderer3D::Statistics tie = Render(*m_Renderer, [](Renderer3D& r)
		{
			for (int i = 0; i < 10; ++i)
				r.SubmitFogVolume(Volume(static_cast<float>(i + 1)), At(0.0f, 1.0f, -6.0f));
		});
		const std::vector<int> firstEight = { 1, 2, 3, 4, 5, 6, 7, 8 };
		const std::vector<int> keptTie = KeptIds(*m_Renderer);
		Check(tie.FogVolumes == 8 && tie.FogVolumesDropped == 2 && keptTie == firstEight,
			std::format("Fog volume 11d: equal distances drop the later submissions (kept {}, ids {})", tie.FogVolumes, Describe(keptTie)));

		Renderer3D::Statistics again = Render(*m_Renderer, [](Renderer3D& r) { r.SubmitFogVolume(Volume(1.0f), At(0.0f, 1.0f, -3.0f)); });
		Check(again.FogVolumes == 1 && again.FogVolumesDropped == 0, std::format("Fog volume 11e: the next scene starts again from nothing (kept {}, dropped {})", again.FogVolumes, again.FogVolumesDropped));
	}

	void FogVolumeTest::CheckScenePath()
	{
		const FogVolume first = SceneVolume();
		FogVolume second = Volume(2.0f);
		second.HalfExtents = glm::vec3(1.0f, 2.0f, 1.0f);
		second.Mode = FogVolumeMode::Froxel;

		Transform3DComponent firstTransform(glm::vec3(0.5f, 1.0f, -8.0f), glm::vec3(1.5f, 1.0f, 2.0f));
		firstTransform.SetRotationEuler(glm::vec3(10.0f, 35.0f, 5.0f));
		Transform3DComponent secondTransform(glm::vec3(-3.0f, 1.0f, -14.0f), glm::vec3(2.0f));

		const Renderer3D::Statistics viaScene = Render(*m_Renderer, [&](Renderer3D& r)
		{
			Dingo::Scene scene("Fog volume scene");
			Entity a = scene.CreateEntity("First");
			a.AddComponent<Transform3DComponent>(firstTransform);
			a.AddComponent(FogVolumeComponent(first));
			Entity b = scene.CreateEntity("Second");
			b.AddComponent<Transform3DComponent>(secondTransform);
			b.AddComponent(FogVolumeComponent(second));
			scene.SubmitLights(r);
		});
		const Renderer3D::Statistics direct = Render(*m_Wide, [&](Renderer3D& r)
		{
			r.SetFogVolumeTime(0.0);
			r.SubmitFogVolume(first, firstTransform.GetTransform());
			r.SubmitFogVolume(second, secondTransform.GetTransform());
		});

		Check(viaScene.FogVolumes == 2 && direct.FogVolumes == 2, std::format("Fog volume 12a: both paths keep both volumes (scene {}, direct {})", viaScene.FogVolumes, direct.FogVolumes));
		Check(viaScene.FogVolumes == 2 && SameRecords(*m_Renderer, *m_Wide, 2), "Fog volume 12b: the records from FogVolumeComponents are byte for byte those of SubmitFogVolume");

		const Record& record = Access::FogVolumeRecords(*m_Renderer).Volumes[0];
		const bool sphere = record.Shape.w == 1.0f;
		const bool scaled = std::abs(record.Shape.x - 2.0f * 1.5f) < 1e-4f && std::abs(record.Shape.y - 1.5f * 1.0f) < 1e-4f && std::abs(record.Shape.z - 3.0f * 2.0f) < 1e-4f;
		Check(sphere && scaled, std::format("Fog volume 12c: the record's world half extents are HalfExtents times the entity's scale ({:.3f}, {:.3f}, {:.3f}), sphere {}", record.Shape.x, record.Shape.y, record.Shape.z, sphere));
	}

	void FogVolumeTest::CheckSceneRules()
	{
		const FogVolume volume = Volume(1.0f);
		const Transform3DComponent placed(glm::vec3(0.0f, 1.0f, -6.0f));

		const Renderer3D::Statistics stats = Render(*m_Renderer, [&](Renderer3D& r)
		{
			Dingo::Scene scene("Fog volume rules");

			Entity active = scene.CreateEntity("Active");
			active.AddComponent<Transform3DComponent>(placed);
			active.AddComponent(FogVolumeComponent(volume));

			Entity disabled = scene.CreateEntity("Disabled");
			disabled.AddComponent<Transform3DComponent>(placed);
			disabled.AddComponent(FogVolumeComponent(volume)).Enabled = false;

			Entity inactive = scene.CreateEntity("Inactive");
			inactive.AddComponent<Transform3DComponent>(placed);
			inactive.AddComponent(FogVolumeComponent(volume));
			inactive.SetActive(false);

			scene.CreateEntity("Unplaced").AddComponent(FogVolumeComponent(volume));

			scene.DuplicateEntity(active);
			scene.SubmitLights(r);
		});
		Check(stats.FogVolumes == 2 && Access::FogVolumeFroxelCount(*m_Renderer) == 2,
			std::format("Fog volume 13a: of five components only the active one and its duplicate are submitted (kept {})", stats.FogVolumes));
		Check(stats.FogVolumes == 2 && std::memcmp(&Access::FogVolumeRecords(*m_Renderer).Volumes[0], &Access::FogVolumeRecords(*m_Renderer).Volumes[1], sizeof(Record)) == 0,
			"Fog volume 13b: the duplicate records the same volume as the original");

		const Renderer3D::Statistics reenabled = Render(*m_Renderer, [&](Renderer3D& r)
		{
			Dingo::Scene scene("Fog volume reenabled");
			Entity entity = scene.CreateEntity("Toggled");
			entity.AddComponent<Transform3DComponent>(placed);
			entity.AddComponent(FogVolumeComponent(volume));
			entity.SetActive(false);
			entity.SetActive(true);
			scene.SubmitLights(r);
		});
		Check(reenabled.FogVolumes == 1, std::format("Fog volume 13c: an entity set inactive and active again submits its volume (kept {})", reenabled.FogVolumes));
	}

	void FogVolumeTest::CheckInvalid()
	{
		const glm::mat4 placed = At(0.0f, 1.0f, -6.0f);

		std::vector<std::pair<std::string, bool>> results;
		const Renderer3D::Statistics stats = Render(*m_Renderer, [&](Renderer3D& r)
		{
			auto expect = [&](const char* name, bool accepted, const std::function<void(FogVolume&)>& change)
			{
				FogVolume volume = Volume(1.0f);
				change(volume);
				results.emplace_back(name, r.SubmitFogVolume(volume, placed) == accepted);
			};
			auto expectTransform = [&](const char* name, bool accepted, const glm::mat4& transform)
			{
				results.emplace_back(name, r.SubmitFogVolume(Volume(1.0f), transform) == accepted);
			};

			expect("the defaults are accepted", true, [](FogVolume& volume) { volume = FogVolume{}; });
			expect("a negative Density is rejected", false, [](FogVolume& volume) { volume.Density = -1.0f; });
			expect("a NaN Density is rejected", false, [](FogVolume& volume) { volume.Density = k_NaN; });
			expect("an infinite Density is rejected", false, [](FogVolume& volume) { volume.Density = k_Infinity; });
			expect("a Density of 0 is accepted", true, [](FogVolume& volume) { volume.Density = 0.0f; });
			expect("a HalfExtents of 0 is rejected", false, [](FogVolume& volume) { volume.HalfExtents = glm::vec3(1.0f, 0.0f, 1.0f); });
			expect("a negative HalfExtents is rejected", false, [](FogVolume& volume) { volume.HalfExtents = glm::vec3(1.0f, -1.0f, 1.0f); });
			expect("a NaN HalfExtents is rejected", false, [](FogVolume& volume) { volume.HalfExtents = glm::vec3(k_NaN, 1.0f, 1.0f); });
			expect("a NoiseScale of 0 is rejected", false, [](FogVolume& volume) { volume.NoiseScale = 0.0f; });
			expect("a NaN NoiseVelocity is rejected", false, [](FogVolume& volume) { volume.NoiseVelocity = glm::vec3(k_NaN, 0.0f, 0.0f); });
			expect("a negative EdgeFade is rejected", false, [](FogVolume& volume) { volume.EdgeFade = -0.5f; });
			expect("a negative LightIntensity is rejected", false, [](FogVolume& volume) { volume.LightIntensity = -1.0f; });
			expect("a negative ScatterTint is rejected", false, [](FogVolume& volume) { volume.ScatterTint = glm::vec3(1.0f, -0.1f, 1.0f); });
			expect("a negative MaxInScatter is rejected", false, [](FogVolume& volume) { volume.MaxInScatter = -1.0f; });
			expect("an unknown Shape is rejected", false, [](FogVolume& volume) { volume.Shape = static_cast<FogVolumeShape>(7); });
			expect("an unknown Mode is rejected", false, [](FogVolume& volume) { volume.Mode = static_cast<FogVolumeMode>(7); });
			expect("an Anisotropy past the limit is clamped, not rejected", true, [](FogVolume& volume) { volume.Anisotropy = 2.0f; });
			expect("an Albedo past 1 is clamped, not rejected", true, [](FogVolume& volume) { volume.Albedo = glm::vec3(2.0f); });
			expect("RaymarchSteps of 0 and of 500 are clamped, not rejected", true, [](FogVolume& volume) { volume.RaymarchSteps = 500; });

			glm::mat4 flat = placed;
			flat[1] = glm::vec4(0.0f);
			glm::mat4 perspective = placed;
			perspective[2][3] = 0.5f;
			glm::mat4 nan = placed;
			nan[3][0] = k_NaN;
			expectTransform("a transform with a flat axis is rejected", false, flat);
			expectTransform("a transform that is not affine is rejected", false, perspective);
			expectTransform("a transform with a NaN is rejected", false, nan);
			expectTransform("a rotated and scaled transform is accepted", true, glm::rotate(placed, 0.7f, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 3.0f)));
		});
		for (const auto& [name, passed] : results)
			Check(passed, "Fog volume 14: SubmitFogVolume " + name);
		Check(stats.FogVolumes <= 8, std::format("Fog volume 14b: the accepted volumes of one scene stay within the budget (kept {}, dropped {})", stats.FogVolumes, stats.FogVolumesDropped));
	}

	void FogVolumeTest::Cleanup()
	{
		m_DriftActive = false;
		m_Alive.reset();
		DestroyAndDelete(m_Box);
		DestroyAndDelete(m_Target);
		for (Renderer3D** renderer : { &m_Renderer, &m_Wide, &m_Off, &m_Probe })
		{
			if (*renderer)
			{
				(*renderer)->Shutdown();
				delete *renderer;
				*renderer = nullptr;
			}
		}
		m_Checks.clear();
	}

	void FogVolumeTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void FogVolumeTest::ImGuiRender()
	{
		ImGui::TextWrapped("Local fog volumes: the checks read the CPU records, the statistics and the froxel volumes of off-screen renderers, so this view stays empty.");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& result : m_Checks)
			ImGui::TextColored(result.Passed ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s %s", result.Passed ? "PASS" : "FAIL", result.Name.c_str());
	}

}
