// The equirectangular panorama convention of Environment and the skybox: include it with
// <DingoEngine/Environment.glsl>. Written in the GLSL subset that also compiles as C++ over GLM (float
// literals carry the f suffix for that), so EnvironmentTests.cpp checks this text.
//
// u = 0.5 looks down -Z and u = 0.75 down +X. v runs along the texture's rows: v = 1 is +Y, the top of
// the picture, because Texture::CreateFromHdrFile and Environment flip the file's rows on load.

#ifndef DE_ENVIRONMENT_GLSL
#define DE_ENVIRONMENT_GLSL

#ifndef DE_PI
#define DE_PI 3.14159265358979f
#endif

// dir must be normalised.
vec2 DE_EquirectUv(vec3 dir)
{
	float u = atan(dir.x, -dir.z) / (2.0f * DE_PI) + 0.5f;
	float v = asin(clamp(dir.y, -1.0f, 1.0f)) / DE_PI + 0.5f;
	return vec2(u, v);
}

vec3 DE_EquirectDirection(vec2 uv)
{
	float phi = (uv.x - 0.5f) * 2.0f * DE_PI;
	float theta = (uv.y - 0.5f) * DE_PI;
	float c = cos(theta);
	return vec3(c * sin(phi), sin(theta), -c * cos(phi));
}

// Turns dir about +Y by the angle whose cosine and sine are given, right-handed: +X goes toward -Z.
// A sky turned by R degrees is looked up with the angle -R.
vec3 DE_RotateY(vec3 dir, float cosTheta, float sinTheta)
{
	return vec3(cosTheta * dir.x + sinTheta * dir.z, dir.y, -sinTheta * dir.x + cosTheta * dir.z);
}

#endif
