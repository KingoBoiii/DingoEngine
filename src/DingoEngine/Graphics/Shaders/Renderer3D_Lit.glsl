// Vertices arrive in world space (Renderer3D transforms them on the CPU while batching), so the
// vertex stage only applies the camera.
//
// CameraData mirrors Renderer3D::CameraData. Its first three members are a frozen prefix that
// custom material shaders declare on their own, so members are only ever appended.

#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec3 v_WorldPosition;

void main()
{
	gl_Position     = ViewProjection * vec4(a_Position, 1.0);
	v_Normal        = a_Normal;
	v_Color         = a_Color;
	v_WorldPosition = a_Position;
}

#type fragment
#version 450

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_WorldPosition;

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
};

// Material params (binding 1, following the scene UBO at binding 0). Only the built-in
// default material binds this — custom materials supply their own uniforms/layout.
layout(std140, binding = 1) uniform MaterialData
{
	vec4 EmissiveColor;    // rgb = color, a unused (padding)
	vec4 EmissiveStrength; // x = strength, yzw unused (padding)
};

layout(location = 0) out vec4 o_Color;

void main()
{
	vec3 normal = normalize(v_Normal);

	vec3 lighting = AmbientColor.rgb;
	int directionalCount = min(LightCounts.x, MAX_DIRECTIONAL_LIGHTS);
	for (int i = 0; i < directionalCount; ++i)
	{
		vec3 toLight = normalize(-DirectionalLights[i].Direction.xyz);
		lighting += DirectionalLights[i].Color.rgb * max(dot(normal, toLight), 0.0);
	}

	int localCount = min(LightCounts.y, MAX_LOCAL_LIGHTS);
	for (int i = 0; i < localCount; ++i)
	{
		LocalLight light = LocalLights[i];
		vec3 toLight = light.PositionRange.xyz - v_WorldPosition;
		float distanceSquared = dot(toLight, toLight);
		float rangeSquared = light.PositionRange.w * light.PositionRange.w;
		if (!(distanceSquared < rangeSquared))
			continue;

		toLight *= inversesqrt(max(distanceSquared, 1e-8));
		float falloff = 1.0 - distanceSquared / rangeSquared;
		float cone = clamp(dot(-toLight, light.SpotDirection.xyz) * light.Color.w + light.SpotDirection.w, 0.0, 1.0);
		lighting += light.Color.rgb * max(dot(normal, toLight), 0.0) * (falloff * falloff) * (cone * cone);
	}

	vec3 finalColor = v_Color.rgb * lighting;
	finalColor += EmissiveColor.rgb * EmissiveStrength.x;
	o_Color = vec4(finalColor, v_Color.a);
}
