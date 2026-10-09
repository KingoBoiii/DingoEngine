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

	struct DirectionalLight
	{
		glm::vec3 Direction{ -0.4f, -1.0f, -0.35f }; // the way the light travels
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		// The first casting directional light of a scene gets cascaded shadow maps
		// (Renderer3DParams::Shadows); another one warns once and lights unshadowed. ShadowStrength
		// is how dark its shadows are, from 0 (none: it renders no cascades and doesn't count as
		// casting) to 1.
		bool CastShadows = false;
		float ShadowStrength = 1.0f;
		// The caster groups this light's shadows take: a mesh casts for it only when its own
		// ShadowGroups (SubmitMesh, MeshRendererComponent) share a bit with this. Leave the player's
		// group out of the lantern it carries, and the lantern lights the room without the body's shadow.
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu;
	};

	struct PointLight
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		// Casting point and spot lights take the scene's shadow slots in the order the light budget
		// ranks them (Renderer3DCapabilities::MaxShadowedLocalLights); one past them lights
		// unshadowed. A point light takes six tiles of the shadow atlas, a spot light one.
		bool CastShadows = false;
		float ShadowStrength = 1.0f;
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu; // see DirectionalLight
	};

	struct SpotLight
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Direction{ 0.0f, -1.0f, 0.0f }; // the way the cone points
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		// Angles from the cone's axis, in degrees: full strength inside InnerConeAngle, fading
		// to zero at OuterConeAngle (1 to 179).
		float InnerConeAngle = 20.0f;
		float OuterConeAngle = 30.0f;
		// See PointLight. A cone wider than 75 degrees from its axis casts like a point light.
		bool CastShadows = false;
		float ShadowStrength = 1.0f;
		uint32_t ShadowCasterGroups = 0xFFFFFFFFu; // see DirectionalLight
	};

	// The weight Renderer3D's lit shader gives the light at `point`, from 0 to 1, for gameplay
	// tests such as "is the player inside that cone": falloff^2 for a point light and
	// falloff^2 * cone^2 for a spot, where falloff = 1 - d^2 / Range^2 (0 from Range on) and the
	// cone is set up exactly as SubmitLight sets it up. It leaves out the surface's N.L, Color,
	// Intensity, shadows and the frame's light budget, and is 0 for a light SubmitLight rejects as
	// unusable.
	float GetLightAttenuation(const PointLight& light, const glm::vec3& point);
	float GetLightAttenuation(const SpotLight& light, const glm::vec3& point);

}
