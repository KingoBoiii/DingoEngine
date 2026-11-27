// The CameraData uniform block at binding 0, with the light structs it holds: include it with
// <DingoEngine/Renderer3DCamera.glsl> in any stage. The layout mirrors Renderer3D::CameraData.

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
	vec4 EnvironmentParams; // x = 1: lit by the environment, y = intensity, z/w = cos/sin of the lookup's turn (DE_RotateY)
	vec4 VolumeParams; // x = 1: the froxel volume is sampled, y = near, z = 1 / log2(far / near), w = far
	vec4 VolumeGrid;   // xyz = grid size, w = 1 / the first slice's far depth
};
