// Renderer3D's shadow-atlas pass: depth only, every shadow view of a scene (the cascades, then each
// shadowed local light's one or six) in one instanced draw per batch. Instance i takes view i: its
// matrix, orthographic or perspective, then a move into its tile of the atlas, with clip distances at
// the tile's edges so nothing spills into a neighbour. Static vertices are in world
// space already; with DE_SKINNED they are skinned like the lit shader's (Skinning.glsl). Mirrors
// Renderer3D::ShadowViews.

#type vertex
#version 450

const int MAX_SHADOW_VIEWS = 100;

struct ShadowView
{
	mat4 ViewProjection;
	vec4 Tile; // xy = the tile's centre in atlas clip space, zw = its scale
};

layout(std140, binding = 0) uniform ShadowViews
{
	ShadowView Views[MAX_SHADOW_VIEWS];
};

layout(location = 0) in vec3 a_Position;

#ifdef DE_SKINNED
#include <DingoEngine/Skinning.glsl>
layout(location = 1) in uvec4 a_Joints;
layout(location = 2) in vec4 a_Weights;
#endif

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_ClipDistance[4];
};

void main()
{
#ifdef DE_SKINNED
	vec4 worldPosition = Model * (SkinMatrix(a_Joints, a_Weights) * vec4(a_Position, 1.0));
#else
	vec4 worldPosition = vec4(a_Position, 1.0);
#endif

	ShadowView view = Views[gl_InstanceIndex];
	vec4 clip = view.ViewProjection * worldPosition;

	gl_ClipDistance[0] = clip.w + clip.x;
	gl_ClipDistance[1] = clip.w - clip.x;
	gl_ClipDistance[2] = clip.w + clip.y;
	gl_ClipDistance[3] = clip.w - clip.y;
	gl_Position = vec4(clip.xy * view.Tile.zw + view.Tile.xy * clip.w, clip.z, clip.w);
}
