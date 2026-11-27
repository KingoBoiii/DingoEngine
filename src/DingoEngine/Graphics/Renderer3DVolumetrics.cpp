#include "depch.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Graphics/VolumetricShaders.h"

#include <glm/gtc/matrix_access.hpp>

#include <cmath>

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
				m_VolumetricInject[i] = injectPass;

				ComputePass* integratePass = ComputePass::Create(ComputePassParams().SetDebugName("Renderer3D volumetric integrate").SetShader(integrate));
				integratePass->SetUniformBuffer(1, m_VolumetricDataBuffer);
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
		m_VolumetricAtlas = nullptr;
		m_VolumetricAtlasId = 0;
		m_VolumetricScene = false;
		InvalidateVolumetricHistory();
	}

	void Renderer3D::PrepareVolumetrics(bool perspective)
	{
		m_VolumetricScene = false;
		m_CameraData.VolumeParams = glm::vec4(0.0f);
		m_CameraData.VolumeGrid = glm::vec4(0.0f);
		m_SceneVolumeParams = glm::vec4(0.0f);
		m_SceneVolumeGrid = glm::vec4(0.0f);
		if (!m_Params.Capabilities.Volumetrics || !m_VolumetricFogSet || !perspective)
			return;
		if (!EnsureVolumetricResources())
			return;

		const glm::dmat4 viewProjection(m_CameraData.ViewProjection);
		const glm::dmat4 inverse = glm::inverse(viewProjection);
		VolumetricData data;
		if (!CornerRay(viewProjection, inverse, -1.0, 1.0, data.FrustumRays[0]) || !CornerRay(viewProjection, inverse, 1.0, 1.0, data.FrustumRays[1])
			|| !CornerRay(viewProjection, inverse, -1.0, -1.0, data.FrustumRays[2]) || !CornerRay(viewProjection, inverse, 1.0, -1.0, data.FrustumRays[3]))
			return;

		const VolumetricFog& fog = m_VolumetricFog;
		const glm::uvec3 grid = glm::uvec3(m_VolumetricScatter[0]->GetParams().Width, m_VolumetricScatter[0]->GetParams().Height, m_VolumetricScatter[0]->GetParams().Depth);
		const uint64_t frame = Renderer::GetFrameIndex();
		const bool temporal = fog.Temporal && m_VolumetricTemporalFrame != frame;
		const glm::vec2 range(fog.NearDistance, fog.MaxDistance);
		const bool historyValid = temporal && m_VolumetricHistoryFrame != ~0ull && m_VolumetricHistoryFrame + 1 == frame
			&& m_VolumetricHistoryGrid == grid && m_VolumetricHistoryRange == range;

		data.CameraPosition = glm::vec4(glm::vec3(m_CameraData.CameraPosition), 1.0f);
		data.PreviousViewProjection = m_VolumetricPreviousViewProjection;
		data.Grid = glm::vec4(glm::vec3(grid), historyValid ? 1.0f : 0.0f);
		const float jitter = temporal ? HaltonBase2(static_cast<uint32_t>(frame % 8) + 1u) : 0.5f;
		data.Slices = glm::vec4(fog.NearDistance, fog.MaxDistance, std::log2(fog.MaxDistance / fog.NearDistance), jitter);
		data.Medium = glm::vec4(fog.Density, fog.BaseHeight, fog.HeightFalloff, fog.Anisotropy);
		data.Albedo = glm::vec4(fog.Albedo, historyValid ? k_VolumetricHistoryBlend : 1.0f);
		data.Intensities = glm::vec4(fog.AmbientIntensity, fog.LightIntensity, 0.0f, 0.0f);
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
		}

		m_VolumetricScene = true;
		m_Statistics.Volumetrics = true;
		m_Statistics.VolumetricTemporal = historyValid;
		m_CameraData.FogColor = glm::vec4(0.0f);
		m_CameraData.FogParams = glm::vec4(0.0f);

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
		const glm::uvec3 grid = glm::uvec3(m_VolumetricData.Grid);

		DE_PROFILE_SCOPE("Renderer3D::DispatchVolumetrics");
		Renderer::BeginGpuTimer("Volumetrics");
		const bool injected = Renderer::Dispatch(m_VolumetricInject[m_VolumetricPass], Groups(grid.x, 4), Groups(grid.y, 4), Groups(grid.z, 4));
		const bool integrated = injected && Renderer::Dispatch(m_VolumetricIntegrate[m_VolumetricPass], Groups(grid.x, 8), Groups(grid.y, 8), 1);
		Renderer::EndGpuTimer();
		if (!integrated)
		{
			if (!m_VolumetricWarned)
				DE_CORE_ERROR("Renderer3D: recording the volumetric fog dispatches failed.");
			m_VolumetricWarned = true;
			InvalidateVolumetricHistory();
			return;
		}
		m_Statistics.VolumetricDispatches += 2;
	}

}
