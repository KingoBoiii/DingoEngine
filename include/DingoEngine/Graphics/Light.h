#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{

	// Lights for Renderer3D::SubmitLight. Intensity scales Color, and a scene's lights and ambient
	// add up: past 1.0 the frame clips, unless the post chain (PostProcess.h) tone-maps it.
	//
	// Point and spot lights are local lights: their brightness falls off smoothly from Intensity
	// at the light to exactly zero at Range, as (1 - (d / Range)^2)^2, and they share one
	// per-scene budget (Renderer3DCapabilities::MaxLocalLights).

	/// A sun-like light: parallel rays, no position, no range, no falloff.
	struct DirectionalLight
	{
		/// World-space direction the light travels (does not need to be unit length).
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f };
		/// Linear RGB colour; multiplied by Intensity.
		glm::vec3 Color{ 1.0f };
		/// Scales Color; unlike a local light, 0 is still accepted (it adds nothing).
		float Intensity = 1.0f;
		/// The first casting directional light of a scene gets cascaded shadow maps
		/// (Renderer3DParams::Shadows); another one warns once and lights unshadowed.
		/// ShadowStrength is how dark its shadows are, from 0 (none: it renders no cascades and
		/// doesn't count as casting) to 1.
		bool CastShadows = false;
		float ShadowStrength = 1.0f; ///< Shadow darkness, clamped to 0..1; see CastShadows.
		/// The caster groups this light's shadows take: a mesh casts for it only when its own
		/// ShadowGroups (SubmitMesh, MeshRendererComponent) share a bit with this. Leave the
		/// player's group out of the lantern it carries, and the lantern lights the room without
		/// the body's shadow.
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu;
		/// Scales how much this light scatters in volumetric fog (VolumetricFog, FogVolume); 0 keeps
		/// it out of the haze while it still lights surfaces. A negative value counts as 0 and a
		/// non-finite one as 1.
		float VolumetricIntensity = 1.0f;
	};

	/// A light radiating in all directions from a point, fading to zero at Range.
	struct PointLight
	{
		/// World-space position.
		glm::vec3 Position{ 0.0f };
		/// Linear RGB colour; multiplied by Intensity.
		glm::vec3 Color{ 1.0f };
		/// Scales Color; SubmitLight rejects a light whose Intensity is 0 or less.
		float Intensity = 1.0f;
		/// Distance in world units at which the light reaches exactly zero; must be above 0.
		float Range = 10.0f;
		/// Casting point and spot lights take the scene's shadow slots in the order the light
		/// budget ranks them (Renderer3DCapabilities::MaxShadowedLocalLights); one past them lights
		/// unshadowed. A point light takes six tiles of the shadow atlas, a spot light one.
		bool CastShadows = false;
		float ShadowStrength = 1.0f; ///< Shadow darkness, clamped to 0..1.
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu; ///< See DirectionalLight::ShadowCasterGroups.
		float VolumetricIntensity = 1.0f; ///< See DirectionalLight::VolumetricIntensity.
	};

	/// A cone of light from a point, fading to zero at Range and at the outer cone angle.
	struct SpotLight
	{
		/// World-space position.
		glm::vec3 Position{ 0.0f };
		/// World-space direction the cone points (does not need to be unit length).
		glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };
		/// Linear RGB colour; multiplied by Intensity.
		glm::vec3 Color{ 1.0f };
		/// Scales Color; SubmitLight rejects a light whose Intensity is 0 or less.
		float Intensity = 1.0f;
		/// Distance in world units at which the light reaches exactly zero; must be above 0.
		float Range = 10.0f;
		/// Angles from the cone's axis, in degrees: full strength inside InnerConeAngle, fading
		/// to zero at OuterConeAngle (1 to 179).
		float InnerConeAngle = 20.0f;
		float OuterConeAngle = 30.0f; ///< Degrees from the axis where the light reaches zero.
		/// See PointLight. A cone wider than 75 degrees from its axis casts like a point light.
		bool CastShadows = false;
		float ShadowStrength = 1.0f; ///< Shadow darkness, clamped to 0..1.
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu; ///< See DirectionalLight::ShadowCasterGroups.
		float VolumetricIntensity = 1.0f; ///< See DirectionalLight::VolumetricIntensity.
	};

	// Distance fog for Renderer3D::SetFog: the lit shader blends each pixel toward Color by the fog
	// factor at its distance from the camera (radial, in world units) times MaxOpacity. Linear rises
	// from 0 at Start to 1 at End; Exponential is 1 - e^(-Density d) and ExponentialSquared
	// 1 - e^(-(Density d)^2). Only a perspective camera's scenes are fogged.
	/// How fog density grows with distance; see Fog.
	enum class FogMode
	{
		None,
		Linear,
		Exponential,
		ExponentialSquared
	};

	/// Distance fog settings for Renderer3D::SetFog.
	struct Fog
	{
		FogMode Mode = FogMode::Linear; ///< None turns fog off.
		glm::vec3 Color{ 0.5f }; ///< Linear RGB colour the scene fades toward.
		float Start = 10.0f; ///< Linear only: distance in world units where fog begins.
		float End = 50.0f; ///< Linear only: distance in world units where fog is full.
		float Density = 0.05f; ///< Exponential modes only: per world unit.
		float MaxOpacity = 1.0f; ///< Cap on the fog factor, 0 (no fog) to 1 (can fully hide).
	};

	/// Froxel volumetric fog settings for Renderer3D::SetVolumetricFog: a participating medium that
	/// scatters the scene's ambient and directional light toward the camera, with shafts where a
	/// shadow-casting directional light is blocked. Colours are in display space, like Fog: a white
	/// medium under an ambient of A scatters A. It acts only when Renderer3DCapabilities::Volumetrics
	/// is on, and replaces the scene's distance fog.
	struct VolumetricFog
	{
		/// Extinction per world unit at BaseHeight and below, the same unit as Fog::Density in
		/// Exponential mode; must be 0 or more.
		float Density = 0.02f;
		float BaseHeight = 0.0f; ///< World height above which the density falls off.
		/// Per world unit above BaseHeight: density is multiplied by e^(-HeightFalloff h). 0 is a
		/// uniform medium; must be 0 or more.
		float HeightFalloff = 0.0f;
		glm::vec3 Albedo{ 1.0f }; ///< Fraction of the extinguished light that scatters, per channel, clamped to 0..1.
		/// Henyey-Greenstein asymmetry, clamped to -0.95..0.95: above 0 scatters toward where the
		/// light travels, 0 evenly.
		float Anisotropy = 0.2f;
		float AmbientIntensity = 1.0f; ///< Scales the scene's ambient light in the medium; 0 or more.
		float LightIntensity = 1.0f; ///< Scales the directional lights in the medium; 0 or more.
		/// Multiplies the in-scatter of the ambient and directional light per channel, apart from
		/// Albedo and the light's own colour: a haze tinted blue under a violet sun. It never
		/// changes extinction, so transmittance is the same. Finite and 0 or more, and not clamped,
		/// so a value above 1 brightens.
		glm::vec3 ScatterTint{ 1.0f };
		/// How much of the sun reaches a sample that no shadow cascade covers, from 0 (dark, so only
		/// air the shadow map proves lit scatters) to 1 (lit, the default). The shadow of the last
		/// cascade fades to it over the cascade's tail, with the width the surface shadows use
		/// (ShadowSettings::CascadeBlend), so there is no edge at the end of the map. A light that
		/// casts no shadow is unshadowed whatever this is. The light's ShadowStrength scales it: the
		/// outside value is mix(1, OutsideShadowLight, ShadowStrength), so outside air is never darker
		/// than full shadow. Clamped to 0..1.
		float OutsideShadowLight = 1.0f;
		float NearDistance = 0.5f; ///< View depth where the volume starts; above 0.
		float MaxDistance = 64.0f; ///< View depth where the volume ends; surfaces past it take the fog at MaxDistance. Above NearDistance.
		/// Blend each frame with the previous one to smooth the sampling noise. Only the first
		/// temporal scene of a frame uses the history.
		bool Temporal = true;
		/// Scales how much the scene's point and spot lights scatter in the medium; 0 leaves only the
		/// directional lights and the ambient light. A light's own VolumetricIntensity and a
		/// FogVolume's LocalLightIntensity multiply it. Finite and 0 or more.
		float LocalLightIntensity = 1.0f;
		/// The most the in-scatter of one pixel's whole froxel volume may reach, per channel, before
		/// the scene's surface is added: the strongest channel is scaled down to it, keeping the hue.
		/// 0 (the default) is no cap. Finite and 0 or more.
		float MaxInScatter = 0.0f;
	};

	/// The shape of a FogVolume, scaled by its transform.
	enum class FogVolumeShape
	{
		Box,    ///< A box of HalfExtents around the transform's origin.
		Sphere  ///< An ellipsoid with HalfExtents as its radii.
	};

	/// How a FogVolume is drawn.
	enum class FogVolumeMode
	{
		Froxel,   ///< Part of the froxel volumetric fog: costs nothing extra per pixel.
		Raymarch  ///< Marched per pixel at the resolution Renderer3DCapabilities::VolumeRaymarchResolution sets; needs the post-process chain, else it falls back to Froxel with a warning.
	};

	/// A debug view of the volumetric fog, set with Renderer3D::SetVolumetricDebugView.
	enum class VolumetricDebugView
	{
		None,          ///< The normal picture.
		ShadowSamples  ///< Light samples in false colour: lit green, shadowed red, outside the shadow map blue, unshadowed yellow.
	};

	/// A local volume of fog for Renderer3D::SubmitFogVolume: a box or an ellipsoid of participating
	/// medium that adds to the scene's volumetric fog inside it. The shape is HalfExtents scaled by
	/// the submitted transform, so a volume's world size is HalfExtents times the transform's scale.
	/// Colours are in display space, like VolumetricFog.
	struct FogVolume
	{
		FogVolumeShape Shape = FogVolumeShape::Box; ///< Box or ellipsoid.
		FogVolumeMode Mode = FogVolumeMode::Froxel; ///< Froxel (default) or Raymarch.
		/// Half sizes along the volume's local axes, before the transform's scale; each above 0.
		glm::vec3 HalfExtents{ 1.0f };
		/// Extinction per world unit, the unit of VolumetricFog::Density; 0 or more.
		float Density = 0.05f;
		glm::vec3 Albedo{ 1.0f }; ///< As VolumetricFog::Albedo, clamped to 0..1.
		float Anisotropy = 0.2f; ///< As VolumetricFog::Anisotropy, clamped to -0.95..0.95.
		glm::vec3 ScatterTint{ 1.0f }; ///< As VolumetricFog::ScatterTint: finite and 0 or more.
		float OutsideShadowLight = 1.0f; ///< As VolumetricFog::OutsideShadowLight, clamped to 0..1.
		float AmbientIntensity = 1.0f; ///< Scales the ambient light in this volume; 0 or more.
		float LightIntensity = 1.0f; ///< Scales the directional lights in this volume; 0 or more.
		/// Scales the point and spot lights in this volume, on top of VolumetricFog::LocalLightIntensity
		/// and each light's VolumetricIntensity; 0 or more.
		float LocalLightIntensity = 1.0f;
		/// World units over which the density fades to 0 toward the surface of the shape; 0 gives a
		/// hard edge. Finite and 0 or more.
		float EdgeFade = 0.5f;

		/// Height profile, in metres above the volume's bottom along its local up: the density is full
		/// up to HeightFadeStart and falls linearly to 0 at HeightFadeEnd. Off when End is not past Start.
		///@{
		float HeightFadeStart = 0.0f;
		float HeightFadeEnd = 0.0f;
		///@}
		/// A band between HeightBandBottom and HeightBandTop (metres above the volume's bottom) where
		/// the density is multiplied by HeightBandMultiplier, easing in and out over HeightBandSoftness
		/// metres at each edge. Off when Top is not past Bottom or the multiplier is 1. The multiplier
		/// and softness are 0 or more.
		///@{
		float HeightBandBottom = 0.0f;
		float HeightBandTop = 0.0f;
		float HeightBandMultiplier = 1.0f;
		float HeightBandSoftness = 0.5f;
		///@}

		/// Thins the in-scatter (never the density, so transmittance is unchanged) with the distance
		/// from the camera: NearFadeKeep of it at NearFadeStart and closer, all of it from NearFadeEnd,
		/// linear between. Off when End is not past Start. Keep is clamped to 0..1.
		///@{
		float NearFadeStart = 0.0f;
		float NearFadeEnd = 0.0f;
		float NearFadeKeep = 0.0f;
		///@}
		/// The same for far away: all of the in-scatter up to FarFadeStart, FarFadeKeep of it from
		/// FarFadeEnd, linear between. Off when End is not past Start.
		///@{
		float FarFadeStart = 0.0f;
		float FarFadeEnd = 0.0f;
		float FarFadeKeep = 1.0f;
		///@}

		/// The most one pixel's in-scatter of this volume may reach in Raymarch mode, per channel; the
		/// strongest channel is scaled down to it. 0 is no cap. Froxel volumes share
		/// VolumetricFog::MaxInScatter instead. Finite and 0 or more.
		float MaxInScatter = 0.0f;
		/// Raymarch mode only: samples along the ray through the volume, clamped to 1..64.
		uint32_t RaymarchSteps = 16;

		/// Scrolling 3D noise on the density. NoiseScale is the world size of one noise tile (above 0),
		/// NoiseVelocity the drift in world units per second (set the clock with
		/// Renderer3D::SetFogVolumeTime), and the density is multiplied by a value between NoiseMin and
		/// NoiseMax that follows the noise. Off when NoiseMin equals NoiseMax; both 0 or more.
		///@{
		float NoiseScale = 4.0f;
		glm::vec3 NoiseVelocity{ 0.0f };
		float NoiseMin = 1.0f;
		float NoiseMax = 1.0f;
		///@}
	};

	/// The weight Renderer3D's lit shader gives the light at `point`, from 0 to 1, for gameplay
	/// tests such as "is the player inside that cone": falloff^2 for a point light and
	/// falloff^2 * cone^2 for a spot, where falloff = 1 - d^2 / Range^2 (0 from Range on) and the
	/// cone is set up exactly as SubmitLight sets it up. It leaves out the surface's N.L, Color,
	/// Intensity, shadows and the frame's light budget, and is 0 for a light SubmitLight rejects as
	/// unusable.
	///@{
	float GetLightAttenuation(const PointLight& light, const glm::vec3& point);
	float GetLightAttenuation(const SpotLight& light, const glm::vec3& point);
	///@}

}
