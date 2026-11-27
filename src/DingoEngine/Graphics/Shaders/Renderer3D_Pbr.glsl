// The metallic-roughness PBR twin of Renderer3D_Lit.glsl: the same vertex stage, camera, lights, shadows,
// fog and object records, lit with Cook-Torrance (Brdf.glsl). Material, ambient and light colours are
// decoded from sRGB, lit linearly and encoded again before fog, so the tone map and bloom still see
// display-space values. Resources
// sit where the five material slots (bindings 2 to 11) leave room: ShadowData 12, its sampler 13, the
// environment sampler 14, the irradiance coefficients 15, the atlas 16, DE_ObjectData 17, DE_InstanceData 18,
// DE_SkinPalette 19, the environment's specular cube 20, BRDF table 21 and the froxel fog volume 22 (read with
// the environment sampler); 23 up is reserved.
//
// PbrMaterialData mirrors Internal::PbrMaterialData (PbrShaders.h).

#type vertex
#version 450

#define DE_INSTANCE_DATA_BINDING 18
#define DE_SKIN_PALETTE_BINDING 19
#include <DingoEngine/Renderer3D_MeshVertex.glsl>

#type fragment
#version 450

#define DE_SHADOW_DATA_BINDING 12
#define DE_SHADOW_SAMPLER_BINDING 13
#define DE_SHADOW_ATLAS_BINDING 16
#define DE_OBJECT_DATA_BINDING 17
#include <DingoEngine/Renderer3DScene.glsl>
#include <DingoEngine/Shadows.glsl>
#include <DingoEngine/Brdf.glsl>
#include <DingoEngine/Environment.glsl>
#include <DingoEngine/Ibl.glsl>

layout(std140, binding = 1) uniform MaterialData
{
	vec4 PbrBaseColor;
	vec4 PbrEmissive; // rgb = colour, a = strength
	vec4 PbrSurface;  // x = metallic, y = roughness, z = occlusion strength, w = normal scale
	ivec4 PbrFlags;   // x = fog target: 0 FogColor, 1 black, 2 white; y = 1 translucent; z = 1 has a normal map
};

layout(binding = 2) uniform texture2D u_Albedo;
layout(binding = 3) uniform sampler u_AlbedoSampler;
layout(binding = 4) uniform texture2D u_Normal;
layout(binding = 5) uniform sampler u_NormalSampler;
layout(binding = 6) uniform texture2D u_MetallicRoughness;
layout(binding = 7) uniform sampler u_MetallicRoughnessSampler;
layout(binding = 8) uniform texture2D u_Occlusion;
layout(binding = 9) uniform sampler u_OcclusionSampler;
layout(binding = 10) uniform texture2D u_Emissive;
layout(binding = 11) uniform sampler u_EmissiveSampler;

layout(binding = 14) uniform sampler u_EnvironmentSampler;
layout(std430, binding = 15) readonly buffer DE_EnvironmentSh
{
	vec4 Sh[9];
};
layout(binding = 20) uniform textureCube u_EnvironmentSpecular;
layout(binding = 21) uniform texture2D u_EnvironmentBrdf;

#define DE_VOLUMETRIC_FOG_BINDING 22
#define DE_VOLUMETRIC_FOG_SAMPLER u_EnvironmentSampler
#include <DingoEngine/Volumetrics.glsl>

layout(location = 0) out vec4 o_Color;

// One light's Cook-Torrance and Lambert terms, times its irradiance (cosine included, colour not).
vec3 PbrDirect(vec3 normal, vec3 toCamera, vec3 toLight, float NoV, vec3 diffuseColor, vec3 F0, float a)
{
	float NoL = clamp(dot(normal, toLight), 0.0, 1.0);
	vec3 halfway = toLight + toCamera;
	halfway *= inversesqrt(max(dot(halfway, halfway), 1e-8));
	float NoH = clamp(dot(normal, halfway), 0.0, 1.0);
	float VoH = clamp(dot(toCamera, halfway), 0.0, 1.0);

	vec3 F = F_Schlick(F0, VoH);
	vec3 specular = F * (D_GGX(NoH, a) * V_SmithGGXCorrelated(NoV, NoL, a));
	vec3 diffuse = (vec3(1.0) - F) * diffuseColor * (1.0 / DE_PI);
	return (diffuse + specular) * NoL;
}

