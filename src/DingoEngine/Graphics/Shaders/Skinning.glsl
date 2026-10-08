// GPU skinning for a vertex stage: the joint palette Renderer3D uploads per skinned instance, and the
// pose of one vertex. Mirrors Renderer3D::SkinData. Found by its name, so a custom skinned shader may
// put the block at any binding the material's own uniforms and textures leave free, up to 13: define
// DE_SKIN_DATA_BINDING before including this.

#ifndef DE_SKIN_DATA_BINDING
#define DE_SKIN_DATA_BINDING 4
#endif

const int MAX_JOINTS = 128;

layout(std140, binding = DE_SKIN_DATA_BINDING) uniform SkinData
{
	mat4 Model;
	mat4 NormalMatrix;
	vec4 Color;
	mat4 Joints[MAX_JOINTS];
};

mat4 SkinMatrix(uvec4 joints, vec4 weights)
{
	return weights.x * Joints[joints.x] + weights.y * Joints[joints.y]
	     + weights.z * Joints[joints.z] + weights.w * Joints[joints.w];
}

// The cofactor matrix is the inverse transpose scaled by the determinant, so a squashed or stretched
// joint keeps normals perpendicular without inverting a matrix per vertex. The model loader poses
// rest normals the same way.
vec3 SkinNormal(mat4 skin, vec3 normal)
{
	mat3 m = mat3(skin);
	mat3 cofactor = mat3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1]));
	float handedness = dot(m[0], cofactor[0]) < 0.0 ? -1.0 : 1.0;
	return mat3(NormalMatrix) * (cofactor * normal * handedness);
}
