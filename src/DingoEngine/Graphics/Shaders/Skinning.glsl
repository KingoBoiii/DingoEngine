// GPU skinning for a vertex stage: an instance's placement and joint palette, and the pose of one
// vertex. Read them through SkinModel, SkinNormalMatrix, SkinColor and SkinMatrix.
//
// With DE_SKIN_PALETTE (the engine's shaders) every instance of the scene sits in one storage arena,
// DE_SkinPalette, mirrored by Renderer3D's palette records: from the vec4 a draw's DE_SkinDraw names,
// Model (4), the normal matrix's columns (3), Color, then four per joint. Without it the instance is
// the SkinData uniform block, mirrored by Renderer3D::SkinData, uploaded per draw. Both blocks are
// found by name, so a custom skinned shader may put them at bindings its own uniforms and textures
// leave free, up to 13 for the uniform block: define DE_SKIN_DATA_BINDING (and
// DE_SKIN_PALETTE_BINDING) before including this.

#ifndef DE_SKIN_DATA_BINDING
#define DE_SKIN_DATA_BINDING 4
#endif

const int MAX_JOINTS = 128;

#ifdef DE_SKIN_PALETTE

#ifndef DE_SKIN_PALETTE_BINDING
#define DE_SKIN_PALETTE_BINDING 11
#endif

layout(std430, binding = DE_SKIN_PALETTE_BINDING) readonly buffer DE_SkinPalette
{
	vec4 SkinPalette[];
};

layout(std140, binding = DE_SKIN_DATA_BINDING) uniform DE_SkinDraw
{
	uvec4 SkinDraw; // x = the instance's first vec4 in DE_SkinPalette
};

mat4 PaletteMatrix(uint first)
{
	return mat4(SkinPalette[first], SkinPalette[first + 1u], SkinPalette[first + 2u], SkinPalette[first + 3u]);
}

mat4 SkinModel()        { return PaletteMatrix(SkinDraw.x); }
mat3 SkinNormalMatrix() { return mat3(SkinPalette[SkinDraw.x + 4u].xyz, SkinPalette[SkinDraw.x + 5u].xyz, SkinPalette[SkinDraw.x + 6u].xyz); }
vec4 SkinColor()        { return SkinPalette[SkinDraw.x + 7u]; }
mat4 SkinJoint(uint joint) { return PaletteMatrix(SkinDraw.x + 8u + joint * 4u); }

#else

layout(std140, binding = DE_SKIN_DATA_BINDING) uniform SkinData
{
	mat4 Model;
	mat4 NormalMatrix;
	vec4 Color;
	mat4 Joints[MAX_JOINTS];
};

mat4 SkinModel()        { return Model; }
mat3 SkinNormalMatrix() { return mat3(NormalMatrix); }
vec4 SkinColor()        { return Color; }
mat4 SkinJoint(uint joint) { return Joints[joint]; }

#endif

mat4 SkinMatrix(uvec4 joints, vec4 weights)
{
	return weights.x * SkinJoint(joints.x) + weights.y * SkinJoint(joints.y)
	     + weights.z * SkinJoint(joints.z) + weights.w * SkinJoint(joints.w);
}

// The cofactor matrix is the inverse transpose scaled by the determinant, so a squashed or stretched
// joint keeps normals perpendicular without inverting a matrix per vertex. The model loader poses
// rest normals the same way.
vec3 SkinNormal(mat4 skin, vec3 normal)
{
	mat3 m = mat3(skin);
	mat3 cofactor = mat3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1]));
	float handedness = dot(m[0], cofactor[0]) < 0.0 ? -1.0 : 1.0;
	return SkinNormalMatrix() * (cofactor * normal * handedness);
}