// The tangent-space normal in `mapped` (a sampled texel) in world space, from the screen-space
// derivatives of position and UV: no tangent attribute. The determinant's sign, not its value, orients
// the frame, so mirrored UVs and the Vulkan/D3D flip of screen y (which negates both derivative pairs)
// leave it unchanged. B keeps its own direction rather than being rebuilt from N x T. Without usable
// UVs (all equal, or none) the geometric normal stands. Derivatives are taken unconditionally.
vec3 PerturbNormal(vec3 geometricNormal, vec3 position, vec2 uv, vec3 mapped, float scale, bool enabled)
{
	vec3 dp1 = dFdx(position);
	vec3 dp2 = dFdy(position);
	vec2 duv1 = dFdx(uv);
	vec2 duv2 = dFdy(uv);

	float det = duv1.x * duv2.y - duv1.y * duv2.x;
	float uvScale = length(duv1) * length(duv2);
	float handedness = sign(det);
	vec3 T = (dp1 * duv2.y - dp2 * duv1.y) * handedness;
	vec3 B = (dp2 * duv1.x - dp1 * duv2.x) * handedness;
	T -= geometricNormal * dot(geometricNormal, T);
	B -= geometricNormal * dot(geometricNormal, B);

	float tt = dot(T, T);
	float bb = dot(B, B);
	if (!enabled || !(abs(det) > 1e-6 * uvScale) || !(tt > 0.0) || !(bb > 0.0))
		return geometricNormal;

	vec3 n = mapped * 2.0 - 1.0;
	n.xy *= scale;
	return normalize(T * (inversesqrt(tt) * n.x) + B * (inversesqrt(bb) * n.y) + geometricNormal * n.z);
}

// The ambient light on the surface: the environment's irradiance and reflections when the scene has one
// (EnvironmentParams.x), else the flat ambient colour. Every fetch is taken before the final select, which
// keeps the flat path exactly as it was. Radiance is linear and enters undecoded; EnvironmentParams.y scales
// it before the shared encode. The horizon term fades reflections that point below the geometric surface.
vec3 PbrAmbient(vec3 diffuseColor, vec3 F0, float roughness, float NoV, float occlusion, vec3 normal, vec3 geometricNormal, vec3 toCamera)
{
	vec3 flatAmbient = SrgbToLinear(AmbientColor.rgb) * (diffuseColor + EnvBRDFApprox(F0, roughness, NoV)) * occlusion;

	vec3 reflected = reflect(-toCamera, normal);
	vec3 lookupNormal = DE_RotateY(normal, EnvironmentParams.z, EnvironmentParams.w);
	vec3 lookupReflected = DE_RotateY(reflected, EnvironmentParams.z, EnvironmentParams.w);

	vec3 irradiance = vec3(0.0);
	for (int i = 0; i < 9; ++i)
		irradiance += Sh[i].rgb * DE_ShBasis(i, lookupNormal);
	irradiance = max(irradiance, vec3(0.0));

	vec3 prefiltered = textureLod(samplerCube(u_EnvironmentSpecular, u_EnvironmentSampler), lookupReflected, DE_PrefilterLod(roughness)).rgb;
	vec2 dfg = textureLod(sampler2D(u_EnvironmentBrdf, u_EnvironmentSampler), DE_BrdfLutUv(NoV, roughness), 0.0).rg;

	float specularOcclusion = DE_SpecularOcclusion(NoV, occlusion, roughness * roughness);
	float horizon = min(1.0 + dot(reflected, geometricNormal), 1.0);
	horizon *= horizon;
	vec3 ibl = EnvironmentParams.y * (diffuseColor * irradiance * occlusion + prefiltered * (F0 * dfg.x + vec3(dfg.y)) * (specularOcclusion * horizon));

	return EnvironmentParams.x > 0.5 ? ibl : flatAmbient;
}

// Fog as the lit shader applies it: the distance fog, or the froxel volume when the scene has one.
vec3 ApplySceneFog(vec3 color)
{
	if (FogParams.w > 0.5 && CameraPosition.w > 0.5)
	{
		vec3 fogTarget = PbrFlags.x == 0 ? FogColor.rgb : vec3(PbrFlags.x == 1 ? 0.0 : 1.0);
		color = mix(color, fogTarget, FogFactor(distance(CameraPosition.xyz, v_WorldPosition)) * FogColor.a);
	}
	if (VolumeParams.x > 0.5)
	{
		vec4 volume = DE_SampleVolumetricFog(ViewProjection, VolumeParams, VolumeGrid, v_WorldPosition);
		color = DE_ApplyVolumetricFog(color, volume, PbrFlags.x);
	}
	return color;
}

