#include "depch.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/PostProcessAccess.h"
#include "DingoEngine/Graphics/FullscreenPass.h"
#include "DingoEngine/Graphics/MaterialAccess.h"
#include "DingoEngine/Graphics/RendererBackend.h"
#include "DingoEngine/Graphics/VolumetricShaders.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Dingo
{

	namespace
	{

		TextureParams VolumeTextureParams(const char* name, const glm::uvec3& grid)
		{
			return TextureParams()
				.SetDebugName(name)
				.SetWidth(grid.x)
				.SetHeight(grid.y)
				.SetDepth(grid.z)
				.SetFormat(TextureFormat::RGBA16F)
				.SetDimension(TextureDimension::Texture3D)
				.SetIsStorage(true);
		}

		TextureParams RaymarchTextureParams(const char* name, TextureFormat format, const glm::uvec2& size)
		{
			return TextureParams()
				.SetDebugName(name)
				.SetWidth(size.x)
				.SetHeight(size.y)
				.SetFormat(format)
				.SetDimension(TextureDimension::Texture2D)
				.SetIsStorage(true);
		}

		glm::uvec2 RaymarchLowSize(uint32_t width, uint32_t height, uint32_t divisor)
		{
			return glm::uvec2((std::max)((width + divisor - 1) / divisor, 1u), (std::max)((height + divisor - 1) / divisor, 1u));
		}

		bool HasGrid(const Texture* texture, const glm::uvec3& grid)
		{
			const TextureParams& params = texture->GetParams();
			return params.Width == grid.x && params.Height == grid.y && params.Depth == grid.z;
		}

		float HaltonBase2(uint32_t index)
		{
			uint32_t bits = index;
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return static_cast<float>(bits) * 2.3283064365386963e-10f;
		}

		glm::dvec3 Unproject(const glm::dmat4& inverse, double x, double y, double z)
		{
			const glm::dvec4 point = inverse * glm::dvec4(x, y, z, 1.0);
			return glm::dvec3(point) / point.w;
		}

		// The direction through the corner (x, y) in clip space, scaled so that one unit along it is one
		// unit of view depth; false when the matrix is not a usable perspective projection.
		bool CornerRay(const glm::dmat4& viewProjection, const glm::dmat4& inverse, double x, double y, glm::vec4& ray)
		{
			const glm::dvec3 direction = Unproject(inverse, x, y, 0.5) - Unproject(inverse, x, y, 0.0);
			const glm::dvec3 depthRow(viewProjection[0][3], viewProjection[1][3], viewProjection[2][3]);
			const double alongDepth = glm::dot(depthRow, direction);
			if (!std::isfinite(alongDepth) || std::abs(alongDepth) < 1e-12)
				return false;
			const glm::dvec3 scaled = direction / alongDepth;
			if (!std::isfinite(scaled.x) || !std::isfinite(scaled.y) || !std::isfinite(scaled.z))
				return false;
			ray = glm::vec4(glm::vec3(scaled), 0.0f);
			return true;
		}

		uint32_t Groups(uint32_t size, uint32_t group)
		{
			return (size + group - 1) / group;
		}

	}

	void Renderer3D::InvalidateVolumetricHistory()
	{
		m_VolumetricHistoryFrame = ~0ull;
		m_FogVolumeHistoryTimeValid = false;
	}

	bool Renderer3D::CullFogVolumes(bool perspective)
	{
		m_FogVolumeFroxelCount = 0;
		m_FogVolumeRaymarchCount = 0;
		m_FogVolumeData.Global.w = 0.0f;
		if (!m_Params.Capabilities.Volumetrics || !perspective || m_FogVolumes.empty())
			return false;

		struct Candidate
		{
			uint32_t Index = 0;
			float Gap = 0.0f;
			bool Raymarch = false;
			bool FellBack = false;
		};

		const glm::vec3 camera(m_CameraData.CameraPosition);
		const Frustum frustum = Frustum::FromViewProjection(m_CameraData.ViewProjection);
		const float froxelMax = m_VolumetricFogSet ? m_VolumetricFog.MaxDistance : VolumetricFog{}.MaxDistance;
		bool raymarchKnown = false;
		bool raymarchAvailable = false;

		std::vector<Candidate> candidates;
		candidates.reserve(m_FogVolumes.size());
		uint32_t culled = 0;
		for (uint32_t index = 0; index < m_FogVolumes.size(); ++index)
		{
			const SubmittedFogVolume& submitted = m_FogVolumes[index];
			const glm::vec3 axisX = glm::vec3(submitted.World[0]) * submitted.Settings.HalfExtents.x;
			const glm::vec3 axisY = glm::vec3(submitted.World[1]) * submitted.Settings.HalfExtents.y;
			const glm::vec3 axisZ = glm::vec3(submitted.World[2]) * submitted.Settings.HalfExtents.z;
			const float radius = std::max({ glm::length(axisX + axisY + axisZ), glm::length(axisX + axisY - axisZ), glm::length(axisX - axisY + axisZ), glm::length(axisX - axisY - axisZ) });
			const glm::vec3 center(submitted.World[3]);
			if (!frustum.Intersects(center, radius))
			{
				++culled;
				continue;
			}

			Candidate candidate;
			candidate.Index = index;
			candidate.Gap = std::max(glm::distance(camera, center) - radius, 0.0f);
			candidate.Raymarch = submitted.Settings.Mode == FogVolumeMode::Raymarch;
			if (candidate.Raymarch)
			{
				if (!raymarchKnown)
				{
					PostProcessStack& post = Renderer::GetPostProcessStack();
					raymarchAvailable = post.IsActive() && post.GetSceneTarget() == Renderer::GetRenderTarget();
					raymarchKnown = true;
				}
				if (!raymarchAvailable)
				{
					candidate.Raymarch = false;
					candidate.FellBack = true;
				}
			}
			if (!candidate.Raymarch && candidate.Gap > froxelMax)
			{
				++culled;
				continue;
			}
			candidates.push_back(candidate);
		}

		std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.Gap < b.Gap; });

		const uint32_t budget = (std::min)(m_Params.Capabilities.MaxFogVolumes, k_MaxFogVolumes);
		std::vector<Candidate> kept;
		kept.reserve(candidates.size());
		uint32_t dropped = 0;
		uint32_t raymarchKept = 0;
		for (const Candidate& candidate : candidates)
		{
			if (kept.size() >= budget || (candidate.Raymarch && raymarchKept >= k_MaxRaymarchVolumes))
			{
				++dropped;
				continue;
			}
			raymarchKept += candidate.Raymarch ? 1u : 0u;
			kept.push_back(candidate);
		}

		if (dropped > 0 && !m_FogVolumeBudgetWarned)
		{
			DE_CORE_WARN("Renderer3D: a scene has more fog volumes in view than Capabilities.MaxFogVolumes ({}) or {} Raymarch ones; the ones farthest from the camera are dropped.", budget, k_MaxRaymarchVolumes);
			m_FogVolumeBudgetWarned = true;
		}

		uint32_t fallbacks = 0;
		for (const Candidate& candidate : kept)
			fallbacks += candidate.FellBack ? 1u : 0u;
		if (fallbacks > 0 && !m_RaymarchFallbackWarned)
		{
			DE_CORE_WARN("Renderer3D: a Raymarch fog volume is drawn as a Froxel one: its render target has no post-process scene target.");
			m_RaymarchFallbackWarned = true;
		}

		std::sort(kept.begin(), kept.end(), [](const Candidate& a, const Candidate& b) { return a.Raymarch != b.Raymarch ? !a.Raymarch : a.Index < b.Index; });

		const double time = m_FogVolumeTime;
		const double delta = time - m_FogVolumeHistoryTime;
		const bool driftUsable = m_FogVolumeHistoryTimeValid && delta >= 0.0 && delta <= 0.5;
		bool dropHistory = false;
		bool anyNoise = false;

		for (uint32_t slot = 0; slot < kept.size(); ++slot)
		{
			const Candidate& candidate = kept[slot];
			const SubmittedFogVolume& submitted = m_FogVolumes[candidate.Index];
			const FogVolume& settings = submitted.Settings;
			FogVolumeRecord& record = m_FogVolumeData.Volumes[slot];
			record = {};

			const glm::dmat4 model = glm::dmat4(submitted.World) * glm::scale(glm::dmat4(1.0), glm::dvec3(settings.HalfExtents));
			const glm::dmat4 inverse = glm::inverse(model);
			for (int row = 0; row < 3; ++row)
				record.WorldToLocal[row] = glm::vec4(static_cast<float>(inverse[0][row]), static_cast<float>(inverse[1][row]), static_cast<float>(inverse[2][row]), static_cast<float>(inverse[3][row]));

			const glm::vec3 worldHalf(settings.HalfExtents.x * glm::length(glm::vec3(submitted.World[0])), settings.HalfExtents.y * glm::length(glm::vec3(submitted.World[1])),
				settings.HalfExtents.z * glm::length(glm::vec3(submitted.World[2])));
			const bool noise = settings.NoiseMin != settings.NoiseMax;
			record.Shape = glm::vec4(worldHalf, settings.Shape == FogVolumeShape::Sphere ? 1.0f : 0.0f);
			record.Medium = glm::vec4(settings.Density, settings.Anisotropy, settings.EdgeFade, static_cast<float>(settings.RaymarchSteps));
			record.Albedo = glm::vec4(settings.Albedo, settings.AmbientIntensity);
			record.ScatterTint = glm::vec4(settings.ScatterTint, settings.OutsideShadowLight);
			record.Intensities = glm::vec4(settings.LightIntensity, settings.LocalLightIntensity, settings.MaxInScatter, noise ? 1.0f : 0.0f);
			record.HeightFade = glm::vec4(settings.HeightFadeStart, settings.HeightFadeEnd, settings.HeightBandBottom, settings.HeightBandTop);
			record.HeightBand = glm::vec4(settings.HeightBandMultiplier, settings.HeightBandSoftness, 0.0f, 0.0f);
			record.Fades = glm::vec4(settings.NearFadeStart, settings.NearFadeEnd, settings.FarFadeStart, settings.FarFadeEnd);
			record.FadeKeep = glm::vec4(settings.NearFadeKeep, settings.FarFadeKeep, settings.NoiseMin, settings.NoiseMax);
			record.Noise = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f / settings.NoiseScale);
			if (noise)
			{
				anyNoise = true;
				const glm::dvec3 velocity(settings.NoiseVelocity);
				const glm::dvec3 phase = velocity * time / static_cast<double>(settings.NoiseScale);
				record.Noise = glm::vec4(static_cast<float>(phase.x - std::floor(phase.x)), static_cast<float>(phase.y - std::floor(phase.y)),
					static_cast<float>(phase.z - std::floor(phase.z)), record.Noise.w);
				if (velocity != glm::dvec3(0.0))
				{
					if (driftUsable)
						record.Drift = glm::vec4(glm::vec3(velocity * delta), 0.0f);
					else if (m_FogVolumeHistoryTimeValid)
						dropHistory = true;
				}
			}
		}

		m_FogVolumeFroxelCount = static_cast<uint32_t>(kept.size()) - raymarchKept;
		m_FogVolumeRaymarchCount = raymarchKept;
		m_FogVolumeData.Global.w = anyNoise ? 1.0f : 0.0f;
		m_Statistics.FogVolumes = static_cast<uint32_t>(kept.size());
		m_Statistics.RaymarchVolumes = raymarchKept;
		m_Statistics.FogVolumesCulled = culled;
		m_Statistics.FogVolumesDropped = dropped;
		m_Statistics.RaymarchFallbacks = fallbacks;
		return dropHistory;
	}

	bool Renderer3D::EnsureVolumetricResources()
	{
		const glm::uvec3 requested = m_Params.Capabilities.VolumetricGrid;
		const glm::uvec3 grid(std::clamp(requested.x, 8u, 256u), std::clamp(requested.y, 8u, 256u), std::clamp(requested.z, 8u, 128u));

		Shader* inject = Internal::GetVolumetricShader(Internal::VolumetricShaderKind::Inject);
		Shader* integrate = Internal::GetVolumetricShader(Internal::VolumetricShaderKind::Integrate);
		if (!inject || !integrate)
		{
			if (!m_VolumetricWarned)
				DE_CORE_ERROR("Renderer3D: the volumetric fog shaders are not valid; scenes draw without volumetric fog until they are.");
			m_VolumetricWarned = true;
			return false;
		}

		if (!m_VolumetricScatter[0])
		{

			m_VolumetricScatter[0] = Texture::Create(VolumeTextureParams("Renderer3D_VolumetricScatter0", grid));
			m_VolumetricScatter[1] = Texture::Create(VolumeTextureParams("Renderer3D_VolumetricScatter1", grid));
			if (!m_VolumetricScatter[0] || !m_VolumetricScatter[1])
			{
				if (!m_VolumetricWarned)
					DE_CORE_ERROR("Renderer3D: the volumetric fog textures could not be created; scenes draw without volumetric fog.");
				m_VolumetricWarned = true;
				DestroyVolumetricResources();
				return false;
			}

			for (uint32_t i = 0; i < 2; ++i)
			{
				ComputePass* injectPass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D volumetric inject").SetShader(inject));
				injectPass->SetUniformBuffer(0, m_SceneUniformBuffer);
				injectPass->SetUniformBuffer(1, m_VolumetricDataBuffer);
				injectPass->SetUniformBuffer(2, m_ShadowDataBuffer);
				injectPass->SetSampler(4, m_ShadowSampler);
				injectPass->SetTexture(5, m_VolumetricScatter[1 - i]);
				injectPass->SetSampler(6, Renderer::GetClampSampler());
				injectPass->SetStorageTexture(7, m_VolumetricScatter[i]);
				injectPass->SetUniformBuffer(8, m_FogVolumeBuffer);
				injectPass->SetTexture(9, m_VolumeNoise);
				injectPass->SetSampler(10, m_VolumeNoiseSampler);
				m_VolumetricInject[i] = injectPass;

				ComputePass* integratePass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D volumetric integrate").SetShader(integrate));
				integratePass->SetUniformBuffer(1, m_VolumetricDataBuffer);
				integratePass->SetUniformBuffer(8, m_FogVolumeBuffer);
				integratePass->SetTexture(5, m_VolumetricScatter[i]);
				integratePass->SetStorageTexture(7, m_VolumetricIntegrated);
				m_VolumetricIntegrate[i] = integratePass;
			}
			m_VolumetricAtlasId = 0;
		}
		else if (!HasGrid(m_VolumetricScatter[0], grid))
		{
			m_VolumetricScatter[0]->Reinitialize(VolumeTextureParams("Renderer3D_VolumetricScatter0", grid));
			m_VolumetricScatter[1]->Reinitialize(VolumeTextureParams("Renderer3D_VolumetricScatter1", grid));
			InvalidateVolumetricHistory();
		}

		if (!HasGrid(m_VolumetricIntegrated, grid))
			m_VolumetricIntegrated->Reinitialize(VolumeTextureParams("Renderer3D_VolumetricIntegrated", grid));
		return true;
	}

	void Renderer3D::DestroyVolumetricResources()
	{
		for (ComputePass*& pass : m_VolumetricInject)
			DestroyAndDelete(pass);
		for (ComputePass*& pass : m_VolumetricIntegrate)
			DestroyAndDelete(pass);
		for (Texture*& texture : m_VolumetricScatter)
			DestroyAndDelete(texture);
		DestroyAndDelete(m_VolumetricSlicePass);
		DestroyAndDelete(m_VolumetricSlice);
		DestroyAndDelete(m_VolumetricSliceBuffer);
		m_VolumetricSliceSourceId = 0;
		m_VolumetricAtlas = nullptr;
		m_VolumetricAtlasId = 0;
		m_VolumetricScene = false;
		InvalidateVolumetricHistory();
	}

	void Renderer3D::PrepareVolumetrics(bool perspective)
	{
		const bool dropVolumeHistory = CullFogVolumes(perspective);
		m_FogVolumeData.Global.z = static_cast<float>(m_VolumetricDebugView);
		m_VolumetricScene = false;
		m_RaymarchScene = false;
		m_FogVolumeUploaded = false;
		m_CameraData.VolumeParams = glm::vec4(0.0f);
		m_CameraData.VolumeGrid = glm::vec4(0.0f);
		m_SceneVolumeParams = glm::vec4(0.0f);
		m_SceneVolumeGrid = glm::vec4(0.0f);
		PrepareFroxelFog(perspective, dropVolumeHistory);
		PrepareRaymarch(perspective);
	}

	void Renderer3D::PrepareFroxelFog(bool perspective, bool dropVolumeHistory)
	{
		if (!m_Params.Capabilities.Volumetrics || !perspective || (!m_VolumetricFogSet && m_FogVolumeFroxelCount == 0))
			return;
		if (!EnsureVolumetricResources())
			return;

		const glm::dmat4 viewProjection(m_CameraData.ViewProjection);
		const glm::dmat4 inverse = glm::inverse(viewProjection);
		VolumetricData data;
		if (!CornerRay(viewProjection, inverse, -1.0, 1.0, data.FrustumRays[0]) || !CornerRay(viewProjection, inverse, 1.0, 1.0, data.FrustumRays[1])
			|| !CornerRay(viewProjection, inverse, -1.0, -1.0, data.FrustumRays[2]) || !CornerRay(viewProjection, inverse, 1.0, -1.0, data.FrustumRays[3]))
			return;

		VolumetricFog fog = m_VolumetricFogSet ? m_VolumetricFog : VolumetricFog{};
		if (!m_VolumetricFogSet)
			fog.Density = 0.0f;
		m_FogVolumeData.Global = glm::vec4(fog.LocalLightIntensity, fog.MaxInScatter, static_cast<float>(m_VolumetricDebugView), m_FogVolumeData.Global.w);
		const glm::uvec3 grid = glm::uvec3(m_VolumetricScatter[0]->GetParams().Width, m_VolumetricScatter[0]->GetParams().Height, m_VolumetricScatter[0]->GetParams().Depth);
		const uint64_t frame = Renderer::GetFrameIndex();
		const bool temporal = fog.Temporal && m_VolumetricTemporalFrame != frame;
		const glm::vec2 range(fog.NearDistance, fog.MaxDistance);
		const bool historyValid = temporal && m_VolumetricHistoryFrame != ~0ull && m_VolumetricHistoryFrame + 1 == frame
			&& m_VolumetricHistoryGrid == grid && m_VolumetricHistoryRange == range && !dropVolumeHistory;

		data.CameraPosition = glm::vec4(glm::vec3(m_CameraData.CameraPosition), 1.0f);
		data.PreviousViewProjection = m_VolumetricPreviousViewProjection;
		data.Grid = glm::vec4(glm::vec3(grid), historyValid ? 1.0f : 0.0f);
		const float jitter = temporal ? HaltonBase2(static_cast<uint32_t>(frame % 8) + 1u) : 0.5f;
		data.Slices = glm::vec4(fog.NearDistance, fog.MaxDistance, std::log2(fog.MaxDistance / fog.NearDistance), jitter);
		data.Medium = glm::vec4(fog.Density, fog.BaseHeight, fog.HeightFalloff, fog.Anisotropy);
		data.Albedo = glm::vec4(fog.Albedo, historyValid ? k_VolumetricHistoryBlend : 1.0f);
		const int localLights = m_CameraData.LightCounts.y;
		bool localMedium = m_VolumetricFogSet && fog.Density > 0.0f && fog.LocalLightIntensity > 0.0f;
		for (uint32_t volume = 0; !localMedium && fog.LocalLightIntensity > 0.0f && volume < m_FogVolumeFroxelCount; ++volume)
			localMedium = m_FogVolumeData.Volumes[volume].Intensities.y > 0.0f;
		bool localScatter = false;
		for (int slot = 0; localMedium && !localScatter && slot < localLights; ++slot)
			localScatter = m_FogVolumeData.LocalScatter[slot / 4][slot % 4] > 0.0f;
		data.Intensities = glm::vec4(fog.AmbientIntensity, fog.LightIntensity, static_cast<float>(m_FogVolumeFroxelCount), localScatter ? static_cast<float>(localLights) : 0.0f);
		data.ScatterTint = glm::vec4(fog.ScatterTint, fog.OutsideShadowLight);
		m_VolumetricData = data;

		m_VolumetricPass = 1 - m_VolumetricHistory;
		if (temporal)
		{
			m_VolumetricHistory = m_VolumetricPass;
			m_VolumetricHistoryFrame = frame;
			m_VolumetricTemporalFrame = frame;
			m_VolumetricPreviousViewProjection = m_CameraData.ViewProjection;
			m_VolumetricHistoryGrid = grid;
			m_VolumetricHistoryRange = range;
			m_FogVolumeHistoryTime = m_FogVolumeTime;
			m_FogVolumeHistoryTimeValid = true;
		}

		m_VolumetricScene = true;
		m_Statistics.Volumetrics = true;
		m_Statistics.VolumetricTemporal = historyValid;
		if (m_VolumetricFogSet)
		{
			m_CameraData.FogColor = glm::vec4(0.0f);
			m_CameraData.FogParams = glm::vec4(0.0f);
		}

		const float firstSliceDepth = fog.NearDistance * std::pow(fog.MaxDistance / fog.NearDistance, 1.0f / static_cast<float>(grid.z));
		m_CameraData.VolumeParams = glm::vec4(1.0f, fog.NearDistance, 1.0f / data.Slices.z, fog.MaxDistance);
		m_CameraData.VolumeGrid = glm::vec4(glm::vec3(grid), 1.0f / firstSliceDepth);
		m_SceneVolumeParams = m_CameraData.VolumeParams;
		m_SceneVolumeGrid = m_CameraData.VolumeGrid;
	}

	void Renderer3D::DispatchVolumetrics(Texture* shadowAtlas)
	{
		if (!m_VolumetricScene)
			return;
		m_VolumetricScene = false;

		const uint64_t atlasId = shadowAtlas->GetId();
		if (atlasId != m_VolumetricAtlasId)
		{
			for (ComputePass* pass : m_VolumetricInject)
				pass->SetTexture(3, shadowAtlas);
			m_VolumetricAtlas = shadowAtlas;
			m_VolumetricAtlasId = atlasId;
		}

		Renderer::Upload(m_VolumetricDataBuffer, &m_VolumetricData, sizeof(VolumetricData));
		Renderer::Upload(m_FogVolumeBuffer, &m_FogVolumeData, sizeof(FogVolumeData));
		m_FogVolumeUploaded = true;
		const glm::uvec3 grid = glm::uvec3(m_VolumetricData.Grid);

		DE_PROFILE_SCOPE("Renderer3D::DispatchVolumetrics");
		Renderer::BeginGpuTimer("Volumetrics");
		const bool injected = Renderer::Dispatch(m_VolumetricInject[m_VolumetricPass], Groups(grid.x, 4), Groups(grid.y, 4), Groups(grid.z, 4));
		const bool integrated = injected && Renderer::Dispatch(m_VolumetricIntegrate[m_VolumetricPass], Groups(grid.x, 8), Groups(grid.y, 8), 1);
		Renderer::EndGpuTimer();
		if (!integrated)
		{
			if (!m_VolumetricDispatchWarned)
				DE_CORE_ERROR("Renderer3D: recording the volumetric fog dispatches failed.");
			m_VolumetricDispatchWarned = true;
			InvalidateVolumetricHistory();
			// The scene samples the volume anyway (the camera data is already uploaded), and after a
			// create or a grid change it holds undefined memory: no in-scatter, full transmittance.
			if (Internal::CommandList* list = Internal::RendererBackend::TryGetRecordingCommandList())
				list->ClearTexture(m_VolumetricIntegrated, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			return;
		}
		m_Statistics.VolumetricDispatches += 2;
		DispatchVolumetricSlice(grid);
	}

	void Renderer3D::DispatchVolumetricSlice(const glm::uvec3& grid)
	{
		if (!m_SliceRequest.Active)
			return;
		const VolumetricSliceRequest request = m_SliceRequest;
		m_SliceRequest.Active = false;

		Shader* kernel = Internal::GetVolumetricShader(Internal::VolumetricShaderKind::Slice);
		if (!kernel)
		{
			if (!m_VolumetricSliceWarned)
				DE_CORE_ERROR("Renderer3D: the volumetric slice shader is not valid; the debug slice view stays empty until it is.");
			m_VolumetricSliceWarned = true;
			return;
		}

		const glm::uvec2 size(grid.x, grid.y);
		if (!m_VolumetricSlice)
		{
			m_VolumetricSlice = Texture::Create(RaymarchTextureParams("Renderer3D_VolumetricSlice", TextureFormat::RGBA16F, size));
			m_VolumetricSliceBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
				.SetDebugName("Renderer3D_VolumetricSliceData")
				.SetByteSize(sizeof(glm::vec4))
				.SetType(BufferType::UniformBuffer)
				.SetIsVolatile(true)
				.SetUpload(BufferUpload::Deferred)
				.SetMaxWritesPerFrame(k_MaxScenesPerFrame));
			if (!m_VolumetricSlice || !m_VolumetricSliceBuffer)
			{
				if (!m_VolumetricSliceWarned)
					DE_CORE_ERROR("Renderer3D: the volumetric slice texture could not be created; the debug slice view stays empty.");
				m_VolumetricSliceWarned = true;
				DestroyAndDelete(m_VolumetricSlice);
				DestroyAndDelete(m_VolumetricSliceBuffer);
				return;
			}
			m_VolumetricSlicePass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D volumetric slice").SetShader(kernel));
			m_VolumetricSlicePass->SetUniformBuffer(1, m_VolumetricSliceBuffer);
			m_VolumetricSlicePass->SetStorageTexture(7, m_VolumetricSlice);
			m_VolumetricSliceSourceId = 0;
		}
		else if (m_VolumetricSlice->GetParams().Width != size.x || m_VolumetricSlice->GetParams().Height != size.y)
			m_VolumetricSlice->Reinitialize(RaymarchTextureParams("Renderer3D_VolumetricSlice", TextureFormat::RGBA16F, size));

		Texture* source = request.Scatter ? m_VolumetricScatter[m_VolumetricPass] : m_VolumetricIntegrated;
		if (source->GetId() != m_VolumetricSliceSourceId)
		{
			m_VolumetricSlicePass->SetTexture(5, source);
			m_VolumetricSliceSourceId = source->GetId();
		}

		const glm::vec4 params(static_cast<float>(request.Slice), request.Exposure, request.Alpha ? 1.0f : 0.0f, 0.0f);
		Renderer::Upload(m_VolumetricSliceBuffer, &params, sizeof(params));
		if (!Renderer::Dispatch(m_VolumetricSlicePass, Groups(size.x, 8), Groups(size.y, 8), 1) && !m_VolumetricSliceWarned)
		{
			DE_CORE_ERROR("Renderer3D: recording the volumetric slice dispatch failed.");
			m_VolumetricSliceWarned = true;
		}
	}

	void Renderer3D::PrepareRaymarch(bool perspective)
	{
		if (!perspective || m_FogVolumeRaymarchCount == 0)
			return;
		const Framebuffer* target = Renderer::GetRenderTarget();
		if (!target || target->GetWidth() == 0 || target->GetHeight() == 0 || !EnsureRaymarchResources())
			return;

		const uint32_t width = target->GetWidth();
		const uint32_t height = target->GetHeight();
		const glm::vec2 low(RaymarchLowSize(width, height, m_RaymarchDivisor));
		const glm::mat4 inverse(glm::inverse(glm::dmat4(m_CameraData.ViewProjection)));
		const glm::vec4 camera(glm::vec3(m_CameraData.CameraPosition), 1.0f);

		m_RaymarchData.InverseViewProjection = inverse;
		m_RaymarchData.CameraPosition = camera;
		m_RaymarchData.Target = glm::vec4(low, static_cast<float>(width), static_cast<float>(height));
		m_RaymarchData.Params = glm::vec4(static_cast<float>(m_FogVolumeFroxelCount), static_cast<float>(m_FogVolumeRaymarchCount),
			static_cast<float>(m_RaymarchDivisor), m_VolumetricScene ? 1.0f : 0.0f);
		m_RaymarchData.Reserved = glm::vec4(0.0f);

		m_RaymarchCompositeData.InverseViewProjection = inverse;
		m_RaymarchCompositeData.CameraPosition = camera;
		m_RaymarchCompositeData.Low = glm::vec4(low, static_cast<float>(width) / static_cast<float>(m_RaymarchDivisor), static_cast<float>(height) / static_cast<float>(m_RaymarchDivisor));
		m_RaymarchScene = true;
	}

	void Renderer3D::UploadRaymarchResources()
	{
		if (!m_RaymarchScene)
			return;
		if (!m_FogVolumeUploaded)
		{
			Renderer::Upload(m_FogVolumeBuffer, &m_FogVolumeData, sizeof(FogVolumeData));
			m_FogVolumeUploaded = true;
		}
		Renderer::Upload(m_RaymarchDataBuffer, &m_RaymarchData, sizeof(RaymarchData));
	}

	bool Renderer3D::EnsureRaymarchResources()
	{
		const uint32_t divisor = m_Params.Capabilities.VolumeRaymarchResolution == RaymarchResolution::Quarter ? 4u : 2u;

		Shader* kernel = Internal::GetVolumetricShader(Internal::VolumetricShaderKind::Raymarch);
		if (!m_RaymarchCompositeShader)
			m_RaymarchCompositeShader = new Internal::FullscreenShader("VolumeRaymarchComposite", "VolumeRaymarchComposite.glsl");
		Shader* composite = m_RaymarchCompositeShader->GetShader();
		if (!kernel || !composite || !composite->IsValid())
		{
			if (!m_RaymarchWarned)
				DE_CORE_ERROR("Renderer3D: the Raymarch fog volume shaders are not valid; Raymarch volumes are not drawn until they are.");
			m_RaymarchWarned = true;
			return false;
		}

		if (!m_RaymarchPass)
		{
			m_RaymarchDataBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
				.SetDebugName("Renderer3D_RaymarchData")
				.SetByteSize(sizeof(RaymarchData))
				.SetType(BufferType::UniformBuffer)
				.SetIsVolatile(true)
				.SetUpload(BufferUpload::Deferred)
				.SetMaxWritesPerFrame(k_MaxScenesPerFrame));
			if (!m_RaymarchDataBuffer)
			{
				if (!m_RaymarchWarned)
					DE_CORE_ERROR("Renderer3D: the Raymarch fog volume buffer could not be created; Raymarch volumes are not drawn.");
				m_RaymarchWarned = true;
				DestroyRaymarchResources();
				return false;
			}

			m_RaymarchPass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D volume raymarch").SetShader(kernel));
			m_RaymarchPass->SetUniformBuffer(0, m_SceneUniformBuffer);
			m_RaymarchPass->SetUniformBuffer(1, m_RaymarchDataBuffer);
			m_RaymarchPass->SetUniformBuffer(2, m_ShadowDataBuffer);
			m_RaymarchPass->SetSampler(4, m_ShadowSampler);
			m_RaymarchPass->SetUniformBuffer(8, m_FogVolumeBuffer);
			m_RaymarchPass->SetTexture(9, m_VolumeNoise);
			m_RaymarchPass->SetSampler(10, m_VolumeNoiseSampler);
			m_RaymarchPass->SetTexture(11, m_VolumetricIntegrated);
			m_RaymarchPass->SetSampler(12, Renderer::GetClampSampler());

			m_RaymarchComposite = Material::Create(MaterialParams()
				.SetDebugName("Renderer3D volume raymarch composite")
				.SetShader(composite)
				.SetCullMode(CullMode::None)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendMode(BlendMode::Alpha)
				.SetUniformWritesPerFrame(k_MaxScenesPerFrame));
			m_RaymarchComposite->SetSampler(0, Renderer::GetPointSampler());
			m_RaymarchComposite->SetSampler(1, Renderer::GetPointSampler());
			m_RaymarchComposite->SetSampler(2, Renderer::GetPointSampler());
			m_RaymarchAtlasId = 0;
			m_RaymarchBoundDepthId = 0;
		}

		m_RaymarchDivisor = divisor;
		return true;
	}

	void Renderer3D::DestroyRaymarchResources()
	{
		DestroyAndDelete(m_RaymarchPass);
		DestroyAndDelete(m_RaymarchComposite);
		for (RaymarchTarget& target : m_RaymarchTargets)
		{
			DestroyAndDelete(target.Scatter);
			DestroyAndDelete(target.Depth);
		}
		m_RaymarchTargets.clear();
		m_RaymarchScatter = nullptr;
		m_RaymarchDepth = nullptr;
		DestroyAndDelete(m_RaymarchDataBuffer);
		m_RaymarchAtlasId = 0;
		m_RaymarchBoundDepthId = 0;
		m_RaymarchScene = false;
	}

	Renderer3D::RaymarchTarget* Renderer3D::AcquireRaymarchTarget(uint64_t depthId, const glm::uvec2& low)
	{
		const uint64_t frame = Renderer::GetFrameIndex();
		for (auto it = m_RaymarchTargets.begin(); it != m_RaymarchTargets.end();)
		{
			if (it->DepthId != depthId && frame > it->LastFrame + k_RaymarchIdleFrames)
			{
				if (it->DepthId == m_RaymarchBoundDepthId)
					m_RaymarchBoundDepthId = 0;
				if (it->Scatter == m_RaymarchScatter)
				{
					m_RaymarchScatter = nullptr;
					m_RaymarchDepth = nullptr;
				}
				DestroyAndDelete(it->Scatter);
				DestroyAndDelete(it->Depth);
				it = m_RaymarchTargets.erase(it);
				continue;
			}
			++it;
		}

		for (RaymarchTarget& target : m_RaymarchTargets)
		{
			if (target.DepthId != depthId)
				continue;
			target.LastFrame = frame;
			if (target.Scatter->GetParams().Width != low.x || target.Scatter->GetParams().Height != low.y)
			{
				target.Scatter->Reinitialize(RaymarchTextureParams("Renderer3D_RaymarchScatter", TextureFormat::RGBA16F, low));
				target.Depth->Reinitialize(RaymarchTextureParams("Renderer3D_RaymarchDepth", TextureFormat::R32F, low));
			}
			return &target;
		}

		RaymarchTarget created;
		created.Scatter = Texture::Create(RaymarchTextureParams("Renderer3D_RaymarchScatter", TextureFormat::RGBA16F, low));
		created.Depth = Texture::Create(RaymarchTextureParams("Renderer3D_RaymarchDepth", TextureFormat::R32F, low));
		if (!created.Scatter || !created.Depth)
		{
			if (!m_RaymarchWarned)
				DE_CORE_ERROR("Renderer3D: the Raymarch fog volume textures could not be created; Raymarch volumes are not drawn.");
			m_RaymarchWarned = true;
			DestroyAndDelete(created.Scatter);
			DestroyAndDelete(created.Depth);
			return nullptr;
		}
		created.DepthId = depthId;
		created.LastFrame = frame;
		m_RaymarchTargets.push_back(created);
		return &m_RaymarchTargets.back();
	}

	void Renderer3D::DrawVolumeRaymarch(Texture* shadowAtlas)
	{
		if (!m_RaymarchScene)
			return;
		m_RaymarchScene = false;

		DE_PROFILE_SCOPE("Renderer3D::DrawVolumeRaymarch");
		PostProcessStack& post = Renderer::GetPostProcessStack();
		const bool depthCopied = Internal::PostProcessAccess::ApplyAmbientOcclusion(post);
		Renderer::BeginGpuTimer("VolumeRaymarch");
		Texture* sceneDepth = Internal::PostProcessAccess::RefreshSceneDepth(post, depthCopied);
		const glm::uvec2 low(static_cast<uint32_t>(m_RaymarchData.Target.x), static_cast<uint32_t>(m_RaymarchData.Target.y));
		RaymarchTarget* target = sceneDepth ? AcquireRaymarchTarget(sceneDepth->GetId(), low) : nullptr;
		if (target)
		{
			if (target->DepthId != m_RaymarchBoundDepthId)
			{
				m_RaymarchPass->SetTexture(5, sceneDepth);
				m_RaymarchPass->SetStorageTexture(6, target->Depth);
				m_RaymarchPass->SetStorageTexture(7, target->Scatter);
				m_RaymarchComposite->SetTexture(0, target->Scatter);
				m_RaymarchComposite->SetTexture(1, target->Depth);
				m_RaymarchComposite->SetTexture(2, sceneDepth);
				m_RaymarchBoundDepthId = target->DepthId;
			}
			m_RaymarchScatter = target->Scatter;
			m_RaymarchDepth = target->Depth;
			const uint64_t atlasId = shadowAtlas->GetId();
			if (atlasId != m_RaymarchAtlasId)
			{
				m_RaymarchPass->SetTexture(3, shadowAtlas);
				m_RaymarchAtlasId = atlasId;
			}

			if (Renderer::Dispatch(m_RaymarchPass, Groups(low.x, 8), Groups(low.y, 8), 1))
			{
				++m_Statistics.RaymarchDispatches;
				const std::vector<uint8_t>& current = Internal::MaterialAccess::GetUniformCPUData(m_RaymarchComposite);
				if (current.size() != sizeof(RaymarchCompositeData) || std::memcmp(current.data(), &m_RaymarchCompositeData, sizeof(RaymarchCompositeData)) != 0)
					m_RaymarchComposite->SetUniform(m_RaymarchCompositeData);
				Internal::DrawFullscreen(m_RaymarchComposite, Renderer::GetRenderTarget());
			}
			else if (!m_RaymarchWarned)
			{
				DE_CORE_ERROR("Renderer3D: recording the Raymarch fog volume dispatch failed.");
				m_RaymarchWarned = true;
			}
		}
		Renderer::EndGpuTimer();
	}

}
