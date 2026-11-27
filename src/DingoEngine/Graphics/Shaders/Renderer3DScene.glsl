// The fragment-side scene declarations of Renderer3D_Lit.glsl: include it with
// <DingoEngine/Renderer3DScene.glsl> after a #type fragment line. The varyings match
// Renderer3D_MeshVertex.glsl. The DE_ObjectData buffer sits at DE_OBJECT_DATA_BINDING, 8 unless defined before
// including it.

#include <DingoEngine/Renderer3DCamera.glsl>

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_WorldPosition;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) flat in uint v_ObjectIndex;

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
