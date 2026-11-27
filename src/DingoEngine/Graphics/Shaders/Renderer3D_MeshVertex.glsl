// The vertex stage of Renderer3D_Lit.glsl, shared with every shader that draws the same meshes: include it
// with <DingoEngine/Renderer3D_MeshVertex.glsl> after a #type vertex line. DE_SKINNED and DE_INSTANCED pick the
// variant.

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) flat out uint v_ObjectIndex;

// Written out under precise rather than as a matrix product, which a driver may fuse differently in two
// programs (and SPIR-V to HLSL drops precise from): every variant then projects a world position to the
// same bits, so an instanced copy covers exactly the CPU batch's pixels.
vec4 ProjectPosition(mat4 viewProjection, vec4 position)
{
	precise vec4 clip = (viewProjection[0] * position.x + viewProjection[1] * position.y) + (viewProjection[2] * position.z + viewProjection[3] * position.w);
	return clip;
}

#ifdef DE_SKINNED

#define DE_SKIN_PALETTE
#include <DingoEngine/Skinning.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in uvec4 a_Joints;
layout(location = 4) in vec4 a_Weights;

void main()
{
	mat4 skin = SkinMatrix(a_Joints, a_Weights);
	vec4 worldPosition = SkinModel() * (skin * vec4(a_Position, 1.0));

	gl_Position     = ProjectPosition(ViewProjection, worldPosition);
	v_Normal        = SkinNormal(skin, a_Normal);
	v_Color         = SkinColor();
	v_WorldPosition = worldPosition.xyz;
	v_TexCoord      = a_TexCoord;
	v_ObjectIndex   = 0u;
}

#elif defined(DE_INSTANCED)

#define DE_INSTANCE_DRAW_PUSH
#include <DingoEngine/Instancing.glsl>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

void main()
{
	InstanceRecord instance = GetInstance(uint(gl_InstanceIndex));
	vec3 worldPosition = InstanceWorldPosition(instance, a_Position);

	gl_Position     = ProjectPosition(ViewProjection, vec4(worldPosition, 1.0));
	v_Normal        = InstanceWorldNormal(instance, a_Normal);
	v_Color         = instance.Color;
	v_WorldPosition = worldPosition;
	v_TexCoord      = a_TexCoord;
	v_ObjectIndex   = instance.Misc.x;
}

#else

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in uint a_ObjectIndex;

void main()
{
	gl_Position     = ProjectPosition(ViewProjection, vec4(a_Position, 1.0));
	v_Normal        = a_Normal;
	v_Color         = a_Color;
	v_WorldPosition = a_Position;
	v_TexCoord      = a_TexCoord;
	v_ObjectIndex   = a_ObjectIndex;
}

#endif
