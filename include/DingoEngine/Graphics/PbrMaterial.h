#pragma once

#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/Texture.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{

	/// The texture slots of a material from Renderer3D::CreatePbrMaterial, in binding order.
	enum class PbrTextureSlot : uint32_t
	{
		Albedo,            ///< sRGB-encoded RGBA8; multiplies BaseColor.
		Normal,            ///< Tangent-space normals, green = +v; null is a flat normal.
		MetallicRoughness, ///< glTF layout: G = roughness, B = metallic; multiply Roughness and Metallic.
		Occlusion,         ///< The red channel multiplies the ambient term, scaled by Ao.
		Emissive,          ///< sRGB-encoded RGBA8; multiplies Emissive.
		Count              ///< The number of slots; Renderer3D::k_PbrTextureSlots.
	};

	/// The settings of a metallic-roughness PBR material for Renderer3D::CreatePbrMaterial. A null
	/// map reads as white (a flat normal for Normal), so a material with no maps is just its factors.
	struct PbrMaterialParams
	{
		/// Pipeline state and DebugName; its Shader is set for you. CullMode is honoured (the
		/// default is back-face culling), unlike lit materials. A mirrored mesh (a negative scale
		/// on one axis) flips its winding, so it needs CullMode::None here.
		MaterialParams Material;
		glm::vec4 BaseColor         = { 1.0f, 1.0f, 1.0f, 1.0f }; ///< Linear base colour; alpha is opacity.
		float     Metallic          = 0.0f;                       ///< 0 dielectric to 1 metal.
		float     Roughness         = 0.5f;                       ///< 0 mirror-like to 1 fully rough.
		glm::vec3 Emissive          = { 0.0f, 0.0f, 0.0f };       ///< Linear RGB glow colour.
		float     EmissiveStrength  = 1.0f;                       ///< Multiplies Emissive.
		float     Ao                = 1.0f;                       ///< Occlusion strength: 0 ignores the occlusion map.
		float     NormalScale       = 1.0f;                       ///< Normal-map strength; 0 is flat.

		Dingo::Texture* AlbedoMap            = nullptr; ///< sRGB-encoded RGBA8; null is white.
		Dingo::Texture* NormalMap            = nullptr; ///< Tangent-space, green = +v; null is flat.
		Dingo::Texture* MetallicRoughnessMap = nullptr; ///< G = roughness, B = metallic; null is white.
		Dingo::Texture* OcclusionMap         = nullptr; ///< Occlusion in R; null is white.
		Dingo::Texture* EmissiveMap          = nullptr; ///< sRGB-encoded RGBA8; null is white.
		/// Samples every map; null uses the renderer's PBR sampler.
		Dingo::Sampler* Sampler              = nullptr;

		///@{
		/// Each setter sets the field of the same name and returns *this, for chaining.
		PbrMaterialParams& SetMaterial(const MaterialParams& material)  { Material = material; return *this; }
		PbrMaterialParams& SetBaseColor(const glm::vec4& color)         { BaseColor = color; return *this; }
		PbrMaterialParams& SetMetallic(float metallic)                  { Metallic = metallic; return *this; }
		PbrMaterialParams& SetRoughness(float roughness)                { Roughness = roughness; return *this; }
		PbrMaterialParams& SetEmissive(const glm::vec3& color)          { Emissive = color; return *this; }
		PbrMaterialParams& SetEmissiveStrength(float strength)          { EmissiveStrength = strength; return *this; }
		PbrMaterialParams& SetAo(float ao)                              { Ao = ao; return *this; }
		PbrMaterialParams& SetNormalScale(float scale)                  { NormalScale = scale; return *this; }
		PbrMaterialParams& SetAlbedoMap(Dingo::Texture* texture)        { AlbedoMap = texture; return *this; }
		PbrMaterialParams& SetNormalMap(Dingo::Texture* texture)        { NormalMap = texture; return *this; }
		PbrMaterialParams& SetMetallicRoughnessMap(Dingo::Texture* texture) { MetallicRoughnessMap = texture; return *this; }
		PbrMaterialParams& SetOcclusionMap(Dingo::Texture* texture)     { OcclusionMap = texture; return *this; }
		PbrMaterialParams& SetEmissiveMap(Dingo::Texture* texture)      { EmissiveMap = texture; return *this; }
		PbrMaterialParams& SetSampler(Dingo::Sampler* sampler)          { Sampler = sampler; return *this; }
		///@}

		/// Sets the map of one slot and returns *this.
		PbrMaterialParams& SetMap(PbrTextureSlot slot, Dingo::Texture* texture)
		{
			switch (slot)
			{
			case PbrTextureSlot::Albedo:            AlbedoMap = texture; break;
			case PbrTextureSlot::Normal:            NormalMap = texture; break;
			case PbrTextureSlot::MetallicRoughness: MetallicRoughnessMap = texture; break;
			case PbrTextureSlot::Occlusion:         OcclusionMap = texture; break;
			case PbrTextureSlot::Emissive:          EmissiveMap = texture; break;
			case PbrTextureSlot::Count:             break;
			}
			return *this;
		}
	};

}