void main()
{
	vec4 albedoSample = texture(sampler2D(u_Albedo, u_AlbedoSampler), v_TexCoord);
	vec4 normalSample = texture(sampler2D(u_Normal, u_NormalSampler), v_TexCoord);
	vec4 metallicRoughnessSample = texture(sampler2D(u_MetallicRoughness, u_MetallicRoughnessSampler), v_TexCoord);
	vec4 occlusionSample = texture(sampler2D(u_Occlusion, u_OcclusionSampler), v_TexCoord);
	vec4 emissiveSample = texture(sampler2D(u_Emissive, u_EmissiveSampler), v_TexCoord);

	vec3 emissiveColor = PbrEmissive.rgb;
	float emissiveStrength = PbrEmissive.a;
	float roughnessFactor = PbrSurface.y;
	float metallicFactor = PbrSurface.x;
	if (v_ObjectIndex != 0u)
	{
		ObjectRecord object = Objects[v_ObjectIndex];
		emissiveColor = object.Emissive.rgb;
		emissiveStrength = object.Emissive.a;
		roughnessFactor = object.Surface.x;
		metallicFactor = object.Surface.z;
	}

	vec3 baseColor = SrgbToLinear(albedoSample.rgb) * PbrBaseColor.rgb * SrgbToLinear(v_Color.rgb);
	float roughness = clamp(roughnessFactor * metallicRoughnessSample.g, 0.045, 1.0);
	float metallic = clamp(metallicFactor * metallicRoughnessSample.b, 0.0, 1.0);
	float occlusion = 1.0 + PbrSurface.z * (occlusionSample.r - 1.0);
	vec3 emissive = SrgbToLinear(emissiveSample.rgb) * emissiveColor * emissiveStrength;

	vec3 geometricNormal = normalize(v_Normal);
	vec3 normal = PerturbNormal(geometricNormal, v_WorldPosition, v_TexCoord, normalSample.xyz, PbrSurface.w, PbrFlags.z != 0);

	vec3 diffuseColor = baseColor * (1.0 - metallic);
	vec3 F0 = mix(vec3(0.04), baseColor, metallic);
	float a = roughness * roughness;
	vec3 toCamera = CameraPosition.w > 0.5 ? normalize(CameraPosition.xyz - v_WorldPosition) : CameraPosition.xyz;
	float NoV = clamp(dot(normal, toCamera), 1e-4, 1.0);

	vec3 lighting = PbrAmbient(diffuseColor, F0, roughness, NoV, occlusion, normal, geometricNormal, toCamera);

	int directionalCount = min(LightCounts.x, MAX_DIRECTIONAL_LIGHTS);
	for (int i = 0; i < directionalCount; ++i)
	{
		vec3 toLight = normalize(-DirectionalLights[i].Direction.xyz);
		float shadow = 1.0;
		if (i == ShadowCounts.y && dot(normal, toLight) > 0.0)
			shadow = DirectionalShadow(v_WorldPosition, geometricNormal);
		lighting += SrgbToLinear(DirectionalLights[i].Color.rgb) * DE_PI * shadow * PbrDirect(normal, toCamera, toLight, NoV, diffuseColor, F0, a);
	}

	int localCount = min(LightCounts.y, MAX_LOCAL_LIGHTS);
	for (int i = 0; i < localCount; ++i)
	{
		LocalLight light = LocalLights[i];
		vec3 toLight = light.PositionRange.xyz - v_WorldPosition;
		float distanceSquared = dot(toLight, toLight);
		float rangeSquared = light.PositionRange.w * light.PositionRange.w;
		if (!(distanceSquared < rangeSquared))
			continue;

		toLight *= inversesqrt(max(distanceSquared, 1e-8));
		float falloff = 1.0 - distanceSquared / rangeSquared;
		float cone = clamp(dot(-toLight, light.SpotDirection.xyz) * light.Color.w + light.SpotDirection.w, 0.0, 1.0);
		float shadow = 1.0;
		if (ShadowCounts.w > 0 && dot(normal, toLight) > 0.0 && cone > 0.0)
			shadow = LocalLightShadow(i, v_WorldPosition, geometricNormal);
		lighting += SrgbToLinear(light.Color.rgb) * DE_PI * (shadow * falloff * falloff * cone * cone) * PbrDirect(normal, toCamera, toLight, NoV, diffuseColor, F0, a);
	}

	vec3 finalColor = LinearToSrgb(lighting + emissive);
	if (ShadowCounts.z != 0)
		finalColor *= ShadowCascadeTint(v_WorldPosition);
	finalColor = ApplySceneFog(finalColor);

	float alpha = PbrFlags.y != 0 ? v_Color.a * PbrBaseColor.a * albedoSample.a : v_Color.a;
	o_Color = vec4(finalColor, alpha);
}
