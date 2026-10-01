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
layout(location = 3) in vec2 a_TexCoord;

layout(std140, binding = 0) uniform CameraData
{
	mat4 ViewProjection;
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec3 v_WorldPosition;
layout(location = 3) out vec2 v_TexCoord;

void main()
{
	gl_Position     = ViewProjection * vec4(a_Position, 1.0);
	v_Normal        = a_Normal;
	v_Color         = a_Color;
	v_WorldPosition = a_Position;
	v_TexCoord      = a_TexCoord;
}

#type fragment
#version 450

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_WorldPosition;
layout(location = 3) in vec2 v_TexCoord;

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

// Mirrors Renderer3D::LitMaterialData, written for every lit material each EndScene.
layout(std140, binding = 1) uniform MaterialData
{
	vec4 EmissiveColor; // rgb = colour
	vec4 Surface;       // x = emissive strength, y = roughness, z = specular strength
};

layout(binding = 2) uniform texture2D u_Albedo;
layout(binding = 3) uniform sampler u_AlbedoSampler;

layout(location = 0) out vec4 o_Color;

const float PI = 3.14159265;

// Normalised Blinn-Phong: a rough surface spreads the highlight wide and dim, a smooth one keeps
// it small and bright, for the same light.
float Highlight(vec3 normal, vec3 toLight, vec3 toCamera, float shininess)
{
	vec3 halfway = toLight + toCamera;
	float lengthSquared = dot(halfway, halfway);
	if (lengthSquared < 1e-8)
		return 0.0;

	float nDotH = max(dot(normal, halfway * inversesqrt(lengthSquared)), 0.0);
	return (shininess + 8.0) / (8.0 * PI) * pow(nDotH, shininess);
}

void main()
{
	vec3 normal = normalize(v_Normal);
	vec3 albedo = v_Color.rgb * texture(sampler2D(u_Albedo, u_AlbedoSampler), v_TexCoord).rgb;

	// Skipped entirely without specular, so non-shiny materials compute exactly what they did
	// before the term existed.
	bool shiny = Surface.z > 0.0;
	float shininess = exp2(10.0 * (1.0 - Surface.y) + 1.0);
	vec3 toCamera = CameraPosition.w > 0.5 ? normalize(CameraPosition.xyz - v_WorldPosition) : CameraPosition.xyz;

	vec3 lighting = AmbientColor.rgb;
	vec3 specular = vec3(0.0);

	int directionalCount = min(LightCounts.x, MAX_DIRECTIONAL_LIGHTS);
	for (int i = 0; i < directionalCount; ++i)
	{
		vec3 toLight = normalize(-DirectionalLights[i].Direction.xyz);
		float nDotL = max(dot(normal, toLight), 0.0);
		lighting += DirectionalLights[i].Color.rgb * nDotL;
		if (shiny && nDotL > 0.0)
			specular += DirectionalLights[i].Color.rgb * nDotL * Highlight(normal, toLight, toCamera, shininess);
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
		float nDotL = max(dot(normal, toLight), 0.0);
		lighting += light.Color.rgb * nDotL * (falloff * falloff) * (cone * cone);
		if (shiny && nDotL > 0.0)
			specular += light.Color.rgb * nDotL * (falloff * falloff) * (cone * cone) * Highlight(normal, toLight, toCamera, shininess);
	}

	vec3 finalColor = albedo * lighting + specular * Surface.z;
	finalColor += EmissiveColor.rgb * Surface.x;
	// Lit draws are unsorted and write depth, so only the mesh colour, never an albedo map, makes
	// them see-through.
	o_Color = vec4(finalColor, v_Color.a);
}
