#include "depch.h"
#include "DingoEngine/Graphics/PbrShaders.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/LightMath.h"
#include "DingoEngine/Graphics/MaterialAccess.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
	constexpr size_t k_VariantCount = 3;
	constexpr uint32_t k_NormalSlot = static_cast<uint32_t>(Dingo::PbrTextureSlot::Normal);

	// A variant that failed to compile stays here, invalid and watched, so it is neither rebuilt nor
	// logged again on every call and a hot reload that fixes it makes it usable.
	std::array<Dingo::Shader*, k_VariantCount> s_Shaders = {};
	std::array<bool, k_VariantCount> s_Attempted = {};
	Dingo::Sampler* s_Sampler = nullptr;

	float FiniteOr(float value, float fallback)
	{
		return std::isfinite(value) ? value : fallback;
	}
}

namespace Dingo::Internal
{

	Shader* GetPbrShader(PbrShaderVariant variant)
	{
		const size_t index = static_cast<size_t>(variant);
		Shader*& shader = s_Shaders[index];
		if (s_Attempted[index])
			return shader && shader->IsValid() ? shader : nullptr;
		s_Attempted[index] = true;

		ShaderParams params;
		switch (variant)
		{
		case PbrShaderVariant::Static:    params.SetName("Renderer3DPbrMeshShader"); break;
		case PbrShaderVariant::Skinned:   params.SetName("Renderer3DPbrSkinnedMeshShader").AddDefine("DE_SKINNED"); break;
		case PbrShaderVariant::Instanced: params.SetName("Renderer3DPbrInstancedMeshShader").AddDefine("DE_INSTANCED"); break;
		}
		shader = CreateEngineShader(params, "Renderer3D_Pbr.glsl");
		if (shader)
			WatchUnmanagedShader(shader);
		return shader && shader->IsValid() ? shader : nullptr;
	}

	bool IsPbrShader(const Shader* shader)
	{
		return shader && shader->IsValid() && std::find(s_Shaders.begin(), s_Shaders.end(), shader) != s_Shaders.end();
	}

	Sampler* GetPbrSampler()
	{
		if (!s_Sampler)
		{
			s_Sampler = Sampler::Create(SamplerParams()
				.SetAddressMode(SamplerAddressMode::Repeat)
				.SetMaxAnisotropy(8.0f));
		}
		return s_Sampler;
	}

	void PreparePbrMaterial(Material* material)
	{
		Texture* white = Renderer::GetWhiteTexture();
		const bool hasNormalMap = material->GetTexture(k_NormalSlot) && material->GetTexture(k_NormalSlot) != white;
		for (uint32_t slot = 0; slot < Renderer3D::k_PbrTextureSlots; ++slot)
		{
			if (!material->GetTexture(slot))
				material->SetTexture(slot, white);
			if (!material->GetSampler(slot))
				material->SetSampler(slot, GetPbrSampler());
		}

		const BlendMode blend = material->GetParams().BlendMode;
		const int32_t fogTarget = blend == BlendMode::Additive ? 1 : blend == BlendMode::Multiply ? 2 : 0;

		const glm::vec4& baseColor = material->GetBaseColor();
		const glm::vec3& emissive = material->GetEmissiveColor();

		PbrMaterialData data;
		data.BaseColor = IsFinite(glm::vec3(baseColor)) && std::isfinite(baseColor.a) ? baseColor : glm::vec4(1.0f);
		data.Emissive = glm::vec4(IsFinite(emissive) ? emissive : glm::vec3(0.0f), FiniteOr(material->GetEmissiveStrength(), 0.0f));
		data.Surface = glm::vec4(
			glm::clamp(FiniteOr(material->GetMetallic(), 0.0f), 0.0f, 1.0f),
			glm::clamp(FiniteOr(material->GetRoughness(), 0.5f), 0.0f, 1.0f),
			glm::clamp(FiniteOr(material->GetOcclusion(), 1.0f), 0.0f, 1.0f),
			FiniteOr(material->GetNormalScale(), 1.0f));
		data.Flags = glm::ivec4(fogTarget, material->IsTranslucent() ? 1 : 0, hasNormalMap ? 1 : 0, 0);

		const std::vector<uint8_t>& current = MaterialAccess::GetUniformCPUData(material);
		if (current.size() == sizeof(data) && std::memcmp(current.data(), &data, sizeof(data)) == 0)
			return;
		material->SetUniform(data);
	}

	Material* CreatePbrMaterial(const PbrMaterialParams& params)
	{
		Shader* shader = GetPbrShader(PbrShaderVariant::Static);
		if (!shader)
		{
			DE_CORE_ERROR("CreatePbrMaterial: the PBR shader failed to compile.");
			return nullptr;
		}

		MaterialParams material = params.Material;
		material.SetShader(shader)
			.SetBaseColor(params.BaseColor)
			.SetMetallic(params.Metallic)
			.SetRoughness(params.Roughness)
			.SetEmissiveColor(params.Emissive)
			.SetEmissiveStrength(params.EmissiveStrength)
			.SetOcclusion(params.Ao)
			.SetNormalScale(params.NormalScale);

		Material* result = Material::Create(material);
		if (!result)
			return nullptr;

		Texture* maps[] = { params.AlbedoMap, params.NormalMap, params.MetallicRoughnessMap, params.OcclusionMap, params.EmissiveMap };
		static_assert(std::size(maps) == Renderer3D::k_PbrTextureSlots, "one map per PBR texture slot");
		for (uint32_t slot = 0; slot < Renderer3D::k_PbrTextureSlots; ++slot)
		{
			if (maps[slot])
				result->SetTexture(slot, maps[slot]);
			if (params.Sampler)
				result->SetSampler(slot, params.Sampler);
		}

		PreparePbrMaterial(result);
		return result;
	}

	void DestroyPbrShaders()
	{
		for (Shader*& shader : s_Shaders)
		{
			if (!shader)
				continue;
			UnwatchUnmanagedShader(shader);
			DestroyAndDelete(shader);
		}
		s_Attempted = {};
		DestroyAndDelete(s_Sampler);
	}

}
