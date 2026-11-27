// DE_CubeFaceDirection: the direction through texel `uv` of cube face `face`, for a compute shader
// that fills or reads a cube face by face. Faces are +X, -X, +Y, -Y, +Z, -Z = 0..5; uv is in [0, 1]
// with row 0 at the top of the face, as in the D3D and Vulkan cube layout. The result is normalised.

#ifndef DE_CUBEMAP_GLSL
#define DE_CUBEMAP_GLSL

vec3 DE_CubeFaceDirection(uint face, vec2 uv)
{
	float a = uv.x * 2.0f - 1.0f;
	float b = uv.y * 2.0f - 1.0f;
	vec3 dir;
	switch (face)
	{
		case 0u: dir = vec3(1.0f, -b, -a); break;
		case 1u: dir = vec3(-1.0f, -b, a); break;
		case 2u: dir = vec3(a, 1.0f, b); break;
		case 3u: dir = vec3(a, -1.0f, -b); break;
		case 4u: dir = vec3(a, -b, 1.0f); break;
		default: dir = vec3(-a, -b, -1.0f); break;
	}
	return normalize(dir);
}

#endif
