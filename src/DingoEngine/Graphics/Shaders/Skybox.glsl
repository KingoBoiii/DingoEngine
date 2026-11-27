// Renderer3D's sky: one fullscreen triangle at the far plane (z = w = 1) that looks the environment's cube
// up along each pixel's view ray. Mirrors Dingo::Renderer3D::SkyboxData.

#type vertex
#version 450

layout(location = 0) out vec2 v_Ndc;

void main()
{
	vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vec2 ndc = vec2(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0);
	v_Ndc = ndc;
	gl_Position = vec4(ndc, 1.0, 1.0);
}

#type fragment
#version 450
#include <DingoEngine/Brdf.glsl>
#include <DingoEngine/Environment.glsl>

layout(location = 0) in vec2 v_Ndc;

layout(std140, binding = 0) uniform SkyboxData
{
	mat4 InverseViewProjection;
	vec4 Params;   // x = cos and y = sin of the angle the lookup is turned by, z = linear intensity
	vec4 Volume;   // x = 1: the froxel volume fogs the sky, y = the texture coordinate of its last slice
	vec4 Sun;      // xyz = direction toward the sun, w = cosine of the disc's radius (0 = no disc)
	vec4 SunDisc;  // rgb = linear disc radiance, w = the horizon's y: the disc is clipped below it
};

layout(binding = 1) uniform textureCube u_Cube;
layout(binding = 2) uniform sampler u_Sampler;

#define DE_VOLUMETRIC_FOG_BINDING 3
#define DE_VOLUMETRIC_FOG_SAMPLER u_Sampler
#include <DingoEngine/Volumetrics.glsl>

layout(location = 0) out vec4 o_Color;

vec3 UnprojectPoint(float depth)
{
	vec4 p = InverseViewProjection * vec4(v_Ndc, depth, 1.0);
	return p.xyz / p.w;
}

vec3 ApplySkyFog(vec3 color, vec3 dir)
{
	if (Volume.x > 0.5)
	{
		vec2 uv = vec2(v_Ndc.x * 0.5 + 0.5, 0.5 - v_Ndc.y * 0.5);
		vec4 volume = textureLod(sampler3D(u_VolumetricFog, DE_VOLUMETRIC_FOG_SAMPLER), vec3(uv, Volume.y), 0.0);
		return DE_ApplyVolumetricFog(color, volume, 0);
	}
	return color;
}

void main()
{
	vec3 dir = normalize(UnprojectPoint(0.5) - UnprojectPoint(0.0));
	dir = DE_RotateY(dir, Params.x, Params.y);
	float sunCos = dot(dir, Sun.xyz);
	float sunPixel = max(fwidth(sunCos), 1e-7);
	float horizonPixel = max(fwidth(dir.y), 1e-7);
	vec3 radiance = texture(samplerCube(u_Cube, u_Sampler), dir).rgb * Params.z;
	if (Sun.w > 0.0)
	{
		float sinRadius2 = max(1.0 - Sun.w * Sun.w, 1e-12);
		float limbRatio2 = (1.0 - sunCos * sunCos) / sinRadius2;
		vec3 limb = pow(vec3(sqrt(max(1.0 - limbRatio2, 0.0))), vec3(0.397, 0.503, 0.652));
		float coverage = clamp((sunCos - Sun.w) / sunPixel + 0.5, 0.0, 1.0);
		float aboveHorizon = smoothstep(-horizonPixel, horizonPixel, dir.y - SunDisc.w);
		radiance += SunDisc.rgb * limb * (coverage * aboveHorizon);
	}
	o_Color = vec4(ApplySkyFog(LinearToSrgb(radiance), dir), 1.0);
}
