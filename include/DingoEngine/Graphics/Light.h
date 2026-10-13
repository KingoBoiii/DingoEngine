#pragma once

#include <glm/glm.hpp>

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
