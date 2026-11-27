// The fragment-side scene declarations of Renderer3D_Lit.glsl: include it with
// <DingoEngine/Renderer3DScene.glsl> after a #type fragment line. The varyings match
// Renderer3D_MeshVertex.glsl. The DE_ObjectData buffer sits at DE_OBJECT_DATA_BINDING, 8 unless defined before
// including it.

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_WorldPosition;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) flat in uint v_ObjectIndex;

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_LOCAL_LIGHTS = 32;

struct DirectionalLight
{
	vec4 Direction; // xyz = the way the light travels
	vec4 Color;     // rgb = colour × intensity
};

// A point light is a spot light whose cone factor is always 1: scale 0, offset 1.
struct LocalLight
{
	vec4 PositionRange; // xyz = world position, w = range
	vec4 Color;         // rgb = colour × intensity, w = cone scale
	vec4 SpotDirection; // xyz = the way the cone points, w = cone offset
};

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
	vec4 LightDirection;
	vec4 Ambient;
	vec4 CameraPosition; // w = 1: world position; w = 0: orthographic, xyz = towards the camera
	vec4 AmbientColor;   // rgb = colour × intensity
	ivec4 LightCounts;   // x = directional lights, y = point and spot lights
	DirectionalLight DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
	LocalLight LocalLights[MAX_LOCAL_LIGHTS];
	vec4 FogColor;  // rgb = colour, a = max opacity
	vec4 FogParams; // x = start, y = end, z = density, w = mode: 0 none, 1 linear, 2 exp, 3 exp squared
};

// Mirrors Renderer3D::ObjectRecord.
struct ObjectRecord
{
	vec4 Emissive; // rgb = colour, a = strength
	vec4 Surface;  // x = roughness, y = specular strength, z = metallic (PBR only)
};

#ifndef DE_OBJECT_DATA_BINDING
#define DE_OBJECT_DATA_BINDING 8
#endif

layout(std430, binding = DE_OBJECT_DATA_BINDING) readonly buffer DE_ObjectData
{
	ObjectRecord Objects[];
};

float FogFactor(float distance)
{
	if (FogParams.w < 1.5)
		return clamp((distance - FogParams.x) / (FogParams.y - FogParams.x), 0.0, 1.0);
	float density = FogParams.z * distance;
	return 1.0 - exp(FogParams.w < 2.5 ? -density : -density * density);
}
